#include "picamera.h"
#include "bcm_host.h"
#include <QDebug>

#define verbose true


#define MY_VCOS_ALIGN_DOWN(p,n) ((reinterpret_cast<ptrdiff_t>(p)) & ~((n)-1))
#define MY_VCOS_ALIGN_UP(p,n) MY_VCOS_ALIGN_DOWN(reinterpret_cast<ptrdiff_t>((p)+(n)-1),(n))


// Struct used to pass information in camera still port userdata to callback
typedef struct {
    FILE *file_handle;                   /// File handle to write buffer data to.
    VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
    PiCamera *pCamera;                   /// pointer to our camera in case required in callback
} PORT_USERDATA;


static PORT_USERDATA callbackData;


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


/// buffer header callback function for camera output port
/// Callback will dump buffer data to the specific file
/// port Pointer to port from which callback originated
/// buffer mmal buffer header pointer
static void
cameraBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    int complete = 0;
    // We pass our file handle and other stuff in via the userdata field.
    PORT_USERDATA *pData = reinterpret_cast<PORT_USERDATA *>(port->userdata);
    if(pData) {
        uint bytes_written = 0;
        uint bytes_to_write = buffer->length;
        if(pData->pCamera->cameraControl.onlyLuma)
            bytes_to_write = vcos_min(buffer->length, port->format->es->video.width * port->format->es->video.height);
        if(bytes_to_write && pData->file_handle) {
            mmal_buffer_header_mem_lock(buffer);
            bytes_written = fwrite(buffer->data, 1, bytes_to_write, pData->file_handle);
            mmal_buffer_header_mem_unlock(buffer);
        }
        // We need to check we wrote what we wanted - it's possible we have run out of storage.
        if(buffer->length && bytes_written != bytes_to_write) {
            qDebug() << QString("Unable to write buffer to file - aborting %1 vs %2")
                        .arg(bytes_written)
                        .arg(bytes_to_write);
            complete = 1;
        }
        // Check end of frame or error
        if(buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END |
                            MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED))
            complete = 1;
    }
    else{
        qDebug() << QString("Received a camera still buffer callback with no state");
    }
    // release buffer back to the pool
    mmal_buffer_header_release(buffer);
    // and send one back to the port (if still open)
    if(port->is_enabled) {
        MMAL_STATUS_T status = MMAL_SUCCESS;
        MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(pData->pCamera->pool->queue);
        // and back to the port from there.
        if(new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if(!new_buffer || status != MMAL_SUCCESS)
            qDebug() << QString("Unable to return the buffer to the camera still port");
    }
    if(complete) {
        vcos_semaphore_post(&(pData->complete_semaphore));
    }
}


PiCamera::PiCamera()
    : cameraComponent(nullptr)
    , pool(nullptr)
    , previewConnection(nullptr)
{
    VCOS_STATUS_T vcos_status;
    vcos_status = vcos_semaphore_create(&callbackData.complete_semaphore, "RaspiStill-sem", 0);
    vcos_assert(vcos_status == VCOS_SUCCESS);
}


MMAL_STATUS_T
PiCamera::createComponent(int cameraNum, int sensorMode) {
    MMAL_STATUS_T status;
    // Create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &cameraComponent);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Failed to create camera component");
        if(cameraComponent) {
            mmal_component_destroy(cameraComponent);
            cameraComponent = nullptr;
        }
        return status;
    }
    MMAL_PARAMETER_INT32_T camera_num = {
                                            {MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)},
                                            cameraNum
                                        };
    status = mmal_port_parameter_set(cameraComponent->control, &camera_num.hdr);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not select camera : error %1").arg(status);
        if(cameraComponent) {
            mmal_component_destroy(cameraComponent);
            cameraComponent = nullptr;
        }
        return status;
    }
    if(!cameraComponent->output_num) {
        status = MMAL_ENOSYS;
        qDebug() << QString("Camera doesn't have output ports");
        if(cameraComponent) {
            mmal_component_destroy(cameraComponent);
            cameraComponent = nullptr;
        }
        return status;
    }
    status = mmal_port_parameter_set_uint32(cameraComponent->control,
                                            MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG,
                                            uint32_t(sensorMode));
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not set sensor mode : error %1").arg(status);
        if(cameraComponent) {
            mmal_component_destroy(cameraComponent);
            cameraComponent = nullptr;
        }
    }
    return status;
}


PiCamera::~PiCamera() {
    destroyComponent();
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
        destroyComponent();
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
    result  = cameraControl.set_saturation(cameraComponent, cameraControl.saturation);
    result += cameraControl.set_sharpness(cameraComponent, cameraControl.sharpness);
    result += cameraControl.set_contrast(cameraComponent, cameraControl.contrast);
    result += cameraControl.set_brightness(cameraComponent, cameraControl.brightness);
    result += cameraControl.set_ISO(cameraComponent, cameraControl.ISO);
    result += cameraControl.set_video_stabilisation(cameraComponent, cameraControl.videoStabilisation);
    result += cameraControl.set_exposure_compensation(cameraComponent, cameraControl.exposureCompensation);
    result += cameraControl.set_exposure_mode(cameraComponent, cameraControl.exposureMode);
    result += cameraControl.set_flicker_avoid_mode(cameraComponent, cameraControl.flickerAvoidMode);
    result += cameraControl.set_metering_mode(cameraComponent, cameraControl.exposureMeterMode);
    result += cameraControl.set_awb_mode(cameraComponent, cameraControl.awbMode);
    result += cameraControl.set_awb_gains(cameraComponent, cameraControl.awb_gains_r, cameraControl.awb_gains_b);
    result += cameraControl.set_imageFX(cameraComponent, cameraControl.imageEffect);
    result += cameraControl.set_colourFX(cameraComponent, &cameraControl.colourEffects);
    //result += cameraControl.set_thumbnail_parameters(cameraComponent, &cameraControl.thumbnailConfig);  TODO Not working for some reason
    result += cameraControl.set_rotation(cameraComponent, cameraControl.rotation);
    result += cameraControl.set_flips(cameraComponent, cameraControl.hflip, cameraControl.vflip);
    result += cameraControl.set_ROI(cameraComponent, cameraControl.roi);
    result += cameraControl.set_shutter_speed(cameraComponent, cameraControl.shutter_speed);
    result += cameraControl.set_DRC(cameraComponent, cameraControl.drc_level);
    result += cameraControl.set_stats_pass(cameraComponent, cameraControl.stats_pass);
    result += cameraControl.set_annotate(cameraComponent, cameraControl.enable_annotate, cameraControl.annotate_string,
                                         cameraControl.annotate_text_size,
                                         cameraControl.annotate_text_colour,
                                         cameraControl.annotate_bg_colour,
                                         cameraControl.annotate_justify,
                                         cameraControl.annotate_x,
                                         cameraControl.annotate_y);
    result += cameraControl.set_gains(cameraComponent, cameraControl.analog_gain, cameraControl.digital_gain);
    if(cameraControl.settings) {
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
PiCamera::setPortFormats(bool fullResPreview,
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
    if(cameraControl.shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 },
                                                {166, 1000}
                                               };
        mmal_port_parameter_set(previewPort, &fps_range.hdr);
    }
    else if(cameraControl.shutter_speed > 1000000) {
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
    if(cameraControl.shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 }, {166, 1000}
                                               };
        mmal_port_parameter_set(stillPort, &fps_range.hdr);
    }
    else if(cameraControl.shutter_speed > 1000000) {
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
    qDebug() << "still_port buffer size  :" << still_port->buffer_size;
    qDebug() << "still_port buffer number:" << still_port->buffer_num;
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
    MMAL_PORT_T* camera_still_port   = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
    // Set up our userdata:
    // this is passed through to the callback where we need the information.
    callbackData.file_handle = nullptr;    // Null until we open our filename
    callbackData.pCamera = this;
    camera_still_port->userdata = reinterpret_cast<struct MMAL_PORT_USERDATA_T *>(&callbackData);
    if(verbose)
        qDebug() << QString("Enabling camera still output port");
    // Enable the camera still output port and tell it its callback function
    status = mmal_port_enable(camera_still_port, cameraBufferCallback);
    if (status != MMAL_SUCCESS) {
        qDebug() << QString("Failed to setup camera output");
        handleError(status, pPreview);
    }
/*
    int frame, keep_looping = 1;
    FILE *output_file = nullptr;
    char *use_filename = nullptr;      // Temporary filename while image being written
    char *final_filename = nullptr;    // Name that file gets once writing complete
    frame = 0;
    while(keep_looping) {
        keep_looping = wait_for_next_frame(&state, &frame);
        // Open the file
        if(state.common_settings.filename) {
            if (state.common_settings.filename[0] == '-') {
                output_file = stdout;
                // Ensure we don't upset the output stream with diagnostics/info
                state.common_settings.verbose = 0;
            }
            else {
                vcos_assert(use_filename == NULL && final_filename == NULL);
                status = create_filenames(&final_filename, &use_filename, state.common_settings.filename, frame);
                if (status  != MMAL_SUCCESS) {
                    qDebug() << QString("Unable to create filenames");
                    goto error;
                }
                if (state.common_settings.verbose)
                    qDebug() << QString("Opening output file %1").arg(final_filename);
                // Technically it is opening the temp~ filename which will be renamed
                // to the final filename
                output_file = fopen(use_filename, "wb");
                if (!output_file) {
                    // Notify user, carry on but discarding encoded output buffers
                    qDebug() << QString("%1: Error opening output file: %2\nNo output file will be generated")
                                .arg(__func__)
                                .arg(use_filename);
                }
            }
            callback_data.file_handle = output_file;
        }
        if(output_file) {
            uint num, q;
            // There is a possibility that shutter needs to be set each loop.
            if (mmal_status_to_int(mmal_port_parameter_set_uint32(state.camera_component->control,
                                                                  MMAL_PARAMETER_SHUTTER_SPEED,
                                                                  (uint32_t)state.camera_parameters.shutter_speed) != MMAL_SUCCESS))
                qDebug() << QString("Unable to set shutter speed");
            // Send all the buffers to the camera output port
            num = mmal_queue_length(state.camera_pool->queue);
            for (q=0; q<num; q++) {
                MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.camera_pool->queue);
                if (!buffer)
                    qDebug() << QString("Unable to get a required buffer %d from pool queue", q);
                if (mmal_port_send_buffer(camera_still_port, buffer)!= MMAL_SUCCESS)
                    qDebug() << QString("Unable to send a buffer to camera output port (%d)", q);
            }
            if (state.burstCaptureMode && frame==1) {
                mmal_port_parameter_set_boolean(state.camera_component->control,  MMAL_PARAMETER_CAMERA_BURST_CAPTURE, 1);
            }
            if(state.camera_parameters.enable_annotate) {
                if (state.camera_parameters.enable_annotate & ANNOTATE_APP_TEXT)
                    raspicamcontrol_set_annotate(state.camera_component,
                                                 state.camera_parameters.enable_annotate,
                                                 state.camera_parameters.annotate_string,
                                                 state.camera_parameters.annotate_text_size,
                                                 state.camera_parameters.annotate_text_colour,
                                                 state.camera_parameters.annotate_bg_colour,
                                                 state.camera_parameters.annotate_justify,
                                                 state.camera_parameters.annotate_x,
                                                 state.camera_parameters.annotate_y
                                                 );
            }
            if (state.common_settings.verbose)
                qDebug() << QString("Starting capture %d").arg(frame);
            if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS) {
                qDebug() << QString("%1: Failed to start capture").arg(__func__);
            }
            else {
                // Wait for capture to complete
                // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
                // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
                vcos_semaphore_wait(&callback_data.complete_semaphore);
                if (state.common_settings.verbose)
                    qDebug() << QString("Finished capture %1").arg(frame);
            }
            // Ensure we don't die if get callback with no open file
            callback_data.file_handle = nullptr;
            if (output_file != stdout) {
                rename_file(&state, output_file, final_filename, use_filename, frame);
            }
            else {
                fflush(output_file);
            }
        }
        if (use_filename) {
            free(use_filename);
            use_filename = nullptr;
        }
        if (final_filename) {
            free(final_filename);
            final_filename = nullptr;
        }
    } // end for (frame)
    vcos_semaphore_delete(&callback_data.complete_semaphore);
}
*/
    return MMAL_SUCCESS;
}


/// Connect two specific ports together
/// @param output_port Pointer the output port
/// @param input_port Pointer the input port
/// @param connection Pointer to a mmal connection pointer, reassigned if function successful
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
    MMAL_PORT_T* cameraStillPort = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
    checkDisablePort(cameraStillPort);
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
/// @param port Pointer the port
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
    previewConnection = nullptr;
    MMAL_PORT_T* camera_still_port = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
    if(verbose)
        qDebug() << QString("Disabling camera still output port");
    // Disable the camera still output port
    checkDisablePort(camera_still_port);
}


void
PiCamera::capture(QString sPathName) {
    FILE *output_file = fopen(sPathName.toLatin1(), "wb");
    if (!output_file) {
// Notify user, carry on but discarding encoded output buffers
        qDebug() << QString("%1: Error opening output file: %2\nNo output file will be generated")
                    .arg(__func__)
                    .arg(sPathName);
    }
    else {
        qDebug() << "Writing" << sPathName;
    }
    callbackData.file_handle = output_file;
    MMAL_PORT_T* cameraStillPort = cameraComponent->output[MMAL_CAMERA_CAPTURE_PORT];
// Send all the buffers to the camera output port
    uint num = mmal_queue_length(pool->queue);
    for(uint q=0; q<num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
        if(!buffer)
            qDebug() << QString("Unable to get a required buffer %1 from pool queue").arg(q);
        if(mmal_port_send_buffer(cameraStillPort, buffer)!= MMAL_SUCCESS)
            qDebug() << QString("Unable to send a buffer to camera output port (%1)").arg(q);
    }
    if (verbose)
        qDebug() << QString("Starting capture...");
    if (mmal_port_parameter_set_boolean(cameraStillPort, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS) {
        qDebug() << QString("%1: Failed to start capture").arg(__func__);
    }
    else {
// Wait for capture to complete
        vcos_semaphore_wait(&callbackData.complete_semaphore);
        if(verbose)
            qDebug() << QString("Capture Done !");
    }
    fflush(output_file);
}


// *
// * Create the encoder component, set up its ports
// *
// * @param state Pointer to state control struct. encoder_component member set to the created camera_component if successful.
// *
// * @return a MMAL_STATUS, MMAL_SUCCESS if all OK, something else otherwise

//static
//MMAL_STATUS_T create_encoder_component(RASPISTILL_STATE *state) {
//   MMAL_COMPONENT_T *encoder = nullptr;
//   MMAL_PORT_T *encoder_input = nullptr;
//   MMAL_PORT_T *encoder_output = nullptr;
//   MMAL_STATUS_T status;
//   MMAL_POOL_T *pool;

//   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &encoder);

//   if (status != MMAL_SUCCESS) {
//      vcos_log_error("Unable to create JPEG encoder component");
//      goto error;
//   }

//   if (!encoder->input_num || !encoder->output_num) {
//      status = MMAL_ENOSYS;
//      vcos_log_error("JPEG encoder doesn't have input/output ports");
//      goto error;
//   }

//   encoder_input = encoder->input[0];
//   encoder_output = encoder->output[0];

//   // We want same format on input and output
//   mmal_format_copy(encoder_output->format, encoder_input->format);

//   // Specify out output format
//   encoder_output->format->encoding = state->encoding;

//   encoder_output->buffer_size = encoder_output->buffer_size_recommended;

//   if (encoder_output->buffer_size < encoder_output->buffer_size_min)
//      encoder_output->buffer_size = encoder_output->buffer_size_min;

//   encoder_output->buffer_num = encoder_output->buffer_num_recommended;

//   if (encoder_output->buffer_num < encoder_output->buffer_num_min)
//      encoder_output->buffer_num = encoder_output->buffer_num_min;

//   // Commit the port changes to the output port
//   status = mmal_port_format_commit(encoder_output);

//   if (status != MMAL_SUCCESS) {
//      vcos_log_error("Unable to set format on video encoder output port");
//      goto error;
//   }

//   // Set the JPEG quality level
//   status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_Q_FACTOR, state->quality);

//   if (status != MMAL_SUCCESS) {
//      vcos_log_error("Unable to set JPEG quality");
//      goto error;
//   }

//   // Set the JPEG restart interval
//   status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_RESTART_INTERVAL, state->restart_interval);

//   if (state->restart_interval && status != MMAL_SUCCESS) {
//      vcos_log_error("Unable to set JPEG restart interval");
//      goto error;
//   }

//   // Set up any required thumbnail
//   {
//      MMAL_PARAMETER_THUMBNAIL_CONFIG_T param_thumb = {{MMAL_PARAMETER_THUMBNAIL_CONFIGURATION, sizeof(MMAL_PARAMETER_THUMBNAIL_CONFIG_T)}, 0, 0, 0, 0};

//      if(state->thumbnailConfig.enable &&
//         state->thumbnailConfig.width > 0 &&
//         state->thumbnailConfig.height > 0)
//      {
//         // Have a valid thumbnail defined
//         param_thumb.enable = 1;
//         param_thumb.width = state->thumbnailConfig.width;
//         param_thumb.height = state->thumbnailConfig.height;
//         param_thumb.quality = state->thumbnailConfig.quality;
//      }
//      status = mmal_port_parameter_set(encoder->control, &param_thumb.hdr);
//   }

//   //  Enable component
//   status = mmal_component_enable(encoder);

//   if (status  != MMAL_SUCCESS) {
//      vcos_log_error("Unable to enable video encoder component");
//      goto error;
//   }

//   /* Create pool of buffer headers for the output port to consume */
//   pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

//   if (!pool) {
//      vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
//   }

//   state->encoder_pool = pool;
//   state->encoder_component = encoder;

//   if (state->common_settings.verbose)
//      fprintf(stderr, "Encoder component done\n");

//   return status;

//error:

//   if (encoder)
//      mmal_component_destroy(encoder);

//   return status;
//}


// *
// * Destroy the encoder component
// *
// * @param state Pointer to state control struct
// *

//static void
//destroy_encoder_component(RASPISTILL_STATE *state) {
//   // Get rid of any port buffers first
//   if (state->encoder_pool) {
//      mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);
//   }

//   if (state->encoder_component) {
//      mmal_component_destroy(state->encoder_component);
//      state->encoder_component = NULL;
//   }
//}

