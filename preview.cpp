#include "preview.h"
#include "QDebug"


Preview::Preview(int width, int height)
    : wantPreview(1)
    , wantFullScreenPreview(0)
    , opacity(255)
    , previewWindow(MMAL_RECT_T{0, 0, width, height})
    , pComponent(nullptr)

{
    if(createComponent() != MMAL_SUCCESS)
        exit(EXIT_FAILURE);
}


/// Create the preview component, set up its ports
/// state Pointer to state control struct
/// return MMAL_SUCCESS if all OK, something else otherwise
MMAL_STATUS_T
Preview::createComponent() {
    MMAL_STATUS_T status;
    if(!wantPreview) {
        // No preview required, so create a null sink component to take its place
        status = mmal_component_create("vc.null_sink", &pComponent);
        if(status != MMAL_SUCCESS) {
            qDebug() << QString("Unable to create null sink component");
            if(pComponent)
                mmal_component_destroy(pComponent);
            return status;
        }
    }
    else {
        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                       &pComponent);
        if(status != MMAL_SUCCESS) {
            qDebug() << QString("Unable to create preview component");
            if(pComponent)
                mmal_component_destroy(pComponent);
            return status;
        }
        if(!pComponent->input_num) {
            status = MMAL_ENOSYS;
            qDebug() << QString("No input ports found on component");
            mmal_component_destroy(pComponent);
            return status;
        }
        MMAL_PORT_T *inputPort = pComponent->input[0];
        MMAL_DISPLAYREGION_T param;

        param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
/**
 * param.set is a Bitfield that indicates which fields are set and should be used.
 *  All other fields will maintain their current value.
 *  MMAL_DISPLAYSET_T defines the bits that can be combined.
 */
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
        status = mmal_port_parameter_set(inputPort, &param.hdr);
        if(status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
            qDebug() << QString("unable to set preview port parameters (%1)").arg(status);
            mmal_component_destroy(pComponent);
            return status;
        }
    }
    // Enable component
    status = mmal_component_enable(pComponent);
    if(status != MMAL_SUCCESS) {
        qDebug() << QString("Unable to enable preview/null sink component (%u)").arg(status);
        mmal_component_destroy(pComponent);
        return status;
    }
    return status;
}


/// Destroy the preview component
/// state Pointer to state control struct
void
Preview::destroy() {
   if(pComponent) {
      mmal_component_destroy(pComponent);
      pComponent = nullptr;
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


MMAL_STATUS_T
Preview::setScreenPos(MMAL_RECT_T previewWindow) {
    MMAL_STATUS_T status = MMAL_SUCCESS;
    MMAL_PORT_T *previewPort = pComponent->input[0];
    MMAL_DISPLAYREGION_T param;

    param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    param.set = (MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_FULLSCREEN);
    param.fullscreen = 0;
    param.dest_rect = previewWindow;
    status = mmal_port_parameter_set(previewPort, &param.hdr);
    if(status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
        qDebug() << QString("unable to set preview port parameters (%1)").arg(status);
        mmal_component_destroy(pComponent);
    }
    return status;
}
