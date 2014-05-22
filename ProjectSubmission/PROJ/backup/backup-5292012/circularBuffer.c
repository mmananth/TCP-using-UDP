/******************************************************
 *
 ******************************************************/
/* 1. circular buffer will have a size, start, count variables and a variable to store the actual element
 * 2. operations will be read, write, init, isEmpty, isFull and free */

#include <stdio.h>
#include <malloc.h>

typedef struct
{
	int element;
}ElemType;

typedef struct
{
	int size;
	int start;
	int count;
	ElemType *elem;
}CircularBuffer;

/* Initialize the circular buffer. Takes size as one of the arguments */
void cbInit(CircularBuffer *cb, int size)
{
	cb->size = size;
	cb->start = 0;
	cb->count = 0;
	cb->elem = (ElemType *)calloc(cb->size, sizeof(int));
}

void cbFree(CircularBuffer *cb)
{
	free(cb->elem);
}

int cbIsFull(CircularBuffer *cb)
{
	if(cb->count == cb->size)
		return 1;
	else
		return 0;
}
c

int cbRead(CircularBuffer *cb)
{
	cb->count = cb->count - 1;
	if(!cbIsEmpty(&cb))
	{
		cb->start = cb->start - 1;
		return cb->elem[cb->start].element;
	}
}

void cbWrite(CircularBuffer *cb, ElemType *elem)
{
	cb->elem[cb->start].element = elem;
	if(cb->count == cb->size)
	{
		cb->start = (cb->start + 1) % cb->size;
		cb->count = 0;
	}
	else
	{
		cb->count = cb->count + 1;
		cb->start = cb->start + 1;
	}
	/*if(!cbIsFull(&cb))
	{
		cb->elem[cb->start] = elem;
		cb->start = (cb->start % cb->size) + 1;
	}*/
}

int main()
{
	CircularBuffer *cb;
	int i = 0, size = 10;
	ElemType num = {0};
	cbInit(cb,size);
	for(i = 0; i < size; i++)
	{
		num.element = num.element + 1;
		cbWrite(cb,num.element);
	}
	for(i = 0; i < size; i++)
	{
		printf("The value from the CircularBuffer is: %d\n",cbRead(cb));
	}
	/*cbFree(cb);*/
}
