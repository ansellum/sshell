#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stringstack.h"

#define PATH_MAX 4096

stringStack* newStack(int size)
{
	stringStack* stack = (stringStack*)malloc(sizeof(stringStack));

	stack->maxSize = size;
	stack->top = -1;
	stack->items = malloc(size * sizeof(char*));

	return stack;
}

int push(stringStack *stack, char * element)
{
	//increase top cursor and insert element
	if (stack->top == stack->maxSize)
		return EXIT_FAILURE;

	stack->top++;
	stack->items[stack->top] = malloc(PATH_MAX * sizeof(char));
	strcpy(stack->items[stack->top], element);

	return EXIT_SUCCESS;
}

char * pop(stringStack *stack)
{
	//decrease top cursor and remove element
	if (stack->top < 0)
		return NULL;

	char* prevDirectory = strdup(stack->items[stack->top]);

	stack->items[stack->top][0] = '\0';

	stack->top--;

	return prevDirectory;
}