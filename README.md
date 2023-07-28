# M25 Acquisition Engine for Basler and FLIR Cameras
The M25 acquisition engine is a C++ multithreaded program that controls the acquisition of the 25-plane camera array multifocus microscope (M25) for fast and simultaneous 3D imaging. The engine captures simultaneously from 25 machine vision cameras (FLIR or Basler Ace) to run at >160FPS without loss of frames to capture fast and live 3D dynamics of biological organisms. 

Check out our [journal_article]() and [napari plugin](https://github.com/SaraLab-Group/m25-napari) to see the capabilities and applications of the engine.

## Summary
The M25 acquisition engine is a multithreaded program that spawns individual read and write threads per camera to offload the data into a disk. This program uses the Windows OS ` Windows SDK File Handler` library for rapid storage into NVMe SSD Raid 0 arrays. Using this library we can bypass the default read and write functions by buffering with a pre-allocated RAM memory aligned to the file-system offsets. The NVMe drives are 512B sector aligned which allows us to do direct disk writing to known locations doing pointer arithmetic. We create individual acquisition threads for each camera and copy the sensor data into the sector-aligned buffer. The read-and-write process is done dynamically by implementing the RAM ping-pong buffer that allows the writing directly to RAM and then writing to disk into a binary file. Once the acquisition is done, the binary file is converted to desired file format (i.e `.raw` or `.tif`). 

<img src="https://github.com/SaraLab-Group/M25_Acqusition_Engine/blob/main/docs/images/timing_diagram.png" alt="m25 acuqisition engine diagram" width="500"/>

## Installation
### Required Libraries and Settings
C++20
- 1.2.6.0 -Libusb: https://sourceforge.net/projects/libusb-win32/
- 1.78 - Boost: https://www.boost.org/users/history/version_1_78_0.html
- Disabling windows indexing for Hard Drives [LINK](https://www.auslogics.com/en/articles/how-to-turn-off-windows-indexing/#:~:text=Once%20File%20Explorer%20shows%20up,click%20on%20the%20OK%20button)


## Usage
This engine can be used as CLI or [Napari Plugin](https://github.com/SaraLab-Group/m25-napari). Depending on what cameras the you have setup the scope use the following folders to compile the `.exe`

## Folders
- `basler` - Basler cameras
- `flir` - FLIR cameras

## Branches
- `dev` - development (deprecated)

## License
This project is licensed under BSD-3-clause License -- see the [LICENSE](LICENSE) for more details.




