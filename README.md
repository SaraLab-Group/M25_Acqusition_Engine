# M25 Acquisition Engine for Basler and FLIR Cameras
The M25 acquisiton engine is a C++ multithreaded program that controls the acquisition of the 25-plane camera array multifocus microscope (M25) for fast and simultanous 3D imaging. The engine captures simultaneously from 25 machine vision cameras (FLIR or Basler Ace) to run at >100FPS without loss of frames and outputs `.raw`.

Check out our [journal_article]() and [napari plugin]() to see the capabities and applications of the engine.

## Overview
Windows SDK File Handler library for rapid storage into SSD.
Read and write thread per camera
Ping-Pong Buffer

## Required Libraries and Settings
1.2.6.0 -Libusb:
    https://sourceforge.net/projects/libusb-win32/



(add images)
<image src = "docs/imgs/.png" width="600">

## Usage
This engine can be used as CLI or [Napari Plugin](pending). 

## Branches
- `basler` - branch for Basler cameras
- `flir` - branch for FLIR cameras
- `dev` - development

## Installation
(pending)

## License
This project is licensed under BSD License -- see the LICENSE.md for more details.




