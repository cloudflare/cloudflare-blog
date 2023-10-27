/* compile, but don't link this file:
 * $ gcc -c obj.c
 */

#include <stdio.h>

int add5(int num)
{
    return num + 5;
}

int add10(int num)
{
    num = add5(num);
    return add5(num);
}

const char *get_hello(void)
{
    return "Hello, world!";
}

static int var = 5;

int get_var(void)
{
    return var;
}

void set_var(int num)
{
    var = num;
}

void say_hello(void)
{
    puts("Hello, world!");
}