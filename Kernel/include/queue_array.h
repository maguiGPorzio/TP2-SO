#ifndef QUEUE_ARRAY_H
#define QUEUE_ARRAY_H

#include "queue.h"
#include <stdint.h>
#include <stdbool.h>

bool queue_array_init(queue_t queue_array[], int size);
void queue_array_destroy(queue_t queue_array[], int size);
bool queue_array_add(queue_t queue_array[], int size, int index, int value);
int queue_array_poll(queue_t queue_array[], int size, int index);
bool queue_array_is_empty(queue_t queue_array[], int size, int index);

#endif