#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

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
    int size, max_size;
} queue;

queue q = {NULL, 0, 50};

// queue funtion
int isEmpty() { return q.size == 0; }
int isFull() { return q.size == q.max_size; }
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
}

int front_get_fd() { return q.last->next->conn_fd; }
int front_get_fsz() { return q.last->next->file_size; }
char *front_get_fnm() { return q.last->next->filename; }

int main()
{
    printf("%d\n", isEmpty());
    printf("%d\n", isFull());
    printf("%d\n", get_size());
    push(23, 244, "kishan");
    printf("%d\n", get_size());
    push(24, 244, "kishanverma");
    printf("%d\n", get_size());
    // pop();
    printf("%d\n", get_size());
    printf("fd:%d\n", front_get_fd());
    printf("fsz:%d\n", front_get_fsz());
    printf("fnm:%s\n", front_get_fnm());
    pop();
    printf("done\n");
    pop();
    printf("done\n");

    return 0;
}