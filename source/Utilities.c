#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

// Returns the index position of the first string found in the haystack, else returns -1.
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
// Returns the index position of the first uint found in the nums, else returns -1.
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