#include "jpegencoder.h"
#include "QDebug"
#include "utility.h"


#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"


JpegEncoder::JpegEncoder()
    : pComponent(nullptr)
    , pool(nullptr)
{
    quality = 100;
    restartInterval = 0;
    encoding = MMAL_ENCODING_JPEG;
    if(createComponent() != MMAL_SUCCESS)
        exit(EXIT_FAILURE);
    createBufferPool();
}


MMAL_STATUS_T
JpegEncoder::createComponent() {
   MMAL_PORT_T *encoder_input = nullptr;
   MMAL_PORT_T *encoder_output = nullptr;
   MMAL_STATUS_T status;

   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &pComponent);

   if(status != MMAL_SUCCESS) {
      qDebug() << QString("Unable to create JPEG encoder component");
      if(pComponent)
         mmal_component_destroy(pComponent);
      return status;
   }

   if(!pComponent->input_num || !pComponent->output_num) {
      status = MMAL_ENOSYS;
      qDebug() << QString("JPEG encoder doesn't have input/output ports");
      if(pComponent)
         mmal_component_destroy(pComponent);
      return status;
   }

   encoder_input = pComponent->input[0];
   encoder_output = pComponent->output[0];

   // We want same format on input and output
   mmal_format_copy(encoder_output->format, encoder_input->format);

   // Specify out output format
   encoder_output->format->encoding = encoding;

   encoder_output->buffer_size = encoder_output->buffer_size_recommended;

   if(encoder_output->buffer_size < encoder_output->buffer_size_min)
      encoder_output->buffer_size = encoder_output->buffer_size_min;

   encoder_output->buffer_num = encoder_output->buffer_num_recommended;

   if(encoder_output->buffer_num < encoder_output->buffer_num_min)
      encoder_output->buffer_num = encoder_output->buffer_num_min;

// Commit the port changes to the output port
   status = mmal_port_format_commit(encoder_output);

   if(status != MMAL_SUCCESS) {
      qDebug() << QString("Unable to set format on video encoder output port");
      if (pComponent)
         mmal_component_destroy(pComponent);
      return status;
   }

// Set the JPEG quality level
   status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_Q_FACTOR, quality);

   if(status != MMAL_SUCCESS) {
      mmal_status_to_int(status);
      qDebug() << QString("Unable to set JPEG quality %1")
                  .arg(quality);
      if (pComponent)
         mmal_component_destroy(pComponent);
      return status;
   }
// Set the JPEG restart interval
   status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_RESTART_INTERVAL, restartInterval);
   if(restartInterval && status != MMAL_SUCCESS) {
      qDebug() << QString("Unable to set JPEG restart interval");
      if (pComponent)
         mmal_component_destroy(pComponent);
      return status;
   }
//  Enable component
   status = mmal_component_enable(pComponent);
   if(status  != MMAL_SUCCESS) {
      qDebug() << QString("Unable to enable video encoder component");
      if (pComponent)
         mmal_component_destroy(pComponent);
      return status;
   }
// Create pool of buffer headers for the output port to consume
   pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);
   if(!pool) {
      qDebug() << QString("Failed to create buffer header pool for encoder output port %1")
                  .arg(encoder_output->name);
   }

   if(verbose)
      fprintf(stderr, "Encoder component done\n");

   return status;
}


void
JpegEncoder::createBufferPool() {
    // Create pool of buffer headers for the output port to consume
    MMAL_PORT_T *outputPort = pComponent->output[0];
    if(verbose) {
        qDebug() << "Encoder out Port buffer size  :" << outputPort->buffer_size;
        qDebug() << "Encoder out Port buffer number:" << outputPort->buffer_num;
    }
    // Create pool of buffer headers for the output port to consume
    pool = mmal_port_pool_create(outputPort, outputPort->buffer_num, outputPort->buffer_size);
    if(!pool) {
        qDebug() << QString("Failed to create buffer header pool for encoder output port %1")
                    .arg(outputPort->name);
        exit(EXIT_FAILURE);
    }
}


void
JpegEncoder::destroy() {
   // Get rid of any port buffers first
   if(pool) {
      mmal_port_pool_destroy(pComponent->output[0], pool);
   }
   if(pComponent) {
      mmal_component_destroy(pComponent);
      pComponent = nullptr;
   }
}

