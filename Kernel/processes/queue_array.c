#include "queue_array.h"
#include <stddef.h>

bool queue_array_init(queue_t queue_array[], int size){
    for(int i = 0; i < size; i++){
        queue_array[i] = q_init();
        if(queue_array[i] == NULL){
            // Si falla la creación de alguna cola, destruir las ya creadas
            for(int j = 0; j < i; j++){
                q_destroy(queue_array[j]);
            }
            return false;
        }
    }
    return true;
}

void queue_array_destroy(queue_t queue_array[], int size){
    for(int i = 0; i < size; i++){
        q_destroy(queue_array[i]);
    }
}

bool queue_array_add(queue_t queue_array[], int size, int index, int value){
    if(index < 0 || index >= size){
        return false;
    }
    return q_add(queue_array[index], value);
}

