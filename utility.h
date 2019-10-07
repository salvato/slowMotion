#pragma once

#include "interface/mmal/mmal.h"

#define verbose false


int mmal_status_to_int(MMAL_STATUS_T status);
int get_mem_gpu(void);
void get_camera(int *supported, int *detected);
void checkConfiguration(int min_gpu_mem);
