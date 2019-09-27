#pragma once

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"


// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1


// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define CAMERA_SETTLE_TIME 1000 // In ms


// Supplied by user
#define ANNOTATE_USER_TEXT          1
// Supplied by app using this module
#define ANNOTATE_APP_TEXT           2
// Insert current date
#define ANNOTATE_DATE_TEXT          4
// Insert current time
#define ANNOTATE_TIME_TEXT          8

#define ANNOTATE_SHUTTER_SETTINGS   16
#define ANNOTATE_CAF_SETTINGS       32
#define ANNOTATE_GAIN_SETTINGS      64
#define ANNOTATE_LENS_SETTINGS      128
#define ANNOTATE_MOTION_SETTINGS    256
#define ANNOTATE_FRAME_NUMBER       512
#define ANNOTATE_BLACK_BACKGROUND   1024


typedef struct {
    int enable;
    int width,height;
    int quality;
} MMAL_PARAM_THUMBNAIL_CONFIG_T;


typedef enum {
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_RESET
} ZOOM_COMMAND_T;


// There isn't actually a MMAL structure for the following, so make one
typedef struct {
    int enable;       /// Turn colourFX on or off
    int u,v;          /// U and V to use
} MMAL_PARAM_COLOURFX_T;


typedef struct {
    double x;
    double y;
    double w;
    double h;
} PARAM_FLOAT_RECT_T;


// struct containing camera settings
typedef struct {
} RASPICAM_CAMERA_PARAMETERS;


class CameraControl
{
public:
    CameraControl(MMAL_COMPONENT_T *pCameraComponent);

// Individual setting functions
    int set_saturation(int saturation);
    int set_sharpness(int sharpness);
    int set_contrast(int contrast);
    int set_brightness(int brightness);
    int set_ISO(int ISO);
    int set_video_stabilisation(int vstabilisation);
    int set_exposure_compensation(int exp_comp);
    int set_exposure_mode(MMAL_PARAM_EXPOSUREMODE_T mode);
    int set_flicker_avoid_mode(MMAL_PARAM_FLICKERAVOID_T mode);
    int set_metering_mode(MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode );
    int set_awb_mode(MMAL_PARAM_AWBMODE_T awb_mode);
    int set_awb_gains(float r_gain, float b_gain);
    int set_imageFX(MMAL_PARAM_IMAGEFX_T imageFX);
    int set_colourFX(const MMAL_PARAM_COLOURFX_T *colourFX);
    int set_rotation(int rotation);
    int set_flips(int hflip, int vflip);
    int set_ROI(PARAM_FLOAT_RECT_T rect);
    int set_shutter_speed(int speed);
    int set_DRC(MMAL_PARAMETER_DRC_STRENGTH_T strength);
    int set_stats_pass(int stats_pass);
    int set_annotate(const int settings, const char *string,
                     const int text_size, const int text_colour, const int bg_colour,
                     const unsigned int justify, const unsigned int x, const unsigned int y);
    int set_gains(float analog, float digital);
    int zoom_in_zoom_out(ZOOM_COMMAND_T zoom_command, PARAM_FLOAT_RECT_T *roi);
    int set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode);

//Individual getting functions (NOT YET IMPLEMENTED)
    uint32_t get_shutter_speed();

//    int get_saturation(MMAL_COMPONENT_T *camera);
//    int get_sharpness(MMAL_COMPONENT_T *camera);
//    int get_contrast(MMAL_COMPONENT_T *camera);
//    int get_brightness(MMAL_COMPONENT_T *camera);
//    int get_ISO(MMAL_COMPONENT_T *camera);
//    int get_video_stabilisation(MMAL_COMPONENT_T *camera);
//    int get_exposure_compensation(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_EXPOSUREMETERINGMODE_T get_metering_mode(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_THUMBNAIL_CONFIG_T     get_thumbnail_parameters(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_EXPOSUREMODE_T         get_exposure_mode(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_FLICKERAVOID_T         get_flicker_avoid_mode(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_AWBMODE_T              get_awb_mode(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_IMAGEFX_T              get_imageFX(MMAL_COMPONENT_T *camera);
//    MMAL_PARAM_COLOURFX_T             get_colourFX(MMAL_COMPONENT_T *camera);

protected:

public:

private:
    MMAL_COMPONENT_T *pComponent;
};
