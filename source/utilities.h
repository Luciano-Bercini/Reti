#ifndef UTILITIES_H
#define UTILITIES_H

// Fills the buffer with a random alphanumeric string of the given size.
char *rand_alphanumID(char *buffer, size_t size);
// Returns the index position of the first string found in the haystack, else returns -1.
int strcontained(const char *str, const char **haystack, int haysize);
// Returns the index position of the first uint found in the nums, else returns -1.
uint uintcontained(const uint num, const uint *nums, int nums_size);
// Returns the minimum between a and b.
int min(int a, int b);
// Returns the maximum between a and b.
int max(int a, int b);
#endif