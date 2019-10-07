#pragma once

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"


class JpegEncoder
{
public:
    JpegEncoder();

public:
    void destroy();

protected:
    MMAL_STATUS_T createComponent();
    void createBufferPool();

public:
    MMAL_COMPONENT_T *pComponent;
    MMAL_POOL_T *pool;
    uint32_t quality;
    uint32_t restartInterval;
    MMAL_FOURCC_T encoding;
};
