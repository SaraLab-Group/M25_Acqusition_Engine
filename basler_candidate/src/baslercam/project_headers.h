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
#define NEW_CNT 0x4
#define LAPSE_STOP 0x8
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
#define STAGE_TRIGG_ENABLE 0x40000
#define STAGE_TRIGG_DISABLE 0x80000
#define START_LIVE 0x100000
#define LIVE_RUNNING 0x200000
#define STOP_LIVE 0x400000
#define START_Z_STACK 0x800000
#define Z_STACK_RUNNING 0x1000000
#define STOP_Z_STACK 0x2000000
#define TOGGLE_EMMISION 0x4000000
#define TOGGLE_DIG_MOD 0x8000000
#define LAPSE_CAPTURE 0x10000000 
#define EXIT_THREAD 0x80000000
#define DEFAULT_FPS (65u)
#define MAX_CAMS (25u)
//#define EXIT_USB 0x8000  Big Yikes

// Camera Serials
#define CAM_1 23206716
#define CAM_2 23206693
#define CAM_3 23200570
#define CAM_4 23206700
#define CAM_5 23206715
#define CAM_6 23200558
#define CAM_7 23206703
#define CAM_8 23206711
#define CAM_9 23206710
#define CAM_10 23206705
#define CAM_11 23206719
#define CAM_12 23206707
#define CAM_13 23206694
#define CAM_14 23206682
#define CAM_15 23206683
#define CAM_16 23206704
#define CAM_17 23206692
#define CAM_18 23206691
#define CAM_19 23206718
#define CAM_20 23206690
#define CAM_21 23206701
#define CAM_22 23200571
#define CAM_23 23206706
#define CAM_24 23059572
#define CAM_25 23206684

// shared mem flags
#define WRITING_BUFF1 0x1
#define WRITING_BUFF2 0x2
#define READING_BUFF1 0x4
#define READING_BUFF2 0x8

/*  To Be Added After Napari Is Updated */

//typedef struct usb_data {
//    float fps;
//    uint32_t flags;
//    uint32_t time_waiting; // Currently Time between not ready and ready
//    uint64_t count;
//};

typedef struct usb_data {
    uint16_t fps;
    uint32_t flags;
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

/* For Improved Implementation Coming soon!! */

//typedef struct TCP_IP_DAT {
//    uint32_t horz;
//    uint32_t vert;
//    float fps;
//    uint32_t exp;
//    uint32_t bpp;
//    uint32_t z_frames;
//    uint32_t capTime;
//    float lapse_min;
//    uint32_t lapse_count;
//    char path[256];
//    char proName[256];
//    uint32_t flags;
//    double gain;
//};




typedef struct TCP_IP_DAT {
    uint32_t horz;
    uint32_t vert;
    uint32_t fps;
    uint32_t exp;
    uint32_t bpp;
    uint32_t z_frames;
    uint32_t capTime;
    char path[255];
    char proName[255];
    uint32_t flags;
    double gain;
};

typedef struct USB_THD_DATA {
    // Do I need this?
    usb_data* incoming;
    usb_data* outgoing;
    std::mutex* crit;
    std::mutex* usb_srv_mtx;
};

typedef struct LIVE_THD_DATA {
    std::vector<std::string>* serials;
    std::vector<std::string>* camera_names;
    std::vector<int>* zNums;
    std::condition_variable* signal_live;
    std::mutex* crit;
    cam_data* cam_dat;
    unsigned int* total_cams;
    uint64_t* image_size;
    uint32_t flags;
};

typedef struct SERVER_THD_DATA {
    TCP_IP_DAT* incoming_data;
    TCP_IP_DAT* outgoing_data;
    usb_data* usb_incoming;
    usb_data* usb_outgoing;
    uint32_t* live_flags;
    std::condition_variable* signal_ptr;
    std::mutex* mtx_ptr;
    std::mutex* usb_srv_mtx;
};



//https://stackoverflow.com/questions/26114518/ipc-between-python-and-win32-on-windows-os

typedef struct
{
    void* hFileMap;
    void* pData;
    char MapName[256];
    size_t Size;
} SharedMemory;

#endif