#ifndef STRINGSTACK_H_
#define STRINGSTACK_H "stringstack.h"

typedef struct stringStack
{
        int maxSize;
        int top;
        char** items;
}stringStack;

stringStack* newStack(int size);

int push(stringStack* stack, char * element);

int pop(stringStack *stack);

#endif