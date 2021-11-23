#ifndef UTILITIES_H
#define UTILITIES_H

char *rand_alphanumID(char *buffer, size_t size);
int strcontained(const char *str, const char **haystack, int haysize);
uint uintcontained(const uint num, const uint *nums, int nums_size);
#endif