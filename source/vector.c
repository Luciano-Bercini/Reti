#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"

vector *vector_init(size_t data_size, size_t initial_capacity)
{
    if (initial_capacity < 1)
    {
        printf("Initial capacity must be 1 or above, setting at 4...\n");
        initial_capacity = 4;
    }
    vector *v = malloc(sizeof(vector));
    v->items = malloc(data_size * initial_capacity);
    if (v->items == NULL)
    {
        printf("Failed to allocate for the vector.\n");
    }
    v->count = 0;
    v->data_size = data_size;
    v->capacity = initial_capacity;
    return v;
}
void vector_free(vector *v)
{
    free(v->items);
    free(v);
}
void vector_append(vector *v, void *item)
{
    if (v->count >= v->capacity)
    {
        v->capacity *= EXPAND_RATIO;
        void *temp = realloc(v->items, v->capacity * v->data_size);
        if (!temp)
        {
            printf("Failed to realloc for the vector.\n");
            return;
        }
        v->items = temp;
    }
    vector_set(v, v->count++, item);
}
void *vector_get(vector *v, size_t i)
{
    return v->items + i * v->data_size;
}
void vector_set(vector *v, size_t i, void *item)
{
    if (is_out_of_bounds(v, i))
    {
        printf("Failed to set element: {%lu} is out of bounds!", i);
        return;
    }
    memcpy(v->items + (i * v->data_size), item, v->data_size);
}
int is_out_of_bounds(vector *v, size_t i)
{
    return i < 0 || i >= v->count;
}