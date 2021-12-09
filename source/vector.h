// A very simple and unsafe-type vector, just used within the "Reti" project.
#ifndef VECTOR_H
#define VECTOR_H

#define EXPAND_RATIO 2

typedef struct
{
    size_t count;
    size_t capacity;
    size_t data_size;
    void *items;
} vector;

// Initialize the vector with the given data size and capacity.
vector *vector_init(size_t data_size, size_t initial_capacity);
void vector_free(vector *v);
void vector_append(vector *v, void *item);
void *vector_get(vector *v, size_t i);
void vector_set(vector *v, size_t i, void *item);
int is_out_of_bounds(vector *v, size_t i);

#endif