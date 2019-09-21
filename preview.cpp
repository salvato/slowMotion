#include "preview.h"
#include "QDebug"


Preview::Preview() {
    wantPreview           = 1;
    wantFullScreenPreview = 0;
    opacity               = 255;
    previewWindow.x       = 0;
    previewWindow.y       = 0;
    previewWindow.width   = 320;
    previewWindow.height  = 240;
    previewComponent     = nullptr;
}


/// Create the preview component, set up its ports
/// state Pointer to state control struct
/// return MMAL_SUCCESS if all OK, something else otherwise
MMAL_STATUS_T
Preview::createComponent() {
    // Convenient variables
    MMAL_COMPONENT_T *preview = nullptr;
    MMAL_PORT_T *preview_port = nullptr;
    MMAL_STATUS_T status;

    if(!wantPreview) {
        // No preview required, so create a null sink component to take its place
        status = mmal_component_create("vc.null_sink", &preview);
        if(status != MMAL_SUCCESS) {
            vcos_log_error("Unable to create null sink component");
            if(preview)
                mmal_component_destroy(preview);
            return status;
        }
    }
    else {
        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                       &preview);
        if(status != MMAL_SUCCESS) {
            vcos_log_error("Unable to create preview component");
            if(preview)
                mmal_component_destroy(preview);
            return status;
        }
        if(!preview->input_num) {
            status = MMAL_ENOSYS;
            vcos_log_error("No input ports found on component");
            mmal_component_destroy(preview);
            return status;
        }
        preview_port = preview->input[0];
        MMAL_DISPLAYREGION_T param;
        param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
        param.set = MMAL_DISPLAY_SET_LAYER;
        param.layer = PREVIEW_LAYER;

        param.set |= MMAL_DISPLAY_SET_ALPHA;
        param.alpha = uint32_t(opacity);
        if(wantFullScreenPreview) {
            param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
            param.fullscreen = 1;
        }
        else {
            param.set |= (MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_FULLSCREEN);
            param.fullscreen = 0;
            param.dest_rect = previewWindow;
        }
        status = mmal_port_parameter_set(preview_port, &param.hdr);
        if(status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
            vcos_log_error("unable to set preview port parameters (%u)", status);
            mmal_component_destroy(preview);
            return status;
        }
    }
    // Enable component
    status = mmal_component_enable(preview);
    if(status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable preview/null sink component (%u)", status);
        mmal_component_destroy(preview);
        return status;
    }
    previewComponent = preview;
    return status;
}


/// Destroy the preview component
/// state Pointer to state control struct
void
Preview::destroy() {
   if(previewComponent) {
      mmal_component_destroy(previewComponent);
      previewComponent = nullptr;
   }
}


/// Dump parameters as human readable to stdout
/// state Pointer to parameter block
void
Preview::dump_parameters() {
   qDebug() << QString("Preview %1, Full screen %2")
               .arg(wantPreview ? "Yes" : "No")
               .arg(wantFullScreenPreview ? "Yes" : "No");
   qDebug() << QString("Preview window %1,%2,%3,%4\nOpacity %5\n")
               .arg(previewWindow.x)
               .arg(previewWindow.y)
               .arg(previewWindow.width)
               .arg(previewWindow.height)
               .arg(opacity);
}

