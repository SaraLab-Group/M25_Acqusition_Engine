/* This should contain shared structs and macro defines and shared includes */

#pragma once
#ifndef PROJECT_HEADERS_H
#define PROJECT_HEADERS_H

// Include files to use the pylon API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbCamera.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/InstantCamera.h>

#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

#include <thread>         // std::this_thread::sleep_for 
#include <chrono>         // std::chrono::seconds
#include <condition_variable> // For Waking Write Thread
#include <barrier>
#include <vector>
//#include <cstdlib> // why this and stdlib.h
#include <iostream>
#include <sstream>
#include <string.h>
//#include <stdio.h>
#include <direct.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
//#include <Windows.h> //Needed For windows CreateFile and WriteFile File Handle libraries.


#ifndef _WIN32
#include <pthread.h>
#endif


#include <mutex>



// comment this out to test just aquisition but not convert binary chunks to TIFF
#define CONVERT_TIFF


// Namespace for using pylon objects.
using namespace Pylon;
using namespace GenApi;
using namespace GenICam;


using namespace Basler_UsbCameraParams;
typedef CBaslerUsbCamera Camera_t;
typedef CBaslerUsbImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef CBaslerUsbGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
typedef CBaslerUsbCameraEventHandler CCameraEventHandler_t;




typedef struct cam_data {
    uint8_t number;  // This gives sequentially alocated camera index
    uint64_t offset; // This gives the image_size * number value
    CInstantCamera* camPtr;
    uint8_t* image_dat; // Might be redundant since declared in same scope as lamda

};


typedef struct write_data {
    //frame_buffer* write_buff = out_buff;
    bool first = true; // Most Likely not needed anymore controlled in buffere swap
    uint8_t cam_count = 0;
    // Both of these not really needed because declared in same scope of the lamda function
    std::condition_variable* cv;
    std::mutex* lk;
};


//For keeping track of Frame events
typedef struct cam_event {
    double time_stamp;
    double sensor_readout;
    int64_t missed_frame_count;
    uint64_t frame;
};


/* Stuff to test our code This is the struct sent into our Psoc 5LP */

#define CHANGE_CONFIG 0x1
#define DROPPED_FRAME 0x2
#define SET_RTC 0x4
#define ACK_CMD 0x8
#define START_COUNT 0x10
#define COUNTING 0x20
#define STOP_COUNT 0x40
#define ACQUIRE_CAMERAS 0x80
#define CAMERAS_ACQUIRED 0x100
#define RELEASE_CAMERAS 0x200
#define ACQUIRE_FAIL 0x400
#define START_CAPTURE 0x800
#define CAPTURING 0x1000
#define USB_HERE 0x2000 // Use this as flag for Server alive too, since we wont trigger without USB
#define CONVERTING 0x4000
#define FINISHED_CONVERT 0x8000
#define ACQUIRING_CAMERAS 0x10000
#define CONFIG_CHANGED 0x20000
#define EXIT_THREAD 0x80000000
#define DEFAULT_FPS (65u)

typedef struct usb_data {
    uint16_t flags;
    uint16_t fps;
    uint32_t time_waiting; // Currently Time between not ready and ready
    uint64_t count;
};


/*class ConfData(Structure) :
    __fields__ = [('horz', c_uint),
    ('vert', c_uint),
    ('fps', c_uint),
    ('bpp', c_uint),
    ('capTime', c_uint),
    ('path', c_char),
    ('flags', c_uint16)]
*/

typedef struct TCP_IP_DAT {
    uint32_t horz;
    uint32_t vert;
    uint32_t fps;
    uint32_t exp;
    uint32_t bpp;
    uint32_t capTime;
    char path[255];
    uint32_t flags;
};

typedef struct USB_THD_DATA {
    // Do I need this?
    TCP_IP_DAT* incoming_data;
    TCP_IP_DAT* outgoing_data;
    std::mutex* crit;
    std::mutex* crit2;
};

typedef struct SERVER_THD_DATA {
    TCP_IP_DAT* incoming_data;
    TCP_IP_DAT* outgoing_data;
    std::condition_variable* signal_ptr;
    std::mutex* mtx_ptr;
};

#endif