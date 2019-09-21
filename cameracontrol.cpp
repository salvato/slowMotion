#include "cameracontrol.h"
#include "bcm_host.h"
#include <QDebug>
#include <QString>


#define zoom_full_16P16 (uint(65536 * 0.15))
#define zoom_increment_16P16 (65536UL / 10)


CameraControl::CameraControl() {
    // Default everything to zero
    memset(&cameraParameters, 0, sizeof(RASPICAM_CAMERA_PARAMETERS));
    set_defaults();
}


/// Convert a MMAL status return value to a simple boolean of success
/// Also displays a fault if code is not success
/// @param status The error code to convert
/// @return 0 if status is success, 1 otherwise
int
CameraControl::mmal_status_to_int(MMAL_STATUS_T status) {
   if(status == MMAL_SUCCESS)
      return 0;
   else {
      switch (status) {
          case MMAL_ENOMEM :
             qDebug() << QString("Out of memory");
             break;
          case MMAL_ENOSPC :
             qDebug() << QString("Out of resources (other than memory)");
             break;
          case MMAL_EINVAL:
             qDebug() << QString("Argument is invalid");
             break;
          case MMAL_ENOSYS :
             qDebug() << QString("Function not implemented");
             break;
          case MMAL_ENOENT :
             qDebug() << QString("No such file or directory");
             break;
          case MMAL_ENXIO :
             qDebug() << QString("No such device or address");
             break;
          case MMAL_EIO :
             qDebug() << QString("I/O error");
             break;
          case MMAL_ESPIPE :
             qDebug() << QString("Illegal seek");
             break;
          case MMAL_ECORRUPT :
             qDebug() << QString("Data is corrupt \attention FIXME: not POSIX");
             break;
          case MMAL_ENOTREADY :
             qDebug() << QString("Component is not ready \attention FIXME: not POSIX");
             break;
          case MMAL_ECONFIG :
             qDebug() << QString("Component is not configured \attention FIXME: not POSIX");
             break;
          case MMAL_EISCONN :
             qDebug() << QString("Port is already connected ");
             break;
          case MMAL_ENOTCONN :
             qDebug() << QString("Port is disconnected");
             break;
          case MMAL_EAGAIN :
             qDebug() << QString("Resource temporarily unavailable. Try again later");
             break;
          case MMAL_EFAULT :
             qDebug() << QString("Bad address");
             break;
          default :
             qDebug() << QString("Unknown status error");
             break;
      }
      return 1;
   }
}


/**
 * Asked GPU how much memory it has allocated
 *
 * @return amount of memory in MB
 */
int
CameraControl::get_mem_gpu(void) {
    char response[80] = "";
    int gpu_mem = 0;
    if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
        vc_gencmd_number_property(response, "gpu", &gpu_mem);
    return gpu_mem;
}


/**
 * Ask GPU about its camera abilities
 * @param supported None-zero if software supports the camera
 * @param detected  None-zero if a camera has been detected
 */
void
CameraControl::get_camera(int *supported, int *detected) {
    char response[80] = "";
    if (vc_gencmd(response, sizeof response, "get_camera") == 0) {
        if (supported)
            vc_gencmd_number_property(response, "supported", supported);
        if (detected)
            vc_gencmd_number_property(response, "detected", detected);
    }
}



/**
 * Check to see if camera is supported, and we have allocated enough memory
 * Ask GPU about its camera abilities
 * @param supported None-zero if software supports the camera
 * @param detected  None-zero if a camera has been detected
 */
void
CameraControl::checkConfiguration(int min_gpu_mem) {
    int gpu_mem = get_mem_gpu();
    int supported = 0, detected = 0;
    get_camera(&supported, &detected);
    if (!supported)
        qDebug() << QString("Camera is not enabled in this build. Try running \"sudo raspi-config\" and ensure that \"camera\" has been enabled\n");
    else if (gpu_mem < min_gpu_mem)
        qDebug() << QString("Only %1M of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %2 or greater\n")
                    .arg(gpu_mem)
                    .arg(min_gpu_mem);
    else if (!detected)
        qDebug() << QString("Camera is not detected. Please check carefully the camera module is installed correctly\n");
    else
        qDebug() << QString("Failed to run camera app. Please check for firmware updates\n");
}


///  Give the supplied parameter block a set of default values
///  @param Pointer to parameter block
void
CameraControl::set_defaults() {
    cameraParameters.sharpness             = 0;
    cameraParameters.contrast              = 0;
    cameraParameters.brightness            = 50;
    cameraParameters.saturation            = 0;
    cameraParameters.ISO                   = 0;// 0 = auto
    cameraParameters.videoStabilisation    = 0;
    cameraParameters.exposureCompensation  = 0;
    cameraParameters.exposureMode          = MMAL_PARAM_EXPOSUREMODE_AUTO;
    cameraParameters.flickerAvoidMode      = MMAL_PARAM_FLICKERAVOID_OFF;
    cameraParameters.exposureMeterMode     = MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
    cameraParameters.awbMode               = MMAL_PARAM_AWBMODE_AUTO;
    cameraParameters.imageEffect           = MMAL_PARAM_IMAGEFX_NONE;
    cameraParameters.colourEffects.enable  = 0;
    cameraParameters.colourEffects.u       = 128;
    cameraParameters.colourEffects.v       = 128;
    cameraParameters.rotation              = 0;
    cameraParameters.hflip                 = 0;
    cameraParameters.vflip                 = 0;
    cameraParameters.roi.x                 = 0.0;
    cameraParameters.roi.y                 = 0.0;
    cameraParameters.roi.w                 = 1.0;
    cameraParameters.roi.h                 = 1.0;
    cameraParameters.shutter_speed         = 0;// 0 = auto
    cameraParameters.awb_gains_r           = 0;// Only have any function if AWB OFF is used.
    cameraParameters.awb_gains_b           = 0;
    cameraParameters.drc_level             = MMAL_PARAMETER_DRC_STRENGTH_OFF;
    cameraParameters.stats_pass            = MMAL_FALSE;
    cameraParameters.enable_annotate       = 0;
    cameraParameters.annotate_string[0]    = '\0';
    cameraParameters.annotate_text_size    = 0; //Use firmware default
    cameraParameters.annotate_text_colour  = -1;//Use firmware default
    cameraParameters.annotate_bg_colour    = -1;//Use firmware default
    cameraParameters.stereo_mode.mode      = MMAL_STEREOSCOPIC_MODE_NONE;
    cameraParameters.stereo_mode.decimate  = MMAL_FALSE;
    cameraParameters.stereo_mode.swap_eyes = MMAL_FALSE;
}


/**
 * Adjust the saturation level for images
 * @param camera Pointer to camera component
 * @param saturation Value to adjust, -100 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_saturation(MMAL_COMPONENT_T *camera, int saturation) {
    int ret = 0;
    if(!camera)
        return 1;
    if(saturation >= -100 && saturation <= 100) {
        MMAL_RATIONAL_T value = {saturation, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SATURATION, value));
    }
    else {
        qDebug() << QString("Invalid saturation value");
        ret = 1;
    }
    return ret;
}


/**
 * Set the sharpness of the image
 * @param camera Pointer to camera component
 * @param sharpness Sharpness adjustment -100 to 100
 */
int
CameraControl::set_sharpness(MMAL_COMPONENT_T *camera, int sharpness) {
    int ret = 0;
    if(!camera)
        return 1;
    if(sharpness >= -100 && sharpness <= 100) {
        MMAL_RATIONAL_T value = {sharpness, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SHARPNESS, value));
    }
    else {
        qDebug() << QString("Invalid sharpness value");
        ret = 1;
    }

    return ret;
}


/**
 * Set the contrast adjustment for the image
 * @param camera Pointer to camera component
 * @param contrast Contrast adjustment -100 to  100
 * @return
 */
int
CameraControl::set_contrast(MMAL_COMPONENT_T *camera, int contrast) {
    int ret = 0;
    if(!camera)
        return 1;
    if(contrast >= -100 && contrast <= 100) {
        MMAL_RATIONAL_T value = {contrast, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_CONTRAST, value));
    }
    else {
        qDebug() << QString("Invalid contrast value");
        ret = 1;
    }
    return ret;
}


/**
 * Adjust the brightness level for images
 * @param camera Pointer to camera component
 * @param brightness Value to adjust, 0 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_brightness(MMAL_COMPONENT_T *camera, int brightness) {
    int ret = 0;
    if(!camera)
        return 1;
    if(brightness >= 0 && brightness <= 100) {
        MMAL_RATIONAL_T value = {brightness, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_BRIGHTNESS, value));
    }
    else {
        qDebug() << QString("Invalid brightness value");
        ret = 1;
    }

    return ret;
}


/**
 * Adjust the ISO used for images
 * @param camera Pointer to camera component
 * @param ISO Value to set TODO :
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_ISO(MMAL_COMPONENT_T *camera, int ISO) {
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, uint32_t(ISO)));
}


/**
 * Adjust the exposure compensation for images (EV)
 * @param camera Pointer to camera component
 * @param exp_comp Value to adjust, -10 to +10
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp) {
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set_int32(camera->control, MMAL_PARAMETER_EXPOSURE_COMP, exp_comp));
}


/**
 * Set the video stabilisation flag. Only used in video mode
 * @param camera Pointer to camera component
 * @param saturation Flag 0 off 1 on
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation) {
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_VIDEO_STABILISATION, vstabilisation));
}


/**
 * Set exposure mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_EXPOSUREMODE_OFF,
 *   - MMAL_PARAM_EXPOSUREMODE_AUTO,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
 *   - MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPORTS,
 *   - MMAL_PARAM_EXPOSUREMODE_SNOW,
 *   - MMAL_PARAM_EXPOSUREMODE_BEACH,
 *   - MMAL_PARAM_EXPOSUREMODE_VERYLONG,
 *   - MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
 *   - MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
 *   - MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode) {
    MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {{MMAL_PARAMETER_EXPOSURE_MODE,sizeof(exp_mode)}, mode};
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &exp_mode.hdr));
}


/**
 * Set flicker avoid mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_FLICKERAVOID_OFF,
 *   - MMAL_PARAM_FLICKERAVOID_AUTO,
 *   - MMAL_PARAM_FLICKERAVOID_50HZ,
 *   - MMAL_PARAM_FLICKERAVOID_60HZ,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_flicker_avoid_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_FLICKERAVOID_T mode) {
    MMAL_PARAMETER_FLICKERAVOID_T fl_mode = {{MMAL_PARAMETER_FLICKER_AVOID,sizeof(fl_mode)}, mode};
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &fl_mode.hdr));
}


/**
 * Adjust the metering mode for images
 * @param camera Pointer to camera component
 * @param saturation Value from following
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode ) {
    MMAL_PARAMETER_EXPOSUREMETERINGMODE_T meter_mode = {{MMAL_PARAMETER_EXP_METERING_MODE,sizeof(meter_mode)},
                                                        m_mode
                                                       };
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &meter_mode.hdr));
}


/**
 * Set the aWB (auto white balance) mode for images
 * @param camera Pointer to camera component
 * @param awb_mode Value to set from
 *   - MMAL_PARAM_AWBMODE_OFF,
 *   - MMAL_PARAM_AWBMODE_AUTO,
 *   - MMAL_PARAM_AWBMODE_SUNLIGHT,
 *   - MMAL_PARAM_AWBMODE_CLOUDY,
 *   - MMAL_PARAM_AWBMODE_SHADE,
 *   - MMAL_PARAM_AWBMODE_TUNGSTEN,
 *   - MMAL_PARAM_AWBMODE_FLUORESCENT,
 *   - MMAL_PARAM_AWBMODE_INCANDESCENT,
 *   - MMAL_PARAM_AWBMODE_FLASH,
 *   - MMAL_PARAM_AWBMODE_HORIZON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode) {
    MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE,sizeof(param)}, awb_mode};
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &param.hdr));
}


int
CameraControl::set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain, float b_gain) {
    MMAL_PARAMETER_AWB_GAINS_T param = {{MMAL_PARAMETER_CUSTOM_AWB_GAINS,sizeof(param)}, {0,0}, {0,0}};
    if(!camera)
        return 1;
    if(r_gain==0.0f || b_gain==0.0f)
        return 0;
    param.r_gain.num = int(r_gain * 65536.0f);
    param.b_gain.num = int(b_gain * 65536.0f);
    param.r_gain.den = param.b_gain.den = 65536;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &param.hdr));
}


/**
 * Set the image effect for the images
 * @param camera Pointer to camera component
 * @param imageFX Value from
 *   - MMAL_PARAM_IMAGEFX_NONE,
 *   - MMAL_PARAM_IMAGEFX_NEGATIVE,
 *   - MMAL_PARAM_IMAGEFX_SOLARIZE,
 *   - MMAL_PARAM_IMAGEFX_POSTERIZE,
 *   - MMAL_PARAM_IMAGEFX_WHITEBOARD,
 *   - MMAL_PARAM_IMAGEFX_BLACKBOARD,
 *   - MMAL_PARAM_IMAGEFX_SKETCH,
 *   - MMAL_PARAM_IMAGEFX_DENOISE,
 *   - MMAL_PARAM_IMAGEFX_EMBOSS,
 *   - MMAL_PARAM_IMAGEFX_OILPAINT,
 *   - MMAL_PARAM_IMAGEFX_HATCH,
 *   - MMAL_PARAM_IMAGEFX_GPEN,
 *   - MMAL_PARAM_IMAGEFX_PASTEL,
 *   - MMAL_PARAM_IMAGEFX_WATERCOLOUR,
 *   - MMAL_PARAM_IMAGEFX_FILM,
 *   - MMAL_PARAM_IMAGEFX_BLUR,
 *   - MMAL_PARAM_IMAGEFX_SATURATION,
 *   - MMAL_PARAM_IMAGEFX_COLOURSWAP,
 *   - MMAL_PARAM_IMAGEFX_WASHEDOUT,
 *   - MMAL_PARAM_IMAGEFX_POSTERISE,
 *   - MMAL_PARAM_IMAGEFX_COLOURPOINT,
 *   - MMAL_PARAM_IMAGEFX_COLOURBALANCE,
 *   - MMAL_PARAM_IMAGEFX_CARTOON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX) {
    MMAL_PARAMETER_IMAGEFX_T imgFX = {{MMAL_PARAMETER_IMAGE_EFFECT,sizeof(imgFX)}, imageFX};
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &imgFX.hdr));
}


/**
 * Set the colour effect  for images (Set UV component)
 * @param camera Pointer to camera component
 * @param colourFX  Contains enable state and U and V numbers to set (e.g. 128,128 = Black and white)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX) {
    MMAL_PARAMETER_COLOURFX_T colfx = {{MMAL_PARAMETER_COLOUR_EFFECT,sizeof(colfx)}, 0, 0, 0};
    if(!camera)
        return 1;
    colfx.enable = colourFX->enable;
    colfx.u = uint32_t(colourFX->u);
    colfx.v = uint32_t(colourFX->v);
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &colfx.hdr));
}


/**
 * Set the rotation of the image
 * @param camera Pointer to camera component
 * @param rotation Degree of rotation (any number, but will be converted to 0,90,180 or 270 only)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_rotation(MMAL_COMPONENT_T *camera, int rotation) {
    uint32_t ret;
    int my_rotation = ((rotation % 360 ) / 90) * 90;
    ret = mmal_port_parameter_set_int32(camera->output[0], MMAL_PARAMETER_ROTATION, my_rotation);
    mmal_port_parameter_set_int32(camera->output[1], MMAL_PARAMETER_ROTATION, my_rotation);
    mmal_port_parameter_set_int32(camera->output[2], MMAL_PARAMETER_ROTATION, my_rotation);
    return mmal_status_to_int(MMAL_STATUS_T(ret));
}


/**
 * Set the flips state of the image
 * @param camera Pointer to camera component
 * @param hflip If true, horizontally flip the image
 * @param vflip If true, vertically flip the image
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip) {
    MMAL_PARAMETER_MIRROR_T mirror = {{MMAL_PARAMETER_MIRROR, sizeof(MMAL_PARAMETER_MIRROR_T)}, MMAL_PARAM_MIRROR_NONE};
    if(hflip && vflip)
        mirror.value = MMAL_PARAM_MIRROR_BOTH;
    else if(hflip)
        mirror.value = MMAL_PARAM_MIRROR_HORIZONTAL;
    else if(vflip)
        mirror.value = MMAL_PARAM_MIRROR_VERTICAL;
    mmal_port_parameter_set(camera->output[0], &mirror.hdr);
    mmal_port_parameter_set(camera->output[1], &mirror.hdr);
    return mmal_status_to_int(mmal_port_parameter_set(camera->output[2], &mirror.hdr));
}



/**
 * Set the ROI of the sensor to use for captures/preview
 * @param camera Pointer to camera component
 * @param rect   Normalised coordinates of ROI rectangle
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect) {
    MMAL_PARAMETER_INPUT_CROP_T crop;
    crop.hdr = {MMAL_PARAMETER_INPUT_CROP, sizeof(MMAL_PARAMETER_INPUT_CROP_T)};
    crop.rect.x = int32_t(65536.0 * rect.x);
    crop.rect.y = int32_t(65536.0 * rect.y);
    crop.rect.width = int32_t(65536.0 * rect.w);
    crop.rect.height = int32_t(65536.0 * rect.h);
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &crop.hdr));
}


/**
 * Adjust the exposure time used for images
 * @param camera Pointer to camera component
 * @param shutter speed in microseconds
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_shutter_speed(MMAL_COMPONENT_T *camera, int speed) {
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set_uint32(camera->control,
                                                             MMAL_PARAMETER_SHUTTER_SPEED,
                                                             uint32_t(speed)));
}


/**
 * Adjust the Dynamic range compression level
 * @param camera Pointer to camera component
 * @param strength Strength of DRC to apply
 *        MMAL_PARAMETER_DRC_STRENGTH_OFF
 *        MMAL_PARAMETER_DRC_STRENGTH_LOW
 *        MMAL_PARAMETER_DRC_STRENGTH_MEDIUM
 *        MMAL_PARAMETER_DRC_STRENGTH_HIGH
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_DRC(MMAL_COMPONENT_T *camera, MMAL_PARAMETER_DRC_STRENGTH_T strength) {
    MMAL_PARAMETER_DRC_T drc = {{MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION, sizeof(MMAL_PARAMETER_DRC_T)}, strength};
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &drc.hdr));
}


int
CameraControl::set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass) {
    if(!camera)
        return 1;
    return mmal_status_to_int(mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_CAPTURE_STATS_PASS, stats_pass));
}


/**
 * Set the annotate data
 * @param camera Pointer to camera component
 * @param Bitmask of required annotation data. 0 for off.
 * @param If set, a pointer to text string to use instead of bitmask, max length 32 characters
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int
CameraControl::set_annotate(MMAL_COMPONENT_T *camera, const int settings, const char *string,
                             const int text_size, const int text_colour, const int bg_colour,
                             const unsigned int justify, const unsigned int x, const unsigned int y)
{
    MMAL_PARAMETER_CAMERA_ANNOTATE_V4_T annotate;
    annotate.hdr.id = MMAL_PARAMETER_ANNOTATE;
    annotate.hdr.size = sizeof(MMAL_PARAMETER_CAMERA_ANNOTATE_V4_T);
    if(settings) {
        time_t t = time(nullptr);
        struct tm tm = *localtime(&t);
        char tmp[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V4];
        int process_datetime = 1;
        annotate.enable = 1;
        if(settings & (ANNOTATE_APP_TEXT | ANNOTATE_USER_TEXT)) {
            if((settings & (ANNOTATE_TIME_TEXT | ANNOTATE_DATE_TEXT)) && strchr(string,'%') != nullptr) {
                //string contains strftime parameter?
                strftime(annotate.text, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3, string, &tm );
                process_datetime = 0;
            }
            else {
                strncpy(annotate.text, string, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3);
            }
            annotate.text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3-1] = '\0';
        }
        if(process_datetime && (settings & ANNOTATE_TIME_TEXT)) {
            if(strlen(annotate.text)) {
                strftime(tmp, 32, " %X", &tm );
            }
            else {
                strftime(tmp, 32, "%X", &tm );
            }
            strncat(annotate.text, tmp, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 - strlen(annotate.text) - 1);
        }
        if(process_datetime && (settings & ANNOTATE_DATE_TEXT)) {
            if(strlen(annotate.text)) {
                strftime(tmp, 32, " %x", &tm );
            }
            else {
                strftime(tmp, 32, "%x", &tm );
            }
            strncat(annotate.text, tmp, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 - strlen(annotate.text) - 1);
        }
        if(settings & ANNOTATE_SHUTTER_SETTINGS)
            annotate.show_shutter = MMAL_TRUE;
        if(settings & ANNOTATE_GAIN_SETTINGS)
            annotate.show_analog_gain = MMAL_TRUE;
        if(settings & ANNOTATE_LENS_SETTINGS)
            annotate.show_lens = MMAL_TRUE;
        if(settings & ANNOTATE_CAF_SETTINGS)
            annotate.show_caf = MMAL_TRUE;
        if(settings & ANNOTATE_MOTION_SETTINGS)
            annotate.show_motion = MMAL_TRUE;
        if(settings & ANNOTATE_FRAME_NUMBER)
            annotate.show_frame_num = MMAL_TRUE;
        if(settings & ANNOTATE_BLACK_BACKGROUND)
            annotate.enable_text_background = MMAL_TRUE;
        annotate.text_size = uint8_t(text_size);
        if(text_colour != -1) {
            annotate.custom_text_colour = MMAL_TRUE;
            annotate.custom_text_Y = text_colour&0xff;
            annotate.custom_text_U = (text_colour>>8)&0xff;
            annotate.custom_text_V = (text_colour>>16)&0xff;
        }
        else
            annotate.custom_text_colour = MMAL_FALSE;

        if(bg_colour != -1) {
            annotate.custom_background_colour = MMAL_TRUE;
            annotate.custom_background_Y = bg_colour&0xff;
            annotate.custom_background_U = (bg_colour>>8)&0xff;
            annotate.custom_background_V = (bg_colour>>16)&0xff;
        }
        else
            annotate.custom_background_colour = MMAL_FALSE;

        annotate.justify = justify;
        annotate.x_offset = x;
        annotate.y_offset = y;
    }
    else
        annotate.enable = 0;
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &annotate.hdr));
}


int
CameraControl::set_gains(MMAL_COMPONENT_T *camera, float analog, float digital) {
    MMAL_RATIONAL_T rational = {0,65536};
    MMAL_STATUS_T status;
    if(!camera)
        return 1;
    rational.num = int32_t(analog * 65536);
    status = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_ANALOG_GAIN, rational);
    if(status != MMAL_SUCCESS)
        return mmal_status_to_int(status);
    rational.num = int32_t(digital * 65536);
    status = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_DIGITAL_GAIN, rational);
    return mmal_status_to_int(status);
}


/**
 * Zoom in and Zoom out by changing ROI
 * @param camera Pointer to camera component
 * @param zoom_command zoom command enum
 * @return 0 if successful, non-zero otherwise
 */
int
CameraControl::zoom_in_zoom_out(MMAL_COMPONENT_T *camera, ZOOM_COMMAND_T zoom_command, PARAM_FLOAT_RECT_T *roi) {
    MMAL_PARAMETER_INPUT_CROP_T crop;
    crop.hdr.id = MMAL_PARAMETER_INPUT_CROP;
    crop.hdr.size = sizeof(crop);
    if(mmal_port_parameter_get(camera->control, &crop.hdr) != MMAL_SUCCESS) {
        qDebug() << QString("mmal_port_parameter_get(camera->control, &crop.hdr) failed, skip it");
        return 0;
    }

    if(zoom_command == ZOOM_IN) {
        if(uint32_t(crop.rect.width) <= (zoom_full_16P16 + zoom_increment_16P16)) {
            crop.rect.width = zoom_full_16P16;
            crop.rect.height = zoom_full_16P16;
        }
        else {
            crop.rect.width -= zoom_increment_16P16;
            crop.rect.height -= zoom_increment_16P16;
        }
    }
    else if(zoom_command == ZOOM_OUT) {
        unsigned int increased_size = (uint32_t(crop.rect.width) + zoom_increment_16P16);
        if(increased_size < uint32_t(crop.rect.width)) {//overflow
            crop.rect.width = 65536;
            crop.rect.height = 65536;
        }
        else {
            crop.rect.width = int32_t(increased_size);
            crop.rect.height = int32_t(increased_size);
        }
    }
    if(zoom_command == ZOOM_RESET) {
        crop.rect.x = 0;
        crop.rect.y = 0;
        crop.rect.width = 65536;
        crop.rect.height = 65536;
    }
    else {
        unsigned int centered_top_coordinate = (65536 - uint32_t(crop.rect.width)) / 2;
        crop.rect.x = int32_t(centered_top_coordinate);
        crop.rect.y = int32_t(centered_top_coordinate);
    }
    int ret = mmal_status_to_int(mmal_port_parameter_set(camera->control, &crop.hdr));
    if(ret == 0) {
        roi->x = roi->y = double(crop.rect.x)/65536.0;
        roi->w = roi->h = double(crop.rect.width)/65536.0;
    }
    else {
        qDebug() << QString("Failed to set crop values, x/y: %1, w/h: %2")
                    .arg(crop.rect.x)
                    .arg(crop.rect.width);
        ret = 1;
    }
    return ret;
}


int
CameraControl::set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode) {
    MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo = { {MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof(stereo)},
                                                  MMAL_STEREOSCOPIC_MODE_NONE, MMAL_FALSE, MMAL_FALSE
                                                };
    if(stereo_mode->mode != MMAL_STEREOSCOPIC_MODE_NONE) {
        stereo.mode = stereo_mode->mode;
        stereo.decimate = stereo_mode->decimate;
        stereo.swap_eyes = stereo_mode->swap_eyes;
    }
    return mmal_status_to_int(mmal_port_parameter_set(port, &stereo.hdr));
}


