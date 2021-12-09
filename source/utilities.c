#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

char *rand_alphanumID(char *buffer, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (size > 0)
    {
        size--; // Reserve space for end of string character.
        for (size_t i = 0; i < size - 1; i++)
        {
            int key = rand() % (int)(sizeof(charset) - 1);
            buffer[i] = charset[key];
        }
        buffer[size] = '\0';
    }
    return buffer;
}
int strcontained(const char *str, const char **haystack, int haysize)
{
    for (int i = 0; i < haysize; i++)
    {
        if (strcmp(str, haystack[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}
uint uintcontained(const uint num, const uint *nums, int nums_size)
{
    for (int i = 0; i < nums_size; i++)
    {
        if (num == nums[i])
        {
            return i;
        }
    }
    return -1;
}
int min(int a, int b)
{
    return a > b ? b : a;
}
int max(int a, int b)
{
    return a > b ? a : b;
}