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


typedef struct mmal_param_thumbnail_config_s {
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
typedef struct mmal_param_colourfx_s {
    int enable;       /// Turn colourFX on or off
    int u,v;          /// U and V to use
} MMAL_PARAM_COLOURFX_T;


typedef struct param_float_rect_s {
    double x;
    double y;
    double w;
    double h;
} PARAM_FLOAT_RECT_T;


// struct containing camera settings
typedef struct {
    int sharpness;             /// -100 to 100
    int contrast;              /// -100 to 100
    int brightness;            ///    0 to 100
    int saturation;            /// -100 to 100
    int ISO;                   ///  TODO : what range?
    int videoStabilisation;    /// 0 or 1 (false or true)
    int exposureCompensation;  ///  -10 to +10 ?
    MMAL_PARAM_EXPOSUREMODE_T exposureMode;
    MMAL_PARAM_EXPOSUREMETERINGMODE_T exposureMeterMode;
    MMAL_PARAM_AWBMODE_T awbMode;
    MMAL_PARAM_IMAGEFX_T imageEffect;
    MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imageEffectsParameters;
    MMAL_PARAM_COLOURFX_T colourEffects;
    MMAL_PARAM_FLICKERAVOID_T flickerAvoidMode;
    int rotation;              /// 0-359
    int hflip;                 /// 0 or 1
    int vflip;                 /// 0 or 1
    PARAM_FLOAT_RECT_T  roi;   /// region of interest to use on the sensor. Normalised [0,1] values in the rect
    int shutter_speed;         /// 0 = auto, otherwise the shutter speed in ms
    float awb_gains_r;         /// AWB red gain
    float awb_gains_b;         /// AWB blue gain
    MMAL_PARAMETER_DRC_STRENGTH_T drc_level;  /// Strength of Dynamic Range compression to apply
    MMAL_BOOL_T stats_pass;    /// Stills capture statistics pass on/off
    int enable_annotate;       /// Flag to enable the annotate, 0 = disabled, otherwise a bitmask of what needs to be displayed
    char annotate_string[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2]; /// String to use for annotate - overrides certain bitmask settings
    int annotate_text_size;    /// Text size for annotation
    int annotate_text_colour;  /// Text colour for annotation
    int annotate_bg_colour;    /// Background colour for annotation
    unsigned int annotate_justify;
    unsigned int annotate_x;
    unsigned int annotate_y;
    MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo_mode;
    float analog_gain;         /// Analog gain
    float digital_gain;        /// Digital gain
    int settings;
    int onlyLuma;              /// Only output the luma / Y plane of the YUV data
} RASPICAM_CAMERA_PARAMETERS;


class CameraControl
{
public:
    CameraControl();
    int mmal_status_to_int(MMAL_STATUS_T status);
    int get_mem_gpu(void);
    void get_camera(int *supported, int *detected);
    void checkConfiguration(int min_gpu_mem);

// Individual setting functions
    int set_saturation(MMAL_COMPONENT_T *camera, int saturation);
    int set_sharpness(MMAL_COMPONENT_T *camera, int sharpness);
    int set_contrast(MMAL_COMPONENT_T *camera, int contrast);
    int set_brightness(MMAL_COMPONENT_T *camera, int brightness);
    int set_ISO(MMAL_COMPONENT_T *camera, int ISO);
    int set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation);
    int set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp);
    int set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode);
    int set_flicker_avoid_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_FLICKERAVOID_T mode);
    int set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode );
    int set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode);
    int set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain, float b_gain);
    int set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX);
    int set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX);
    int set_rotation(MMAL_COMPONENT_T *camera, int rotation);
    int set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip);
    int set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect);
    int set_shutter_speed(MMAL_COMPONENT_T *camera, int speed);
    int set_DRC(MMAL_COMPONENT_T *camera, MMAL_PARAMETER_DRC_STRENGTH_T strength);
    int set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass);
    int set_annotate(MMAL_COMPONENT_T *camera, const int settings, const char *string,
                     const int text_size, const int text_colour, const int bg_colour,
                     const unsigned int justify, const unsigned int x, const unsigned int y);
    int set_gains(MMAL_COMPONENT_T *camera, float analog, float digital);
    int zoom_in_zoom_out(MMAL_COMPONENT_T *camera, ZOOM_COMMAND_T zoom_command, PARAM_FLOAT_RECT_T *roi);
    int set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode);

//Individual getting functions (NOT YET IMPLEMENTED)
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
    void set_defaults();

public:
    RASPICAM_CAMERA_PARAMETERS cameraParameters;
};
