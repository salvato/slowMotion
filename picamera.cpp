#include "picamera.h"
#include "bcm_host.h"
#include <QDebug>

#define verbose true


#define MY_VCOS_ALIGN_DOWN(p,n) ((reinterpret_cast<ptrdiff_t>(p)) & ~((n)-1))
#define MY_VCOS_ALIGN_UP(p,n) MY_VCOS_ALIGN_DOWN(reinterpret_cast<ptrdiff_t>((p)+(n)-1),(n))


/** Default camera callback function
 * Handles the --settings
 * @param port
 * @param Callback data
 */
void
default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    qDebug() << QString("Camera control callback  cmd=0x%1x\nPort:%2")
                .arg(buffer->cmd, 8, 16, QLatin1Char('0'))
                .arg(port->name);
    if(buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
        MMAL_EVENT_PARAMETER_CHANGED_T *param = reinterpret_cast<MMAL_EVENT_PARAMETER_CHANGED_T*>(buffer->data);
        switch (param->hdr.id) {
            case MMAL_PARAMETER_CAMERA_SETTINGS: {
                MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = reinterpret_cast<MMAL_PARAMETER_CAMERA_SETTINGS_T*>(param);
                qDebug() << QString("Exposure now %1, analog gain %2/%3, digital gain %4/%5")
                               .arg(settings->exposure)
                               .arg(settings->analog_gain.num)
                               .arg(settings->analog_gain.den)
                               .arg(settings->digital_gain.num)
                               .arg(settings->digital_gain.den);
                qDebug() << QString("AWB R=%1/%2, B=%3/%4")
                               .arg(settings->awb_red_gain.num)
                               .arg(settings->awb_red_gain.den)
                               .arg(settings->awb_blue_gain.num)
                               .arg(settings->awb_blue_gain.den);
            }
            break;
        }
    }
    else if(buffer->cmd == MMAL_EVENT_ERROR) {
        qDebug() << QString("No data received from sensor. Check all connections, including the Sunny one on the camera board");
    }
    else {
        qDebug() << QString("Received unexpected camera control callback event, 0x%1x")
                    .arg(uint(buffer->cmd), 8, 16, QLatin1Char('0'));
    }
    mmal_buffer_header_release(buffer);
}


PiCamera::PiCamera()
    : cameraComponent(nullptr)
    , previewConnection(nullptr)
    , pool(nullptr)
{
}


MMAL_STATUS_T
PiCamera::createComponent(int cameraNum, int sensorMode) {
    MMAL_STATUS_T status;
    // Create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &cameraComponent);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Failed to create camera component");
        if(cameraComponent)
            mmal_component_destroy(cameraComponent);
        return status;
    }
    MMAL_PARAMETER_INT32_T camera_num = {
                                            {MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)},
                                            cameraNum
                                        };
    status = mmal_port_parameter_set(cameraComponent->control, &camera_num.hdr);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not select camera : error %1").arg(status);
        if(cameraComponent)
            mmal_component_destroy(cameraComponent);
        return status;
    }
    if(!cameraComponent->output_num) {
        status = MMAL_ENOSYS;
        qDebug() << QString("Camera doesn't have output ports");
        if(cameraComponent)
            mmal_component_destroy(cameraComponent);
        return status;
    }
    status = mmal_port_parameter_set_uint32(cameraComponent->control,
                                            MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG,
                                            uint32_t(sensorMode));
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not set sensor mode : error %1").arg(status);
        if(cameraComponent)
            mmal_component_destroy(cameraComponent);
    }
    return status;
}


PiCamera::~PiCamera() {
    if(cameraComponent) {
        MMAL_PORT_T *still_port = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
        if(pool)
            mmal_port_pool_destroy(still_port, pool);
        mmal_component_destroy(cameraComponent);
    }
    cameraComponent = nullptr;
    pool = nullptr;
}


void
PiCamera::destroyComponent() {
    if(cameraComponent) {
        MMAL_PORT_T *still_port = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
        if(pool)
            mmal_port_pool_destroy(still_port, pool);
        mmal_component_destroy(cameraComponent);
    }
    cameraComponent = nullptr;
    pool = nullptr;
}


MMAL_STATUS_T
PiCamera::setCallback() {
    MMAL_STATUS_T status;
    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(cameraComponent->control, default_camera_control_callback);
    if(status != MMAL_SUCCESS ){
        qDebug() << QString("Unable to enable control port : error %1").arg(status);
        mmal_component_destroy(cameraComponent);
    }
    return status;
}


MMAL_STATUS_T
PiCamera::setConfig(MMAL_PARAMETER_CAMERA_CONFIG_T* pCam_config) {
    MMAL_STATUS_T status;
    status = mmal_port_parameter_set(cameraComponent->control, &pCam_config->hdr);
    return status;
}


/**
 * Set the specified camera to all the specified settings
 * @param params Pointer to parameter block containing parameters
 * @return 0 if successful, none-zero if unsuccessful.
 */
int
PiCamera::setAllParameters() {
    int result;
    RASPICAM_CAMERA_PARAMETERS params =cameraControl.cameraParameters;
    result  = cameraControl.set_saturation(cameraComponent, params.saturation);
    result += cameraControl.set_sharpness(cameraComponent, params.sharpness);
    result += cameraControl.set_contrast(cameraComponent, params.contrast);
    result += cameraControl.set_brightness(cameraComponent, params.brightness);
    result += cameraControl.set_ISO(cameraComponent, params.ISO);
    result += cameraControl.set_video_stabilisation(cameraComponent, params.videoStabilisation);
    result += cameraControl.set_exposure_compensation(cameraComponent, params.exposureCompensation);
    result += cameraControl.set_exposure_mode(cameraComponent, params.exposureMode);
    result += cameraControl.set_flicker_avoid_mode(cameraComponent, params.flickerAvoidMode);
    result += cameraControl.set_metering_mode(cameraComponent, params.exposureMeterMode);
    result += cameraControl.set_awb_mode(cameraComponent, params.awbMode);
    result += cameraControl.set_awb_gains(cameraComponent, params.awb_gains_r, params.awb_gains_b);
    result += cameraControl.set_imageFX(cameraComponent, params.imageEffect);
    result += cameraControl.set_colourFX(cameraComponent, &params.colourEffects);
    //result += cameraControl.set_thumbnail_parameters(cameraComponent, &params.thumbnailConfig);  TODO Not working for some reason
    result += cameraControl.set_rotation(cameraComponent, params.rotation);
    result += cameraControl.set_flips(cameraComponent, params.hflip, params.vflip);
    result += cameraControl.set_ROI(cameraComponent, params.roi);
    result += cameraControl.set_shutter_speed(cameraComponent, params.shutter_speed);
    result += cameraControl.set_DRC(cameraComponent, params.drc_level);
    result += cameraControl.set_stats_pass(cameraComponent, params.stats_pass);
    result += cameraControl.set_annotate(cameraComponent, params.enable_annotate, params.annotate_string,
                                         params.annotate_text_size,
                                         params.annotate_text_colour,
                                         params.annotate_bg_colour,
                                         params.annotate_justify,
                                         params.annotate_x,
                                         params.annotate_y);
    result += cameraControl.set_gains(cameraComponent, params.analog_gain, params.digital_gain);
    if(params.settings) {
        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request = {
            {MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
            MMAL_PARAMETER_CAMERA_SETTINGS, 1
        };
        MMAL_STATUS_T status = mmal_port_parameter_set(cameraComponent->control, &change_event_request.hdr);
        if(status != MMAL_SUCCESS) {
            qDebug() << QString("No camera settings events");
        }
        result += status;
    }
    return result;
}


MMAL_STATUS_T
PiCamera::setPortFormats(const RASPICAM_CAMERA_PARAMETERS *camera_parameters,
                         bool fullResPreview,
                         MMAL_FOURCC_T encoding,
                         int width,
                         int height)
{
    // Utility variables
    MMAL_STATUS_T status;
    MMAL_PORT_T *previewPort = cameraComponent->output[MMAL_CAMERA_PREVIEW_PORT];
    MMAL_PORT_T *videoPort   = cameraComponent->output[MMAL_CAMERA_VIDEO_PORT];
    MMAL_PORT_T *stillPort   = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
    MMAL_ES_FORMAT_T *format;

// Set up the port formats starting from the Preview Port
    format = previewPort->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    if(camera_parameters->shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 },
                                                {166, 1000}
                                               };
        mmal_port_parameter_set(previewPort, &fps_range.hdr);
    }
    else if(camera_parameters->shutter_speed > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 166, 1000 },
                                                {999, 1000}
                                               };
        mmal_port_parameter_set(previewPort, &fps_range.hdr);
    }
    format->es->video.width = uint32_t(MY_VCOS_ALIGN_UP(width, 32));
    format->es->video.height = uint32_t(MY_VCOS_ALIGN_UP(height, 16));
    format->es->video.crop = MMAL_RECT_T {0, 0, width, height};
    if(fullResPreview) {
        // In this mode we are forcing the preview to be generated from the full capture resolution.
        // This runs at a max of 15fps with the OV5647 sensor.
        format->es->video.frame_rate.num = Preview::FULL_RES_PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = Preview::FULL_RES_PREVIEW_FRAME_RATE_DEN;
    }
    else {
        // Use a full FOV 4:3 mode
        format->es->video.frame_rate.num = Preview::PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = Preview::PREVIEW_FRAME_RATE_DEN;
    }
    status = mmal_port_format_commit(previewPort);
    if(status != MMAL_SUCCESS ) {
        qDebug() << QString("camera viewfinder format couldn't be set");
        mmal_component_destroy(cameraComponent);
        return status;
    }

// Now set up the Video Port (which we don't use) with the same format
    mmal_format_full_copy(videoPort->format, format);
    status = mmal_port_format_commit(videoPort);
    if(status != MMAL_SUCCESS ) {
        qDebug() << QString("camera video format couldn't be set");
        mmal_component_destroy(cameraComponent);
        return status;
    }
// Ensure there are enough buffers to avoid dropping frames
    if(videoPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        videoPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

// Now set up the Still Port
    format = stillPort->format;
    if(camera_parameters->shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 }, {166, 1000}
                                               };
        mmal_port_parameter_set(stillPort, &fps_range.hdr);
    }
    else if(camera_parameters->shutter_speed > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 167, 1000 },
                                                {999, 1000}
                                               };
        mmal_port_parameter_set(stillPort, &fps_range.hdr);
    }
// Set our format on the Stills Port
    if(encoding) {
        format->encoding = encoding;
        if(!mmal_util_rgb_order_fixed(stillPort)) {
            if(format->encoding == MMAL_ENCODING_RGB24)
                format->encoding = MMAL_ENCODING_BGR24;
            else if(format->encoding == MMAL_ENCODING_BGR24)
                format->encoding = MMAL_ENCODING_RGB24;
        }
        format->encoding_variant = 0;  //Irrelevant when not in opaque mode
    }
    else {
        format->encoding = MMAL_ENCODING_I420;
        format->encoding_variant = MMAL_ENCODING_I420;
    }
    format->es->video.width = uint32_t(MY_VCOS_ALIGN_UP(width, 32));
    format->es->video.height = uint32_t(MY_VCOS_ALIGN_UP(height, 16));
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = width;
    format->es->video.crop.height = height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;
    if(stillPort->buffer_size < stillPort->buffer_size_min)
        stillPort->buffer_size = stillPort->buffer_size_min;
    stillPort->buffer_num = stillPort->buffer_num_recommended;
    status = mmal_port_parameter_set_boolean(videoPort, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Failed to select zero copy");
        mmal_component_destroy(cameraComponent);
        return status;
    }
    status = mmal_port_format_commit(stillPort);
    if(status != MMAL_SUCCESS ) {
        qDebug() << QString("camera still format couldn't be set");
        mmal_component_destroy(cameraComponent);
        return status;
    }

    return MMAL_SUCCESS;
}


MMAL_STATUS_T
PiCamera::enableCamera() {
    // Enable component
    MMAL_STATUS_T status = mmal_component_enable(cameraComponent);
    if(status != MMAL_SUCCESS ) {
        qDebug() << QString("camera component couldn't be enabled");
        mmal_component_destroy(cameraComponent);
    }
    return status;
}


void
PiCamera::createBufferPool() {
    // Create pool of buffer headers for the output port to consume
    // Utility variables
    MMAL_PORT_T *still_port = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
    pool = mmal_port_pool_create(still_port, still_port->buffer_num, still_port->buffer_size);
    if(!pool) {
        qDebug() << QString("Failed to create buffer header pool for camera still port %1")
                    .arg(still_port->name);
    }
    if(verbose)
        qDebug() << "Camera component done";
}


MMAL_STATUS_T
PiCamera::start(Preview *pPreview) {
// Note we are lucky that the preview and null sink components use the same
// input port so we can simple do this without conditionals
    MMAL_STATUS_T status;
    MMAL_PORT_T *preview_input_port  = pPreview->previewComponent->input[0];
    MMAL_PORT_T *previewPort = cameraComponent->output[MMAL_CAMERA_PREVIEW_PORT];
// Connect camera to preview (which might be a null_sink if no preview required)
    status = connectPorts(previewPort, preview_input_port, &previewConnection);
    if(status != MMAL_SUCCESS) {
        cameraControl.mmal_status_to_int(status);
        qDebug() << QString("%1: Failed to connect camera to preview").arg(__func__);
        handleError(status, pPreview);
        return status;
    }
    return MMAL_SUCCESS;
}


/// Connect two specific ports together
/// output_port Pointer the output port
/// input_port Pointer the input port
/// connection Pointer to a mmal connection pointer, reassigned if function successful
/// Returns a MMAL_STATUS_T giving result of operation
MMAL_STATUS_T
PiCamera::connectPorts(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection) {
   MMAL_STATUS_T status;
   status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING |
                                                                         MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
   if (status == MMAL_SUCCESS) {
      status =  mmal_connection_enable(*connection);
      if (status != MMAL_SUCCESS)
         mmal_connection_destroy(*connection);
   }
   return status;
}


void
PiCamera::handleError(MMAL_STATUS_T status, Preview *pPreview) {
    cameraControl.mmal_status_to_int(status);
    if(verbose)
        qDebug() << "Closing down";
    // Disable all our ports that are not handled by connections
    MMAL_PORT_T *camera_video_port = cameraComponent->output[MMAL_CAMERA_VIDEO_PORT];
    checkDisablePort(camera_video_port);
    if(previewConnection)
        mmal_connection_destroy(previewConnection);
    // Disable components
    if(pPreview->previewComponent)
        mmal_component_disable(pPreview->previewComponent);
    if(cameraComponent)
        mmal_component_disable(cameraComponent);
    pPreview->destroy();
    destroyComponent();
    if(verbose)
        qDebug() << "Close down completed, all components disconnected, disabled and destroyed";
    cameraControl.checkConfiguration(128);
}


/// Checks if specified port is valid and enabled, then disables it
/// port  Pointer the port
void
PiCamera::checkDisablePort(MMAL_PORT_T *port) {
   if(port && port->is_enabled)
      mmal_port_disable(port);
}


void
PiCamera::stop() {
MMAL_PORT_T *camera_video_port = cameraComponent->output[MMAL_CAMERA_VIDEO_PORT];
    checkDisablePort(camera_video_port);
    if(previewConnection)
        mmal_connection_destroy(previewConnection);
}

