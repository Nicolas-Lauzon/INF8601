#include "log.h"
#include "sinoscope.h"

int sinoscope_opencl_init(sinoscope_opencl_t* opencl, cl_device_id opencl_device_id, unsigned int width,
                          unsigned int height) {
    /*
     * TODO
     *
     * Initialiser `opencl->context` du périphérique reçu en paramètre.
     *
     * Initialiser `opencl->queue` à partir du contexte précèdent.
     *
     * Initialiser `opencl->buffer` à partir du context précèdent (width * height * 3).
     *
     * Initialiser `opencl->kernel` à partir du contexte et du périphérique reçu en
     * paramètre. Utilisez la fonction `opencl_load_kernel_code` déclaré dans `opencl.h` pour lire
     * le code du noyau OpenCL `kernel/sinoscope.cl` dans un tampon. Compiler le noyau
     * en ajoutant le paramètre `"-I " __OPENCL_INCLUDE__`.
     *
     * Vous pouvez utiliser la fonction `opencl_print_device_info` pour obtenir de l'information
     * à propos de la compilation de votre noyau OpenCL.
     */

    

    cl_int ret;
    
    opencl->context = clCreateContext(NULL, 1, &opencl_device_id, NULL, NULL, &ret);
    
    if(ret != CL_SUCCESS) {
        printf("bad context : %d", ret);
        //exit(0);
    }
    
    opencl->queue = clCreateCommandQueueWithProperties(opencl->context, opencl_device_id, 0, &ret);
    if(ret != CL_SUCCESS) {
        printf("bad command queue : %d", ret);
        //exit(0);
    }
    opencl->buffer = clCreateBuffer(opencl->context, CL_MEM_READ_ONLY, (width * height * 3), NULL, &ret);
    if(ret != CL_SUCCESS) {
        printf("bad buffer : %d", ret);
        //exit(0);
    }


    //char **kernel_text = malloc(sizeof(char**));
    //size_t* kernel_len = malloc(sizeof(size_t*));
    char *code = NULL;
    size_t length = 0;
    opencl_load_kernel_code(&code, &length);

    cl_program prog = clCreateProgramWithSource(opencl->context, 1, (const char **) &code, &length, NULL);
    if(ret != CL_SUCCESS) {
        printf("bad create program : %d", ret);
        //exit(0);
    }
    
    ret = clBuildProgram(prog, 0, NULL, "-I " __OPENCL_INCLUDE__, NULL, NULL);
    if(ret != CL_SUCCESS) {
        opencl_print_build_log(prog, opencl_device_id);
        printf("bad program: %d", ret);
        //exit(0);
    }
    free(code);
    opencl->kernel = clCreateKernel(prog, "sinoscope_kernel", &ret);
    if(ret != CL_SUCCESS) {
        printf("bad kernel : %d", ret);
        //exit(0);
    }
    return 0;
}

void sinoscope_opencl_cleanup(sinoscope_opencl_t* opencl) {
    /*
     * TODO
     *
     * Libérez les ressources associées à `opencl->context`, `opencl->queue`,
     * `opencl->buffer` et `opencl->kernel`.
     */
    if (opencl->queue)   clReleaseCommandQueue(opencl->queue);
	if (opencl->buffer)  clReleaseMemObject(opencl->buffer);
	if (opencl->kernel)  clReleaseKernel(opencl->kernel);
	if (opencl->context) clReleaseContext(opencl->context);
}

/*
 * TODO
 *
 * Déclarez les structures partagées avec le noyau OpenCL si nécessaire selon votre énoncé.
 * Utilisez l'attribut `__attribute__((packed))` à vos déclarations.
 */


int sinoscope_image_opencl(sinoscope_t* sinoscope) {
    if (sinoscope->opencl == NULL) {
        LOG_ERROR_NULL_PTR();
        goto fail_exit;
    }

    /*
     * TODO
     *
     * Configurez les paramètres du noyau OpenCL afin d'envoyer `sinoscope->opencl->buffer` et les
     * autres données nécessaire à son exécution.
     *
     * Démarrez le noyau OpenCL et attendez son exécution.
     *
     * Effectuez la lecture du tampon `sinoscope->opencl->buffer` contenant le résultat dans
     * `sinoscope->buffer`.
     */
    cl_int ret;
    cl_event ev;
    size_t work_dim[2] = {(size_t)sinoscope->width, (size_t)sinoscope->height};
    ret = clSetKernelArg(sinoscope->opencl->kernel, 0, sizeof(sinoscope->opencl->buffer), &(sinoscope->opencl->buffer));
	if(ret != CL_SUCCESS) {
        printf("bad arg 0 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 1, sizeof(int),    &(sinoscope->width));
	if(ret != CL_SUCCESS) {
        printf("bad arg 1 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 2, sizeof(int),    &(sinoscope->taylor));
	if(ret != CL_SUCCESS) {
        printf("bad arg 2 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 3, sizeof(int),    &(sinoscope->interval));
	if(ret != CL_SUCCESS) {
        printf("bad arg 3 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 4, sizeof(float),  &(sinoscope->interval_inverse));
	if(ret != CL_SUCCESS) {
        printf("bad arg 4 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 5, sizeof(float),  &(sinoscope->time));
	if(ret != CL_SUCCESS) {
        printf("bad arg 5 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 6, sizeof(float),  &(sinoscope->phase0));
	if(ret != CL_SUCCESS) {
        printf("bad arg 6 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 7, sizeof(float),  &(sinoscope->phase1));
	if(ret != CL_SUCCESS) {
        printf("bad arg 7 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 8, sizeof(float),  &(sinoscope->dx));
	if(ret != CL_SUCCESS) {
        printf("bad arg 8 : %d", ret);
        //exit(0);
    }
    ret = clSetKernelArg(sinoscope->opencl->kernel, 9, sizeof(float),  &(sinoscope->dy));
    if(ret != CL_SUCCESS) {
            printf("bad arg 9 : %d", ret);
            //exit(0);
    }
    /*ret = clSetKernelArg(sinoscope->opencl->kernel, 10, sizeof(float),  &(sinoscope->handler));
    if(ret != CL_SUCCESS) {
            printf("bad arg 10 : %d", ret);
            //exit(0);
    }*/

    ret = clEnqueueNDRangeKernel(sinoscope->opencl->queue, sinoscope->opencl->kernel, 2, NULL, work_dim, NULL, 0, NULL, &ev);
    if(ret != CL_SUCCESS) {
            printf("bad range for i and y : %d", ret);
            //exit(0);
    }

    ret = clFinish(sinoscope->opencl->queue);
    if(ret != CL_SUCCESS) {
            printf("bad cl finish : %d", ret);
            //exit(0);
    }

    ret = clEnqueueReadBuffer(sinoscope->opencl->queue, sinoscope->opencl->buffer, CL_TRUE, 0, sinoscope->buffer_size, sinoscope->buffer, 0, NULL, &ev);
    if(ret != CL_SUCCESS) {
                printf("bad read buffer : %d", ret);
                //exit(0);
    }
    return 0;
fail_exit:
    return -1;
}
