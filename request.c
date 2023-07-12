#include "io_helper.h"
#include "request.h"

#define MAXBUF (8192)
#define FIFO 0
#define SFF 1

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_is_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_is_empty = PTHREAD_COND_INITIALIZER;
//
//	TODO: add code to create and manage the buffer
typedef struct _node
{
  int conn_fd;
  int file_size;
  char *filename;
  struct _node *next;
} node;

typedef struct _queue
{
  node *last;
  int size;
} queue;

queue q = {NULL, 0};

// queue funtion
int isEmpty() { return q.size == 0; }
int isFull() { return q.size == buffer_max_size; }
int get_size() { return q.size; }
void push(int conn_fd, int file_size, char *filename)
{
  assert(isFull() != 1);

  // getting new node;
  node *temp = (node *)malloc(sizeof(node));
  temp->conn_fd = conn_fd;
  temp->file_size = file_size;
  temp->filename = strdup(filename);
  temp->next = NULL;

  // insert node
  if (q.last == NULL)
  {
    q.last = temp;
    q.last->next = q.last;
  }
  else
  {
    node *first = q.last->next;
    temp->next = first;
    q.last->next = temp;
    q.last = temp;
  }
  q.size++;
  buffer_size++;
}

void pop()
{
  assert(isEmpty() != 1);

  node *temp = q.last->next;

  if (q.last->next == q.last)
    q.last = NULL;
  else
  {
    q.last->next = temp->next;
  }
  free(temp);
  q.size--;
  buffer_size--;
}

int front_get_fd() { return q.last->next->conn_fd; }
int front_get_fsz() { return q.last->next->file_size; }
char *front_get_fnm() { return q.last->next->filename; }
//---------------------------------------------------------------------------//

// insert node on list q.last in order of file size

void insert_into_SFF(int conn_fd, int file_size, char *filename)
{

  assert(buffer_size != buffer_max_size);
  node *temp = (node *)malloc(sizeof(node));
  temp->conn_fd = conn_fd;
  temp->file_size = file_size;
  temp->filename = strdup(filename);
  temp->next = NULL;

  if (q.last == NULL)
  {
    q.last = temp;
    q.last->next = q.last;
  }
  else
  {
    node *first = q.last->next;
    node *prev = q.last;
    node *curr = first;
    while (curr != first)
    {
      if (curr->file_size > file_size)
      {
        break;
      }
      prev = curr;
      curr = curr->next;
    }
    if (curr == first)
    {
      temp->next = first;
      q.last->next = temp;
    }
    else
    {
      temp->next = curr;
      prev->next = temp;
    }
  }
  q.size++;
  buffer_size++;
}

void delete_from_SFF()
{

  assert(buffer_size != 0);
  node *temp = q.last->next;
  if (q.last->next == q.last)
  {
    q.last = NULL;
  }
  else
  {
    q.last->next = temp->next;
  }
  free(temp);
  q.size--;
  buffer_size--;
}

int get_fd_from_SFF()
{
  return q.last->next->conn_fd;
}
int get_fsz_from_SFF()
{
  return q.last->next->file_size;
}
char *get_fnm_from_SFF()
{
  return q.last->next->filename;
}

//---------------------------------------------------------------------------//

// insert to buffer method
void addToBuffer(int conn_fd, int filesize, char *filename)
{
  pthread_mutex_lock(&lock);
  while (buffer_size == buffer_max_size)
  {
    pthread_cond_wait(&buffer_is_full, &lock);
  }

  if (scheduling_algo == FIFO)
  {
    push(conn_fd, filesize, filename);
  }
  else
  {
    // insert
    insert_into_SFF(conn_fd, filesize, filename);
  }
  printf("--added to buffer : %s", filename);
  pthread_cond_signal(&buffer_is_empty);
  pthread_mutex_unlock(&lock);
}
// Sends out HTTP response in case of errors
//
void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXBUF], body[MAXBUF];

  // Create the body of error message first (have to know its length for header)
  sprintf(body, ""
                "<!doctype html>\r\n"
                "<head>\r\n"
                "  <title>OSTEP WebServer Error</title>\r\n"
                "</head>\r\n"
                "<body>\r\n"
                "  <h2>%s: %s</h2>\r\n"
                "  <p>%s: %s</p>\r\n"
                "</body>\r\n"
                "</html>\r\n",
          errnum, shortmsg, longmsg, cause);

  // Write out the header information for this response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  write_or_die(fd, buf, strlen(buf));

  sprintf(buf, "Content-Type: text/html\r\n");
  write_or_die(fd, buf, strlen(buf));

  sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
  write_or_die(fd, buf, strlen(buf));

  // Write out the body last
  write_or_die(fd, body, strlen(body));

  // close the socket connection
  close_or_die(fd);
}

//
// Reads and discards everything up to an empty text line
//
void request_read_headers(int fd)
{
  char buf[MAXBUF];

  readline_or_die(fd, buf, MAXBUF);
  while (strcmp(buf, "\r\n"))
  {
    readline_or_die(fd, buf, MAXBUF);
  }
  return;
}

//
// Return 1 if static, 0 if dynamic content (executable file)
// Calculates filename (and cgiargs, for dynamic) from uri
//
int request_parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi"))
  {
    // static
    strcpy(cgiargs, "");
    sprintf(filename, ".%s", uri);
    if (uri[strlen(uri) - 1] == '/')
    {
      strcat(filename, "index.html");
    }
    return 1;
  }
  else
  {
    // dynamic
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
    {
      strcpy(cgiargs, "");
    }
    sprintf(filename, ".%s", uri);
    return 0;
  }
}

//
// Fills in the filetype given the filename
//
void request_get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

//
// Handles requests for static content
//
void request_serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXBUF], buf[MAXBUF];

  request_get_filetype(filename, filetype);
  srcfd = open_or_die(filename, O_RDONLY, 0);

  // Rather than call read() to read the file into memory,
  // which would require that we allocate a buffer, we memory-map the file
  srcp = mmap_or_die(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  close_or_die(srcfd);

  // put together response
  sprintf(buf, ""
               "HTTP/1.0 200 OK\r\n"
               "Server: OSTEP WebServer\r\n"
               "Content-Length: %d\r\n"
               "Content-Type: %s\r\n\r\n",
          filesize, filetype);

  write_or_die(fd, buf, strlen(buf));

  //  Writes out to the client socket the memory-mapped file
  write_or_die(fd, srcp, filesize);
  munmap_or_die(srcp, filesize);
}

//
// Fetches the requests from the buffer and handles them (thread logic)
//
void *thread_request_serve_static(void *arg)
{
  // TODO: write code to actualy respond to HTTP requests
  node *temp = (node *)malloc(sizeof(node));
  while (1)
  {
    pthread_mutex_lock(&lock);
    while (buffer_size == 0)
    {
      pthread_cond_wait(&buffer_is_empty, &lock);
    }

    if (scheduling_algo == FIFO)
    {
      temp->conn_fd = front_get_fd();
      temp->file_size = front_get_fsz();
      temp->filename = front_get_fnm();
      pop();
    }
    else if (scheduling_algo == SFF)
    {
      temp->conn_fd = get_fd_from_SFF();
      temp->file_size = get_fsz_from_SFF();
      temp->filename = get_fnm_from_SFF();
      delete_from_SFF();
    }

    pthread_cond_signal(&buffer_is_full);
    pthread_mutex_unlock(&lock);

    printf("\nremoved from file..serving file: %s\n", temp->filename);
    request_serve_static(temp->conn_fd, temp->filename, temp->file_size);
    close_or_die(temp->conn_fd);
    printf("\n..connection closed..\n");
  }
}

//
// Initial handling of the request

void request_handle(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
  char filename[MAXBUF], cgiargs[MAXBUF];

  // get the request type, file path and HTTP version
  readline_or_die(fd, buf, MAXBUF);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("getting info\n");
  printf("method:%s uri:%s version:%s\n", method, uri, version);

  // verify if the request type is GET or not
  if (strcasecmp(method, "GET"))
  {
    request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
    return;
  }
  request_read_headers(fd);

  // check requested content type (static/dynamic)
  is_static = request_parse_uri(uri, filename, cgiargs);

  // get some data regarding the requested file, also check if requested file is present on server
  if (stat(filename, &sbuf) < 0)
  {
    request_error(fd, filename, "404", "Not found", "server could not find this file");
    return;
  }

  // verify if requested content is static
  if (is_static)
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      request_error(fd, filename, "403", "Forbidden", "server could not read this file");
      return;
    }

    // TODO: write code to add HTTP requests in the buffer based on the scheduling policy
    // security code..
    if (strstr(filename, ".."))
    {
      request_error(fd, filename, "403", "Forbidden", "Permission denied!!!");
      return;
    }
    // ADD to Buffer()..method
    addToBuffer(fd, sbuf.st_size, filename);
  }
  else
  {
    request_error(fd, filename, "501", "Not Implemented", "server does not serve dynamic content request");
  }
}
