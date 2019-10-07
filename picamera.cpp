#include "picamera.h"
#include "utility.h"
#include "bcm_host.h"
#include <QDebug>


#define MY_VCOS_ALIGN_DOWN(p,n) ((reinterpret_cast<ptrdiff_t>(p)) & ~((n)-1))
#define MY_VCOS_ALIGN_UP(p,n) MY_VCOS_ALIGN_DOWN(reinterpret_cast<ptrdiff_t>((p)+(n)-1),(n))


// Struct used to pass information in camera still port userdata to callback
typedef struct {
    FILE *file_handle;                   /// File handle to write buffer data to.
    VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
    void *pSource;                       /// pointer to our camera in case required in callback
} PORT_USERDATA;


static PORT_USERDATA callbackData;


/**
 *  buffer header callback function for encoder
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void
encoderBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
   int complete = 0;
   // We pass our file handle and other stuff in via the userdata field.
   PORT_USERDATA *pData = reinterpret_cast<PORT_USERDATA *>(port->userdata);
   if(pData) {
      uint32_t bytes_written = buffer->length;
      if(buffer->length && pData->file_handle) {
         mmal_buffer_header_mem_lock(buffer);
         bytes_written = fwrite(buffer->data, 1, buffer->length, pData->file_handle);
         mmal_buffer_header_mem_unlock(buffer);
      }
      // We need to check we wrote what we wanted - it's possible we have run out of storage.
      if(bytes_written != buffer->length) {
         qDebug() << QString("Unable to write buffer to file - aborting");
         complete = 1;
      }
      // Now flag if we have completed
      if(buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END |
                          MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED))
         complete = 1;
   }
   else {
      qDebug() << QString("Received a encoder buffer callback with no state");
   }
   // release buffer back to the pool
   mmal_buffer_header_release(buffer);
   // and send one back to the port (if still open)
   if(port->is_enabled) {
      MMAL_STATUS_T status = MMAL_SUCCESS;
      JpegEncoder* pEncoder = reinterpret_cast<JpegEncoder*>(pData->pSource);
      MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(pEncoder->pool->queue);
      if(new_buffer) {
         status = mmal_port_send_buffer(port, new_buffer);
      }
      if(!new_buffer || status != MMAL_SUCCESS)
         qDebug() << QString("Unable to return a buffer to the encoder port");
   }
   if(complete)
      vcos_semaphore_post(&(pData->complete_semaphore));
}


PiCamera::PiCamera(int cameraNum, int sensorMode)
    : component(nullptr)
    , pool(nullptr)
    , previewConnection(nullptr)
{
    if(createComponent(cameraNum, sensorMode) != MMAL_SUCCESS)
        exit(EXIT_FAILURE);
    pControl = new CameraControl(component);
    VCOS_STATUS_T vcos_status = vcos_semaphore_create(&callbackData.complete_semaphore, "RaspiStill-sem", 0);
    if(vcos_status != VCOS_SUCCESS)
        exit(EXIT_FAILURE);
}


MMAL_STATUS_T
PiCamera::createComponent(int cameraNum, int sensorMode) {
    MMAL_STATUS_T status;
    // Create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &component);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Failed to create camera component");
        if(component) {
            mmal_component_destroy(component);
            component = nullptr;
        }
        return status;
    }
    MMAL_PARAMETER_INT32_T camera_num = {
                                            {MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)},
                                            cameraNum
                                        };
    status = mmal_port_parameter_set(component->control, &camera_num.hdr);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not select camera : error %1").arg(status);
        if(component) {
            mmal_component_destroy(component);
            component = nullptr;
        }
        return status;
    }
    if(!component->output_num) {
        status = MMAL_ENOSYS;
        qDebug() << QString("Camera doesn't have output ports");
        if(component) {
            mmal_component_destroy(component);
            component = nullptr;
        }
        return status;
    }
    status = mmal_port_parameter_set_uint32(component->control,
                                            MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG,
                                            uint32_t(sensorMode));
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Could not set sensor mode : error %1").arg(status);
        if(component) {
            mmal_component_destroy(component);
            component = nullptr;
        }
    }
    return status;
}


PiCamera::~PiCamera() {
    destroyComponent();
}


void
PiCamera::destroyComponent() {
    if(component) {
        MMAL_PORT_T *still_port = component->output[MMAL_CAMERA_CAPTURE_PORT];
        if(pool)
            mmal_port_pool_destroy(still_port, pool);
        mmal_component_destroy(component);
    }
    component = nullptr;
    pool = nullptr;
}


MMAL_STATUS_T
PiCamera::setConfig(MMAL_PARAMETER_CAMERA_CONFIG_T* pCam_config) {
    MMAL_STATUS_T status;
    status = mmal_port_parameter_set(component->control, &pCam_config->hdr);
    return status;
}


MMAL_STATUS_T
PiCamera::setPortFormats(bool fullResPreview,
                         MMAL_FOURCC_T encoding,
                         int width,
                         int height)
{
    // Utility variables
    MMAL_STATUS_T status;
    MMAL_PORT_T *previewPort = component->output[MMAL_CAMERA_PREVIEW_PORT];
    MMAL_PORT_T *stillPort   = component->output[MMAL_CAMERA_CAPTURE_PORT];
    MMAL_ES_FORMAT_T *format;

// Set up the port formats starting from the Preview Port
    format = previewPort->format;
// With VideoCore OPAQUE image format, image handles are
// returned to the host but not the actual image data.
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    if(pControl->get_shutter_speed() > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 },
                                                {166, 1000}
                                               };
        mmal_port_parameter_set(previewPort, &fps_range.hdr);
    }
    else if(pControl->get_shutter_speed() > 1000000) {
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
        mmal_component_destroy(component);
        return status;
    }
// Now set up the Still Port
    format = stillPort->format;
    if(pControl->get_shutter_speed() > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 }, {166, 1000}
                                               };
        mmal_port_parameter_set(stillPort, &fps_range.hdr);
    }
    else if(pControl->get_shutter_speed() > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 167, 1000 },
                                                {999, 1000}
                                               };
        mmal_port_parameter_set(stillPort, &fps_range.hdr);
    }
// Set our format on the Stills Port
    encoding = MMAL_ENCODING_RGB24;
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
    status = mmal_port_format_commit(stillPort);
    if(status != MMAL_SUCCESS ) {
        qDebug() << QString("camera still format couldn't be set");
        mmal_component_destroy(component);
        return status;
    }

    return MMAL_SUCCESS;
}


MMAL_STATUS_T
PiCamera::enableCamera() {
    // Enable component
    MMAL_STATUS_T status = mmal_component_enable(component);
    if(status != MMAL_SUCCESS ) {
        qDebug() << QString("camera component couldn't be enabled");
        mmal_component_destroy(component);
    }
    return status;
}


void
PiCamera::createBufferPool() {
    // Create pool of buffer headers for the output port to consume
    // Utility variables
    MMAL_PORT_T *still_port = component->output[MMAL_CAMERA_CAPTURE_PORT];
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
PiCamera::startPreview(Preview *pPreview) {
    if(!pPreview) return MMAL_ENOTREADY;
    MMAL_STATUS_T status;
// Connect Camera to Preview
    MMAL_PORT_T *previewInputPort  = pPreview->pComponent->input[0];
    MMAL_PORT_T *cameraPreviewPort = component->output[MMAL_CAMERA_PREVIEW_PORT];
    status = connectPorts(cameraPreviewPort, previewInputPort, &previewConnection);
    if(status != MMAL_SUCCESS) {
        mmal_status_to_int(status);
        qDebug() << QString("%1: Failed to connect camera to preview").arg(__func__);
    }
    return status;
}


MMAL_STATUS_T
PiCamera::start(JpegEncoder *pEncoder) {
    MMAL_STATUS_T status;
    MMAL_PORT_T* cameraStillPort   = component->output[MMAL_CAMERA_CAPTURE_PORT];
    MMAL_PORT_T* encoderInputPort  = pEncoder->pComponent->input[0];
    if(verbose)
       qDebug() << QString("Connecting Camera Stills port to Encoder Input port");
    // Now connect the camera to the encoder
    status = connectPorts(cameraStillPort, encoderInputPort, &encoderConnection);
    if(status != MMAL_SUCCESS) {
       qDebug() << QString("%1: Failed to connect camera video port to encoder input")
                   .arg(__func__);
       exit(EXIT_FAILURE);
    }
    // Enable the encoder output port
    MMAL_PORT_T* encoderOutputPort = pEncoder->pComponent->output[0];
    if(verbose)
       qDebug() << QString("Enabling encoder output port");
    // Set up our userdata passed through to the callback
    callbackData.file_handle  = nullptr; // Null until we open our filename
    callbackData.pSource      = pEncoder;
    encoderOutputPort->userdata = reinterpret_cast<struct MMAL_PORT_USERDATA_T *>(&callbackData);
    // Enable the Encoder output port and tell it its callback function
    status = mmal_port_enable(encoderOutputPort, encoderBufferCallback);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Failed to setup camera output");
        exit(EXIT_FAILURE);
    }
    // Send all the buffers to the encoder output port
    uint32_t num = mmal_queue_length(pEncoder->pool->queue);
    for(uint32_t q=0; q<num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pEncoder->pool->queue);
        if(!buffer)
            qDebug() << QString("Unable to get a required buffer %1 from pool queue")
                        .arg(q);
        status = mmal_port_send_buffer(encoderOutputPort, buffer);
        if(status != MMAL_SUCCESS) {
            mmal_status_to_int(status);
            qDebug() << QString("%1: Unable to send a buffer to encoder output port (%2)")
                        .arg(__func__)
                        .arg(q);
            exit(EXIT_FAILURE);
        }
    }
    return status;
}


/**
 * Connect two specific ports together
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param connection Pointer to a mmal connection pointer, reassigned if function successful
 * Returns a MMAL_STATUS_T giving result of operation
 */
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
    mmal_status_to_int(status);
    if(verbose)
        qDebug() << "Closing down";
    // Disable all our ports that are not handled by connections
    MMAL_PORT_T *camera_video_port = component->output[MMAL_CAMERA_VIDEO_PORT];
    checkDisablePort(camera_video_port);
    MMAL_PORT_T* cameraStillPort = component->output[MMAL_CAMERA_CAPTURE_PORT];
    checkDisablePort(cameraStillPort);
    if(previewConnection)
        mmal_connection_destroy(previewConnection);
    // Disable components
    if(pPreview->pComponent)
        mmal_component_disable(pPreview->pComponent);
    if(component)
        mmal_component_disable(component);
    pPreview->destroy();
    destroyComponent();
    if(verbose)
        qDebug() << "Close down completed, all components disconnected, disabled and destroyed";
    checkConfiguration(128);
}


/// Checks if specified port is valid and enabled, then disables it
/// @param port Pointer the port
void
PiCamera::checkDisablePort(MMAL_PORT_T *port) {
   if(port && port->is_enabled) {
      MMAL_STATUS_T status = mmal_port_disable(port);
       if(status != MMAL_SUCCESS) {
          qDebug() << QString("%1: Failed to disable the connected port")
                      .arg(__func__);
          exit(EXIT_FAILURE);
       }
   }
}


void
PiCamera::stop(JpegEncoder *pEncoder) {
    MMAL_PORT_T* encoderOutputPort = pEncoder->pComponent->output[0];
    checkDisablePort(encoderOutputPort);
    MMAL_STATUS_T status = mmal_connection_release(encoderConnection);
    if(status != MMAL_SUCCESS) {
       qDebug() << QString("%1: Failed to release the connection between camera port and encoder input")
                   .arg(__func__);
       exit(EXIT_FAILURE);
    }
    if(verbose)
        qDebug() << QString("Disabling camera still output port");
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
        if(verbose)
           qDebug() << "Writing" << sPathName;
    }
    callbackData.file_handle = output_file;
    MMAL_PORT_T* cameraStillPort = component->output[MMAL_CAMERA_CAPTURE_PORT];
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

