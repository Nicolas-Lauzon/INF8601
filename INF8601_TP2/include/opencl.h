/* DO NOT EDIT THIS FILE */

#ifndef INCLUDE_OPENCL_H_
#define INCLUDE_OPENCL_H_

#include <CL/cl.h>

extern char* opencl_kernel_path;

int opencl_load_kernel_code(char** code, size_t* len);
int opencl_get_device_id(unsigned int platform_index, unsigned int device_index, cl_device_id* context_device_id);
int opencl_print_device_info(cl_device_id device_id);
int opencl_print_build_log(cl_program program, cl_device_id device_id);

#endif /* INCLUDE_OPENCL_H_ */
