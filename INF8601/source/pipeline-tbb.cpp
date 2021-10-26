#include <stdio.h>
#include <stdlib.h>
#include "filter.h"
#include "pipeline.h"
#include <tbb/pipeline.h>
using namespace tbb;
extern "C" {

int pipeline_tbb(image_dir_t* image_dir) {

    parallel_pipeline(16,
        make_filter<void, image_t*>(filter::serial,
        [&](flow_control& fc)-> image_t*{
            image_t* imageTemp = image_dir_load_next(image_dir);
            if (imageTemp == NULL) fc.stop();
            return imageTemp;
        }) &
        make_filter<image_t*,image_t*>(filter::parallel,
            [](image_t* image){
                image_t* imageTemp = filter_scale_up(image, 3);
                image_destroy(image);     
                return imageTemp;

            }) &
        make_filter<image_t*,image_t*>(filter::parallel,
            [](image_t* image) {
                image_t* imageTemp = filter_sobel(image);
                image_destroy(image);
                return imageTemp;
            }) &
        make_filter<image_t*,void>(filter::parallel,
            [&](image_t* image) {
                image_dir_save(image_dir, image);
                printf(".");
                fflush(stdout);
                image_destroy(image);
            })
        );

        return 0;
    }

} /* extern "C" */
