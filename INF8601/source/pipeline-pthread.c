#include <stdio.h>
#include <stdlib.h>

#include "filter.h"
#include "pipeline.h"
#include "queue.h"
#define MAX_IMAGE_COUNT 400
#define THREAD_COUNT 6

typedef struct arguments {
    queue_t* image_load_next_q;
    queue_t* filter_scale_up_q;
    queue_t* filter_sobel_q;
    image_dir_t* image_dir;
} arguments_t;

void* call_image_dir_load_next(void *myQueues) {
    arguments_t* args = (arguments_t*) myQueues;
    int pushed_item;
    int index;
    while(1) {
        image_t* image1 = image_dir_load_next(args->image_dir);
        if (image1 == NULL) {
            pushed_item = queue_push(args->image_load_next_q, NULL);
            break;
        }
        pushed_item = queue_push(args->image_load_next_q, image1);
    } 
}

void* call_filter_scale_up(void *myQueues) {
    arguments_t* args = (arguments_t*) myQueues;
    int pushed_item;
    while(1){
        image_t* image1 = queue_pop(args->image_load_next_q);
        if (image1 == NULL) {
            pushed_item = queue_push(args->filter_scale_up_q, NULL);
            pushed_item = queue_push(args->filter_scale_up_q, NULL);
            pushed_item = queue_push(args->filter_scale_up_q, NULL);
            break;
        }
        else{
            image_t* image2 = filter_scale_up(image1, 3);
            
            image_destroy(image1);
            pushed_item = queue_push(args->filter_scale_up_q, image2);
        }
    }  
}

void* call_filter_sobel(void *myQueues) {
    arguments_t* args = (arguments_t*) myQueues;
    int pushed_item;
    while(1){
        image_t* image2 = queue_pop(args->filter_scale_up_q);
        if (image2 == NULL) {
            pushed_item = queue_push(args->filter_sobel_q, NULL);
            break;
        }
        else {
            image_t* image3 = filter_sobel(image2);
            image_destroy(image2);
            pushed_item = queue_push(args->filter_sobel_q, image3);
        }
    }
}

void* call_image_save(void *myQueues) {
    arguments_t* args = (arguments_t*) myQueues;
    int count = 0;
    while(1){
        image_t* image4 = queue_pop(args->filter_sobel_q);
        if(image4 == NULL){
            if(count == 2){
                break;
            }
            count++; 
        }
        else{
            image_dir_save(args->image_dir, image4);
            printf(".");
            fflush(stdout);
            image_destroy(image4);
        }
    }
}

int pipeline_pthread(image_dir_t* image_dir) {
    arguments_t* args = calloc(0, sizeof(arguments_t*));

    args->image_load_next_q = queue_create(MAX_IMAGE_COUNT);
    args->filter_scale_up_q = queue_create(MAX_IMAGE_COUNT);
    args->filter_sobel_q = queue_create(MAX_IMAGE_COUNT);
    args->image_dir = image_dir;

    pthread_t threads[THREAD_COUNT];

    int test1 = pthread_create(&threads[0], NULL, call_image_dir_load_next, (void*)args);
    int test2 = pthread_create(&threads[1], NULL, call_filter_scale_up, (void*)args);
    int test3 = pthread_create(&threads[2], NULL, call_filter_sobel, (void*)args);
    int test4 = pthread_create(&threads[3], NULL, call_filter_sobel, (void*)args);
    int test5 = pthread_create(&threads[4], NULL, call_filter_sobel, (void*)args);
    int test6 = pthread_create(&threads[5], NULL, call_image_save, (void*)args);

    pthread_exit(NULL);

    return 0;
}


