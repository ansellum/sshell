#ifndef STRINGSTACK_H_
#define STRINGSTACK_H_

#define ARGLENGTH_MAX 32

typedef struct stringStack
{
        int maxSize;
        int top;
        char* items[ARGLENGTH_MAX];
}stringStack;

stringStack newStack(int size);

int push(stringStack* stack, char * element);

int pop(stringStack* stack);

#endif