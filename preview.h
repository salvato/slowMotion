#pragma once


#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"


class Preview
{
public:
    Preview(int width, int height);

public:
    void destroy();
    void dump_parameters();
    MMAL_STATUS_T setScreenPos(MMAL_RECT_T previewWindow);

protected:
    MMAL_STATUS_T createComponent();

public:
    /// Layer that preview window should be displayed on
    static const int PREVIEW_LAYER = 2;

    /// Frames rates of 0 implies variable,
    /// but denominator needs to be 1 to prevent div by 0
    static const int PREVIEW_FRAME_RATE_NUM = 0;
    static const int PREVIEW_FRAME_RATE_DEN = 1;
    ///
    static const int FULL_RES_PREVIEW_FRAME_RATE_NUM = 0;
    static const int FULL_RES_PREVIEW_FRAME_RATE_DEN = 1;
    ///
    static const int FULL_FOV_PREVIEW_16x9_X = 1280;
    static const int FULL_FOV_PREVIEW_16x9_Y = 720;
    ///
    static const int FULL_FOV_PREVIEW_4x3_X = 1296;
    static const int FULL_FOV_PREVIEW_4x3_Y = 972;
    ///
    static const int FULL_FOV_PREVIEW_FRAME_RATE_NUM = 0;
    static const int FULL_FOV_PREVIEW_FRAME_RATE_DEN = 1;

    int wantPreview;                       /// Display a preview
    int wantFullScreenPreview;             /// 0 is use previewRect, non-zero to use full screen
    int opacity;                           /// Opacity of window - 0 = transparent, 255 = opaque
    MMAL_RECT_T previewWindow;             /// Destination rectangle for the preview window.
    MMAL_COMPONENT_T *pComponent;
};
