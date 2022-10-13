#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stringstack.h"

stringStack newStack(int size)
{
	stringStack *stack = (stringStack*)malloc(sizeof(stringStack));

	stack->maxSize = size;
	stack->top = -1;

	return *stack;
}

int push(stringStack *stack, char * element)
{
	//increase top cursor and insert element
	if (stack->top == stack->maxSize)
		return EXIT_FAILURE;

	stack->top++;
	strcpy(stack->items[stack->top], element);

	return EXIT_SUCCESS;
}

int pop(stringStack* stack)
{
	//decrease top cursor and remove element
	if (stack->top < 0)
		return EXIT_FAILURE;
	stack->items[stack->top][0] = '\0';
	stack->top--;

	return EXIT_SUCCESS;
}