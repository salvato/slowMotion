#include "utility.h"
#include <QString>
#include <QDebug>
#include "bcm_host.h"


/// Convert a MMAL status return value to a simple boolean of success
/// Also displays a fault if code is not success
/// @param status The error code to convert
/// @return 0 if status is success, 1 otherwise
int
mmal_status_to_int(MMAL_STATUS_T status) {
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
get_mem_gpu(void) {
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
get_camera(int *supported, int *detected) {
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
checkConfiguration(int min_gpu_mem) {
    int gpu_mem = get_mem_gpu();
    int supported = 0, detected = 0;
    get_camera(&supported, &detected);
    if(!supported) {
        qDebug() << QString("Camera is not enabled in this build:");
        qDebug() << QString("Try running \"sudo raspi-config\"");
        qDebug() << QString("and ensure that \"camera\" has been enabled");
        exit(EXIT_FAILURE);
    }
    else if(gpu_mem < min_gpu_mem) {
        qDebug() << QString("Only %1M of gpu_mem is configured.")
                    .arg(gpu_mem);
        qDebug() << QString("Try running \"sudo raspi-config\" and ensure that \"memory_split\"");
        qDebug() << QString("has a value of %2 or greater")
                    .arg(min_gpu_mem);
        exit(EXIT_FAILURE);
    }
    else if(!detected) {
        qDebug() << QString("No Camera has been detected.");
        qDebug() << QString("Please check carefully the camera module is installed correctly");
        exit(EXIT_FAILURE);
    }
    if(verbose) {
        qDebug() << "N° of Camera Supported:" << supported;
        qDebug() << "N° of Camera Detected" << detected;
    }
}
