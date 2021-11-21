#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

int strcontained(const char *str, const char **haystack, int haysize)
{
    for (int i = 0; i < haysize; i++)
    {
        if (strcmp(str, haystack[i]) == 0)
        {
            return 0;
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
            return 0;
        }
    }
    return -1;
}