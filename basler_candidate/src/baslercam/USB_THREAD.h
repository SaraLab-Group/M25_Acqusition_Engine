/*
 The USB Thrad Header
 Most of this code is form the LIBUSB example code modified for our needs
 I put all of the LIBUSB headers and libraries in this projects directory.
 Under Linker Input I needed to specify the path of the .lib file
 C:\Users\Callisto\Documents\abajor\M25_basler\basler_candidate\libusb-win32-bin-1.2.6.0\libusb-win32-bin-1.2.6.0\lib\msvc_x64\libusb.lib
*/

#pragma once

#ifndef USB_THREAD_H
#define USB_THREAD_H
#include "project_headers.h"
#include <lusb0_usb.h>


//////////////////////////////////////////////////////////////////////////////
// TEST SETUP (User configurable)

// Issues a Set configuration request
#define TEST_SET_CONFIGURATION

// Issues a claim interface request
#define TEST_CLAIM_INTERFACE

// Use the libusb-win32 async transfer functions. see
// transfer_bulk_async() below.
//#define TEST_ASYNC

// Attempts one bulk read.
#define TEST_BULK_READ

// Attempts one bulk write.
#define TEST_BULK_WRITE

//////////////////////////////////////////////////////////////////////////////
// DEVICE SETUP (User configurable)

// Device vendor and product id.
#define MY_VID 0x04b4 // Cypress Psoc 5lp default
#define MY_PID 0x8051

// Device configuration and interface id.
#define MY_CONFIG 1
#define MY_INTF 0

// Device endpoint(s)
#define EP_IN 0x01
#define EP_OUT 0x02

// Device of bytes to transfer.
#define BUF_SIZE 64

//////////////////////////////////////////////////////////////////////////////

usb_dev_handle* open_dev(void);
void* USB_THREAD(void* data);

#endif
