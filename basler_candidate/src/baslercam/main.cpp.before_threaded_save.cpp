// Grab_MultipleCameras.cpp
/*
Note: Before getting started, Basler recommends reading the "Programmer's Guide" topic
in the pylon C++ API documentation delivered with pylon.
If you are upgrading to a higher major version of pylon, Basler also
strongly recommends reading the "Migrating from Previous Versions" topic in the pylon C++ API documentation.

This sample illustrates how to grab and process images from multiple cameras
using the CInstantCameraArray class. The CInstantCameraArray class represents
an array of instant camera objects. It provides almost the same interface
as the instant camera for grabbing.
The main purpose of the CInstantCameraArray is to simplify waiting for images and
camera events of multiple cameras in one thread. This is done by providing a single
RetrieveResult method for all cameras in the array.
Alternatively, the grabbing can be started using the internal grab loop threads
of all cameras in the CInstantCameraArray. The grabbed images can then be processed by one or more
image event handlers. Please note that this is not shown in this example.
*/

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
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <direct.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <Windows.h> //Needed For windows CreateFile and WriteFile File Handle libraries.

//#include <boost/iostreams/device/mapped_file.hpp>
//#include <filesystem>

#ifndef _WIN32
#include <pthread.h>
#endif

#include "USB_THREAD.h" // For USB function pointer used for threading.

// Namespace for using pylon objects.
using namespace Pylon;
using namespace GenApi;
using namespace GenICam;

using namespace Basler_UsbCameraParams;
typedef CBaslerUsbCamera Camera_t;
typedef CBaslerUsbImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef CBaslerUsbGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
typedef CBaslerUsbCameraEventHandler CCameraEventHandler_t;

// For array size
static const uint8_t EIGHT_BIT = 8;
static const uint8_t SIXTEEN_BIT = 16;

// This Value is mostly for testing as we may apply a crop factor
static const uint32_t PIX_OVER_8 = 288000; // This is (1920*1200)/8 makes it easier to multiply by 8 or 16bit

// This Value was for testing manual buffering of std::ofstream
static const uint32_t FS_BUF_SIZE = 131072;//524288; //2097152;//262144;

// WriteFile only returns a DWORD == uint32_t
// The Writes must be alligned to 512B sectors
// Therefore MAX_ALLIGNED_WRITE is the largest 
// you can tell WriteFile to write at once.

static const DWORD ALIGNMENT_BYTES = 512;
static const DWORD MAX_ALIGNED_WRITE = 4294966784;


// Thread Stuff
// I think I'm going to drop this so I can do some dynamic pointer arithmetic;
// Different Bit Depth Frame buffers;
typedef struct frame_buffer_8 {
	uint8_t image_array[PIX_OVER_8 * EIGHT_BIT];
};

typedef struct frame_buffer_16 {
	uint8_t image_array[PIX_OVER_8 * SIXTEEN_BIT];
};

// Number of camera images to be grabbed.
// Currently just building an array big enough for 25 images
// Regardless of number of cameras present.
static const uint32_t c_countOfImagesToGrab = 25;

static const size_t c_maxCamerasToUse = 25;

//frame_buffer buff1[c_countOfImagesToGrab];
//frame_buffer buff2[c_countOfImagesToGrab];
//frame_buffer* in_buff = nullptr;
//frame_buffer* out_buff = nullptr;

// All sorts of pointers
// These are to keep track of the head of our buffers
uint8_t* head_buff1 = nullptr;
uint8_t* head_buff2 = nullptr;

// This is for keeping track of frames to buffer
// And buffer chunks written to disk
// To signal camera threads to stop collecting
// Will itterate the count in the write thread
// bool capture will be set to false in the
// barrier end condition
uint32_t write_count = 0; // This is 
uint32_t frame_count = 0;
uint32_t frames = 0; // global frames can be changed in main
bool capture = false;

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

// 2304000 1920x1200
// Namespace for using cout.
using namespace std;

// variables for scan opt
const uint32_t horz_max = 1920;
const uint32_t vert_max = 1200;
uint32_t exposure = 6700; // In Micro Seconds
uint32_t seconds = 0;
uint32_t horz = horz_max;
uint32_t vert = vert_max;
uint32_t horz_off_set = 0;
uint32_t vert_off_set = 0;
uint8_t bitDepth = 8;
uint8_t fps = 100;



double old = 0.0; //variable to test trigger delay

//A prototype
void SetPixelFormat_unofficial(INodeMap& nodemap, String_t format);

//Directory name images to be saved.
// Should probably be set from pycro or micro manager.
string  strDirectryName = "D:\\Ant1 Test\\binaries";
// From Basler Fast Write Example Not currently used;
string  strMetaFileName = "Meta.txt";

// From Basler Sample Code for fast binary writes;
void saveBuffer(const char* FileName, CGrabResultPtr ptrGrabResult)
{
	ofstream fs(FileName, std::ifstream::binary);
	const uint8_t* pImageBuffer = (uint8_t*)ptrGrabResult->GetBuffer();
	fs.write((char*)pImageBuffer, ptrGrabResult->GetPayloadSize());
	//How big it be?
	//printf("PayloadSize: %ld\n", (long)ptrGrabResult->GetPayloadSize());
	fs.close();
}

/********************** LIGHTNING FAST VCR REPAIR **************************************************
* This Buffer to disk method uses the Windows SDK File Handle methods
* to store data very rapidly to our NVME Raid array the larger the file
* the better it does (I've regularly hit in the 6.5GB/s range).
* The file handle must be opened with CreateFile FILE_FLAG_NO_BUFFERING command.
* This is a very powerful but dangerous method. For it to work we must make sure that the
* data is all sector aligned to the filesystem sector sizes.
* The largest data chunk the WriteFile method can accept is 2^32 - 1.
* This is not sector alligned, so the maximum we can write will be a multiple of the sector size
* i.e. 512B.
* Since the Write Method does not know where the end of the buffer is, if you are writting a file larger than 4GB
* you MUST keep track of how many bytes you've written, and edit the dwBytesToWrite value to match (albiet aligned match)
* the remaining bytes to write, otherwise it will just blindly keep writing beyond the memory you intended.
* ***************************************************************************************************/
void saveBigBuffer(const char* FileName, uint8_t* buffer, uint8_t cam_count, uint64_t aligned_size)
{
	/* Some Dead Code from my attempts at using ofstream to be purged*/

	//unsigned int bytes_written = 0;
	//unsigned int file_size = sizeof(frame_buffer) * cam_count;
	//std::ofstream fs;
	//auto data = &buffer;

	//write(data, FileName);
	//auto fs = std::ofstream(FileName, std::ios::binary|std::ios::trunc);

	uint8_t* pImageBuffer = (uint8_t*)buffer;
	uint64_t bytes_written = 0;
	/*fs.rdbuf()->pubsetbuf((char*)pImageBuffer, FS_BUF_SIZE);
	fs.open(FileName, std::ios::binary|std::ios::trunc);
	while (bytes_written < file_size) {
		fs.write((char*)pImageBuffer, FS_BUF_SIZE);
		pImageBuffer += FS_BUF_SIZE;
		bytes_written += FS_BUF_SIZE;
	}
	//How big it be?
	//printf("PayloadSize: %ld\n", sizeof(buffer));
	fs.close();*/

	/* Welcome to Windows Microsoft SDK enjoy the ride*/
	HANDLE hFile;
	DWORD dwBytesToWrite = (DWORD)MAX_ALIGNED_WRITE;//sizeof(frame_buffer) * c_countOfImagesToGrab;
	if (aligned_size < dwBytesToWrite) {
		dwBytesToWrite = (DWORD)aligned_size;//sizeof(frame_buffer) * c_countOfImagesToGrab;
	}
	DWORD dwBytesWritten = 0;
	BOOL bErrorFlag = FALSE;

	hFile = CreateFile(FileName,                // name of the write
		GENERIC_WRITE,          // open for writing
		0,                      // do not share
		NULL,                   // default security
		CREATE_ALWAYS,             // create new file only
		FILE_FLAG_NO_BUFFERING,  // normal file
		NULL);                  // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		//DisplayError(TEXT("CreateFile"));
		//_tprintf(TEXT("Terminal failure: Unable to open file \"%s\" for write.\n"), argv[1]);
		//return;
		std::cout << "Awe Shucks Failed to open file" << std::endl;
	}

	//printf("Writing %d bytes to %s.\n", dwBytesToWrite, FileName);
	while (bytes_written < aligned_size) {
		bErrorFlag = WriteFile(
			hFile,           // open file handle
			pImageBuffer,      // start of data to write
			dwBytesToWrite,  // number of bytes to write
			&dwBytesWritten, // number of bytes that were written
			NULL);            // no overlapped structure

		if (FALSE == bErrorFlag)
		{
			//DisplayError(TEXT("WriteFile"));
			printf("Terminal failure: Unable to write to file.\n");
		}
		else
		{
			if (dwBytesWritten != dwBytesToWrite)
			{
				// This is an error because a synchronous write that results in
				// success (WriteFile returns TRUE) should write all data as
				// requested. This would not necessarily be the case for
				// asynchronous writes.
				printf("Error: dwBytesWritten != dwBytesToWrite\n");
			}
			// Nothing to see here
			/*else
			{
				pImageBuffer += FS_BUF_SIZE;
				bytes_written += FS_BUF_SIZE;
				//printf("Wrote %d bytes to %s successfully.\n", dwBytesWritten, FileName);
			}*/
			bytes_written += dwBytesWritten;
			std::cout << dwBytesWritten << std::endl;
			if (aligned_size - bytes_written < dwBytesToWrite) {
				dwBytesToWrite = aligned_size - bytes_written;
			}
		}
	}
	// Sanity Check
	std::cout << "BytesWritten: " << bytes_written << std::endl;
	std::cout << "file_size: " << aligned_size << std::endl;

	CloseHandle(hFile);
}


/* This little function is for the convert Binary to tif phase*/
void readFile(const char* fileName, uint64_t* outNumberofBytes, uint8_t* chunk)
{   // [out] char* buffer
	ifstream fs(fileName, std::ifstream::binary);
	fs.seekg(0, fs.end);
	uint64_t size = fs.tellg();
	fs.seekg(0);
	//char* bufferTemp = new char[size];
	// allocate memory for file content
	fs.read((char*)chunk, size);
	//buffer->push_back(bufferTemp);
	//returning the file size, needed for converting the buffer into an Pylon image into a bitmap
	*outNumberofBytes = size;
	fs.close();
}


void mkdirTree(string sub, string dir) {
	if (sub.length() == 0)
		return;

	int i = 0;
	for (i; i < sub.length(); i++) {
		dir += sub[i];
		if (sub[i] == '/')
			break;
	}
	cout << "trying to create directory " << dir.c_str() << endl;
	_mkdir(dir.c_str()/*, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH*/);  /* Commented out Unix Flags */
	if (i + 1 < sub.length())
		mkdirTree(sub.substr(i + 1), dir);
}

/* Not Currently in use From Baler binary write sample code*/
void Save_Metadata(list<string>& strList, string strMetaFilename)
{
	ofstream fs(strMetaFilename);
	for (auto it = strList.begin();it != strList.end(); it++)
	{
		string s = *it;
		fs.write(s.c_str(), s.length());
		fs.write("\n", 1);
	}
	fs.close();
}

/*Interesting I think the normal Sleep function is allows context switching and "sleeps threads the same"*/
void sleep(int sec) {
	for (int i = sec; i > 0; --i) {
		std::cout << i << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}



enum MyEvents
{
	eMyEventFrameStart = 100,
	//eMyEventFrameStartWait = 200
};

/* Currently Unused */
class CSampleCameraEventHandler : public CCameraEventHandler_t
{
public:
	// Only very short processing tasks should be performed by this method. Otherwise, the event notification will block the
	// processing of images.
	virtual void OnCameraEvent(Camera_t& camera, intptr_t userProvidedId, INode* nodemap)
	{
		cout << endl;
		switch (userProvidedId)
		{
		case eMyEventFrameStart: // Exposure End event
			cout << "Exposure End event. FrameID: " << camera.EventFrameStartWait.GetValue() << " Timestamp: " << camera.EventFrameStartWait.GetValue() << endl << endl;
			break;
			//case eMyEventFrameStartWait:  // Event Overrun event
			//	cout << "Event Overrun event. FrameID: " << camera.EventFrameStartWaitTimestamp.GetValue() << " Timestamp: " << camera.EventFrameStartWaitTimestamp.GetValue() << endl << endl;
			//	break;
		}
	}


	//virtual void OnCameraEvent( CInstantCamera& camera, intptr_t userProvidedId, GenApi::INode* pNode)
	//{
	//	cout << "OnCameraEvent event for device " << camera.GetDeviceInfo().GetModelName() << std::endl;
	//	cout << "User provided ID: " << userProvidedId << std::endl;
	//	cout << "Event data node name: " << pNode->GetName() << std::endl;
	//	CParameter value( pNode );
	//	if ( value.IsValid() )
	//	{
	//		cout << "Event node data: " << value.ToString() << std::endl;
	//	}
	//	std::cout << std::endl;
	//}

};

/* This has a lot of behind the scenes thread generation and not really possible to synchronize
   Keeping this here for refference                                                          */

//Example of an image event handler.
class CSampleImageEventHandler : public CImageEventHandler
{
public:
	virtual void OnImageGrabbed(CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult)
	{

		INodeMap& nodemap = camera.GetNodeMap();
		//Set the line4 output TTL Low
		intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();
		CBooleanPtr(nodemap.GetNode("UserOutputValue"))->SetValue(false);
		//#ifdef PYLON_WIN_BUILD
		//		// Display the image
		//		Pylon::DisplayImage(cameraContextValue, ptrGrabResult);
		//#endif
		ostringstream filename;
		string SN = camera.GetDeviceInfo().GetSerialNumber().c_str();
		string Dir = SN.c_str();
		_mkdir(Dir.c_str());

		if (ptrGrabResult->GrabSucceeded() && CImagePersistence::CanSaveWithoutConversion(ImageFileFormat_Tiff, ptrGrabResult)) {
			//cout << "CAN SAVE TIFFS" << endl;
			filename << ".\\" + Dir + "\\" << SN.c_str() << ptrGrabResult->GetID() << ".tif";
			//CBooleanPtr(nodemap.GetNode("UserOutputValue"))->SetValue(false);
			CImagePersistence::Save(ImageFileFormat_Tiff, String_t(filename.str().c_str()), ptrGrabResult);
			CBooleanPtr(nodemap.GetNode("UserOutputValue"))->SetValue(true);
			cout << "Using device " << camera.GetDeviceInfo().GetModelName() << endl;
			double d = CFloatPtr(nodemap.GetNode("SensorReadoutTime"))->GetValue();   //Microseconds
			cout << "Sensor Readout Time: " << d / 1.0e3 << endl;
			//cout << "GrabSucceeded: " << ptrGrabResult->GrabSucceeded() << endl;
			double current = ptrGrabResult->GetTimeStamp() / 1.0;
			double diff = (current - old) / 1e9;
			cout << "TimeStamp Diff: " << diff << endl << endl;
			old = current;
			//const uint8_t *pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
			//cout << "Gray value of first pixel: " << (uint32_t)pImageBuffer[0] << endl << endl;
		}
		else {
			cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << std::endl;
		}
	}

	virtual void OnImagesSkipped(CInstantCamera& camera, size_t countOfSkippedImages)
	{
		std::cout << "OnImagesSkipped event for device " << camera.GetDeviceInfo().GetModelName() << std::endl;
		std::cout << countOfSkippedImages << " images have been skipped." << std::endl;
		std::cout << std::endl;
	}

};


/* This Class is great configures camera parameters for us */
class CTriggerConfiguration : public CConfigurationEventHandler {
public:
	static void ApplyConfiguration(GENAPI_NAMESPACE::INodeMap& nodemap)
	{
		//Select line 3 to be the Input to the TTL.
		CEnumerationPtr(nodemap.GetNode("LineSelector"))->FromString("Line3");
		CEnumerationPtr(nodemap.GetNode("LineMode"))->FromString("Input");
		CEnumParameter triggerSelector(nodemap, "TriggerSelector");
		CEnumParameter triggerMode(nodemap, "TriggerMode");
		String_t triggerName("FrameStart");
		CEnumerationPtr(nodemap.GetNode("TriggerActivation"))->FromString("RisingEdge");

		CEnumerationPtr(nodemap.GetNode("ExposureMode"))->FromString("Timed");
		CEnumerationPtr(nodemap.GetNode("ExposureAuto"))->FromString("Off");

		double d1 = CFloatPtr(nodemap.GetNode("ExposureTime"))->GetValue();
		cout << "Exposure Time Current: " << d1 << endl;
		//float exp_ms = 6.7;
		//float exp_us = 1000.0 * exp_ms;	//milisecond
		CFloatPtr(nodemap.GetNode("ExposureTime"))->SetValue((float)exposure);
		double d2 = CFloatPtr(nodemap.GetNode("ExposureTime"))->GetValue();
		cout << "Exposure Time Current: " << d2 << endl;


		//OUTPUT
		CEnumerationPtr(nodemap.GetNode("LineSelector"))->FromString("Line4");
		CEnumerationPtr(nodemap.GetNode("LineMode"))->FromString("Output");
		//CEnumerationPtr(nodemap.GetNode("LineSource"))->FromString("UserOutput2");
		CEnumerationPtr(nodemap.GetNode("LineSource"))->FromString("FrameTriggerWait");
		//CBooleanPtr(nodemap.GetNode("LineInverter"))->SetValue(true);

		// Select the User Output 2 signal
		CEnumerationPtr(nodemap.GetNode("UserOutputSelector"))->FromString("UserOutput2");
		CBooleanPtr(nodemap.GetNode("UserOutputValue"))->SetValue(true);
		CFloatPtr(nodemap.GetNode("LineMinimumOutputPulseWidth"))->SetValue(100.0);  //microseconds
		// Setting the image Resolution
		CIntegerPtr(nodemap.GetNode("Width"))->SetValue(horz);
		CIntegerPtr(nodemap.GetNode("Height"))->SetValue(vert);
		// Centering image
		CBooleanPtr(nodemap.GetNode("CenterX"))->SetValue(true);
		CBooleanPtr(nodemap.GetNode("CenterY"))->SetValue(true);


		if (bitDepth > 8) {
			SetPixelFormat_unofficial(nodemap, "Mono12");
		}
		else {
			SetPixelFormat_unofficial(nodemap, "Mono8");
		}

		// Set the User Output Value for the User Output 1 signal to true.
		// Because User Output 1 is set as the source signal for Line 2,
		// the status of Line 2 is set to high.
		if (!triggerSelector.CanSetValue(triggerName))
		{
			triggerName = "AcquisitionStart";
			if (!triggerSelector.CanSetValue(triggerName))
			{
				throw RUNTIME_EXCEPTION("Could not select trigger. Neither FrameStart nor AcquisitionStart is available.");
			}
		}
		// Get all enumeration entries of trigger selector.
		Pylon::StringList_t triggerSelectorEntries;
		triggerSelector.GetSettableValues(triggerSelectorEntries);

		// Turn trigger mode off for all trigger selector entries except for the frame trigger given by triggerName.
		for (Pylon::StringList_t::const_iterator it = triggerSelectorEntries.begin(); it != triggerSelectorEntries.end(); ++it)
		{
			// Set trigger mode to off.
			triggerSelector.SetValue(*it);
			if (triggerName == *it)
			{
				// Activate trigger.
				triggerMode.SetValue("On");
				CEnumParameter(nodemap, "TriggerSource").SetValue("Line3");
				////The trigger activation must be set to e.g. 'RisingEdge'.
				CEnumParameter(nodemap, "TriggerActivation").SetValue("RisingEdge");
				CEnumerationPtr(nodemap.GetNode("AcquisitionStatusSelector"))->FromString("FrameTriggerWait");
			}
			else
			{
				triggerMode.SetValue("Off");
			}
			triggerSelector.SetValue(triggerName);
		}
		//Set acquisition mode to "continuous"
		CEnumParameter(nodemap, "AcquisitionMode").SetValue("Continuous");
		cout << "CAMERA SETTINGS APPLIED" << nodemap.GetDeviceName() << endl;
	}
	//Set basic camera settings.
	virtual void OnOpened(CInstantCamera& camera)
	{
		try
		{
			INodeMap& nodemap = camera.GetNodeMap();
			cout << nodemap.GetDeviceName() << endl;
			ApplyConfiguration(camera.GetNodeMap());
		}
		catch (const GenericException& e)
		{
			throw RUNTIME_EXCEPTION("Could not apply configuration. Pylon::GenericException caught in OnOpened method msg=%hs", e.what());
		}
		catch (const std::exception& e)
		{
			throw RUNTIME_EXCEPTION("Could not apply configuration. std::exception caught in OnOpened method msg=%hs", e.what());
		}
		catch (...)
		{
			throw RUNTIME_EXCEPTION("Could not apply configuration. Unknown exception caught in OnOpened method.");
		}
	}
};


//Enumeration used for distinguishing different events.
/* I think the Enum got moved earlier */

void SetPixelFormat_unofficial(INodeMap& nodemap, String_t format) {
	CEnumParameter pixelFormat(nodemap, "PixelFormat");
	String_t oldPixelFormat = pixelFormat.GetValue();
	std::cout << "Old PixelFormat : " << oldPixelFormat << std::endl;
	if (pixelFormat.CanSetValue(format)) {
		pixelFormat.SetValue(format);
		std::cout << "New Pixel Format " << pixelFormat.GetValue() << std::endl;
	}
	else {
		std::cout << "Can't set format!" << std::endl;
	}

	//sleep(2);
}



// Lets See if I can reuse this old code for arg parsing
int scan_opts(int argc, char** argv) {

	std::vector<std::string> opts;
	for (int i = 1; i < argc; i++) {
		std::string incoming = argv[i];
		std::cout << incoming << std::endl;
		opts.push_back(incoming);
	}

	for (int i = 0; i < opts.size(); i++) {
		if (!opts[i].compare("-s")) {
			if (argc > (i)) {
				seconds = atoi(opts[i + 1].c_str());
				i++;
				std::cout << "seconds: " << seconds << std::endl;
			}
			else {
				std::cout << "Malformed input exiting:" << std::endl;
				return -1;
			}
		}
		else if (!opts[i].compare("-horz")) {
			if (argc > (i)) {
				horz = atoi(opts[i + 1].c_str());
				horz_off_set = (horz_max - horz) / 2 + 8;
				i++;
				std::cout << "horz: " << (int)horz << std::endl;
			}
			else {
				std::cout << "Malformed input exiting:" << std::endl;
				return -1;
			}
		}
		else if (!opts[i].compare("-vert")) {
			if (argc > (i)) {
				vert = atoi(opts[i + 1].c_str());
				vert_off_set = (vert_max - vert) / 2 + 8;
				i++;
				std::cout << "vert: " << (int)vert << std::endl;
			}
			else {
				std::cout << "Malformed input exiting:" << std::endl;
				return -1;
			}
		}
		else if (!opts[i].compare("-f")) {
			if (argc > (i)) {
				fps = atoi(opts[i + 1].c_str());
				i++;
				std::cout << "fps: " << (int)fps << std::endl;
			}
			else {
				std::cout << "Malformed input exiting:" << std::endl;
				return -1;
			}
		}
		else if (!opts[i].compare("-bpp")) {
			if (argc >= (i)) {
				bitDepth = atoi(opts[i + 1].c_str());
				i++;
				if (bitDepth > 12 || bitDepth < 8 || bitDepth & 0x1) {
					std::cout << "Not compatible Bit depth 8, 10, 12 bpp only" << std::endl;
					return -1;
				}
				else {
					std::cout << "bitDepth: " << (int)bitDepth << std::endl;
					if (bitDepth > 8) {
						bitDepth = 16;
						std::cout << "Requires " << (int)bitDepth << " bpp of space." << std::endl;
					}
				}
			}
			else {
				std::cout << "Malformed input exiting:" << std::endl;
				return -1;
			}
		}
		else if (!opts[i].compare("-exp")) {
			if (argc >= (i)) {
				exposure = atoi(opts[i + 1].c_str());
				i++;
				std::cout << "exposure time: " << (int)exposure << "us" << std::endl;
			}
			else {
				std::cout << "Malformed input exiting:" << std::endl;
				return -1;
			}
		}
	}
	return 0;

}

// Some More Globals to go with main

USB_THD_DATA usb_thread_data;

int main(int argc, char* argv[])
{
	// The exit code of the sample application.
	int exitCode = 0;
	usb_thread_data.flags = 0;

	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " -s <seconds>" << std::endl;
		std::cout << "Must at least use whole second intervals." << std::endl;
		std::cout << "Other Flags:" << std::endl;
		std::cout << "Capture Area: -horz <pixels> -vert <pixels> (Default 1920 X 1200)" << std::endl;
		std::cout << "Frame Rate: -f <fps>" << std::endl;
		std::cout << "Bit Depth: -bpp <bits> (Default 8bits and supports 12bits but requires 16bpp storage)" << std::endl;
		std::cout << "Exposure Time: -exp <time in us> Defualt 6700us or 6.7ms" << std::endl;
		return exitCode;
	}
	else {
		if (scan_opts(argc, argv) != 0) {
			std::cout << "input error exiting" << std::endl;
			return -1;
		}
		//return 0; // Just testing
	}

	usb_thread_data.flags |= CHANGE_FPS;
	usb_thread_data.fps = fps;

	std::thread USB_THD_OBJ(USB_THREAD, (void*)&usb_thread_data);

	// Before using any pylon methods, the pylon runtime must be initialized. 
	PylonInitialize();

	// Create an example event handler. In the present case, we use one single camera handler for handling multiple camera events.
	// The handler prints a message for each received event.
	//CSampleCameraEventHandler* pHandler1 = new CSampleCameraEventHandler; /*This appears to be unused currently*/

	// Want to create an offset for the cameras for array arithmatic here
	// Once Pycro and Micro Manager TCP IP is implemented we will change this to be user set
	uint64_t image_size = ((horz * vert) / 8)*bitDepth;
	
	// Nice day to try some code.
	try
	{
		// Get the transport layer factory.
		CTlFactory& tlFactory = CTlFactory::GetInstance();

		// Get all attached devices and exit application if no device is found.
		DeviceInfoList_t devices;
		if (tlFactory.EnumerateDevices(devices) == 0)
		{
			throw RUNTIME_EXCEPTION("No camera present.");
		}

		/* Abandon All Instant Camera Array's Ye who enter here*/
		// Create an array of instant cameras for the found devices and avoid exceeding a maximum number of devices.
		//CInstantCameraArray cameras(min(devices.size(), c_maxCamerasToUse));

		// This value should be reported back to the Pycro manager
		unsigned int total_cams = min(devices.size(), c_maxCamerasToUse);

		/* Lets make a humble array of pointers */
		// May as well make all 25
		CInstantCamera* pcam[c_maxCamerasToUse];
		cam_data cam_dat[c_maxCamerasToUse];

		// For keeping track of which index has which serial
		// A switch case could be used to Number the cameras to their position
		// to ease readability and processing if desired.
		std::vector<std::string> serials;
		std::vector<cam_event> events[25];

		// Create and attach all Pylon Devices.
		// We could probably not use the pcam array and just allocate directly to cam_dat
		// Since all of these are declared in the same scope as the lamda functions

		//for (size_t i = 0; i < cameras.GetSize(); ++i) /* Why the pre-increment? */
		for (unsigned int i = 0; i < total_cams; i++)
		{
			pcam[i] = new CInstantCamera(tlFactory.CreateDevice(devices[i]));
			//pcam[i].Attach(tlFactory.CreateDevice(devices[i]));

			INodeMap& nodemap = pcam[i]->GetNodeMap();
			
			//pcam[i].RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete); /* Just say no to their threaded stuff */
			pcam[i]->RegisterConfiguration(new CTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);

			//pcam[i]->GrabCameraEvents = true; /* Not sure we need this for our method? */

			if (!IsAvailable(nodemap.GetNode("EventSelector"))) {
				throw RUNTIME_EXCEPTION("The device doesn't support events.");
			}

			//CEnumerationPtr(nodemap.GetNode("EventSelector"))->FromString("ExposureEnd");
			//CEnumerationPtr(nodemap.GetNode("EventNotification"))->FromString("On");
			//CEnumerationPtr(nodemap.GetNode("EventSelector"))->FromString("FrameStartWait");
			//CEnumerationPtr(nodemap.GetNode("EventNotification"))->FromString("On");
			
			serials.push_back(pcam[i]->GetDeviceInfo().GetSerialNumber().c_str());
			std::cout << "Using device " << pcam[i]->GetDeviceInfo().GetModelName() << " SN " << pcam[i]->GetDeviceInfo().GetSerialNumber() << endl;
			cam_dat[i].number = i;
			cam_dat[i].offset = i * image_size;
			cam_dat[i].camPtr = pcam[i];
			// Moved this out of the thread initilization stuff
			cam_dat[i].camPtr->MaxNumBuffer = 5; // I haven't played with this but it seems fine
			cam_dat[i].camPtr->StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByUser); // Priming the cameras
		}


		// Starts grabbing for all cameras starting with index 0. The grabbing
		// is started for one camera after the other. That's why the images of all
		// cameras are not taken at the same time.
		// However, a hardware trigger setup can be used to cause all cameras to grab images synchronously.
		// According to their default configuration, the cameras are
		// set up for free-running continuous acquisition.

		/* This might need to be initialized in the threads or as threads are created */
		// *** cameras.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);


		//cameras.StartGrabbing(GrabStrategy_LatestImageOnly, GrabLoop_ProvidedByInstantCamera);
		//GrabStrategy_LatestImageOnly



		//Testing to turn on and off the lines
		//bool flag = 0;
		//INodeMap& nodemap = pcam[0]->GetNodeMap();
		//INodeMap& nodemap2 = cameras[1].GetNodeMap();
		//bool status = 0;
		//bool previous = 1;
		//bool finished = false;

		// Should this be monitored in Write Thread?
		uint32_t ImagesRemain = c_countOfImagesToGrab; // Probably Change to Frames_To_Grab
		_mkdir(strDirectryName.c_str());
		/**************************************************/
		/* To be put into the body of the capture threads */
		/**************************************************/

		// To Do: rewrite his into a full function, but how to pass data into it easily?
		// Unfortunately standard barrier does not allow pointers to be made of it
		// nor References, so I need to either make it global or initialize it in the scope
		// of the lamda functions I'm using to make my thread loop.

		// frames will probably need to come from the tcp/ip pycromanager interface
		frames = seconds * fps;
		// How Many Large Binary Chunks of 100 frames we'll Write
		uint64_t binary_chunks = seconds;

		// Our Buffer Size is 100 Frames, which should be 1 second at 100fps
		// Currently we are set to only 8bits and not handling crop factor
		// image_size moved up before cams are attached
		// data_size is the actual size of the data stored which
		// may or may not be byte aligned.

		uint64_t frame_size = image_size * total_cams;
		uint64_t data_size = frame_size * fps;
		uint64_t buff_size = data_size;

	    // Just make sure the buffer is sector aligned
		// Any "Slack" will go unused and be no more than
		// 511 Bytes

		if (buff_size % ALIGNMENT_BYTES) {
			buff_size += (ALIGNMENT_BYTES - (buff_size % ALIGNMENT_BYTES));
		}

		// Allocate Aligned buffers
	    // lets dynamically allocate buffers through testing with windows SDK
		// It was revealed that our NVM uses 512B sectors; therfore, we must
		// allign our data to be written to 512B sectors to do so we can use
		// _aligned_malloc( size_wanted, alignment)
		// must use _aligned_free();

		uint8_t* buff1 = (uint8_t*)_aligned_malloc(buff_size, ALIGNMENT_BYTES);
		uint8_t* buff2 = (uint8_t*)_aligned_malloc(buff_size, ALIGNMENT_BYTES);
		head_buff1 = buff1;
		head_buff2 = buff2;
		uint8_t* in_buff = buff1;
		uint8_t* out_buff = buff2;

		// Attempting to Pre-Initialize Memory
		// To see if it helped early slow start
		// Next I will try to fill the buffers for 200 frames or 2 seconds before
		// Storing data as an attempt to get all of the Caches behaving.

		/*for (uint64_t i = 0; i < buff_size; i++) {
			buff1 = 0;
			buff2 = 0;
		}*/

		// Some Mutex stuff
		std::condition_variable cnt_v; // For sleeping and waking write
		std::mutex lk; // Requred for the condition_variable to sleep.


		// This is the magical mythical buffer swap
		uint32_t swap_counter = 0;
		uint8_t toggle = 0;
		uint8_t begin_writing = 0;
		uint8_t pre_write = 0;
		uint32_t swap_count = 0;


		auto buffer_swap = [&]() noexcept {
			// Currently Swaps Buffer every fps Frames
			//std::cout << "Completed Cycle: " << std::endl;
			if (pre_write > 1) {
				if (frame_count == 0) {
					usb_thread_data.flags |= START_COUNT;
				}
				if (!begin_writing && frame_count == fps - 1) {
					begin_writing = 1;
				}
				frame_count++;
			}
			

			if (frame_count == frames) {
				capture = false;
				usb_thread_data.flags |= STOP_COUNT;
			}

			if (swap_counter >= fps - 1) {
				
				std::cout << "Swapping Count: " << swap_count++ << std::endl;
				if (toggle) {
					in_buff = head_buff1;
					out_buff = head_buff2;
					toggle = !toggle;
				}
				else {
					in_buff = head_buff2;
					out_buff = head_buff1;
					toggle = !toggle;
				}
				// Write Thread Starts off Sleeping
				// Waiting for this function to wake it
				swap_counter = 0;
				// Allow Buffer to fill twice before Collecting data.
				// This will allow both buffers to be initialized
				// Hopefully reducing caching latency.

				if (begin_writing) {
					cnt_v.notify_one(); // Wake me up inside
				}
				else {
					pre_write++;
				}
			}
			else {
				in_buff += frame_size;
				swap_counter++;
			}

		};

		// This Synchronization primitive makes sure all of the cams complete before starting again
		// It also atomically calls buffer swap to handle buffer incrementing and signaling write thread
		std::barrier sync_point(total_cams, buffer_swap);

		capture = true;
		write_count = 0;
		frame_count = 0;

		// This is the begining fo my lambda function for the camera capture threads.
		auto cam_thd = [&](cam_data* cam) {
			
			//cam->camPtr->MaxNumBuffer = 5; // I haven't played with this but it seems fine
			//cam->camPtr->StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByUser); // Priming the cameras

			CGrabResultPtr ptrGrabResult;
			INodeMap& nodemap = cam->camPtr->GetNodeMap();
			//Find if all the cameras are ready

			while (capture) {
				// Wait for an image and then retrieve it. A timeout of 5000 ms is used.
				//auto start = chrono::steady_clock::now();
				cam->camPtr->RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

				// Image grabbed successfully?
				if (ptrGrabResult->GrabSucceeded())
				{	
					// A little Pointer Arithmatic never hurt anybody
					memcpy((void*)(in_buff + cam->offset), (const void*)ptrGrabResult->GetBuffer(), ptrGrabResult->GetPayloadSize());
					if (pre_write > 1) {
						cam_event thd_event;
						thd_event.frame = frame_count;
						//thd_event.missed_frame_count = ptrGrabResult->GetNumberOfSkippedImages();
						//thd_event.sensor_readout = CFloatPtr(nodemap.GetNode("SensorReadoutTime"))->GetValue();   //Microseconds					
						thd_event.time_stamp = ptrGrabResult->GetTimeStamp() / 1.0;
						events[cam->number].push_back(thd_event);
					}
					
					// Hurry up and wait
					sync_point.arrive_and_wait();
				}
				else
				{
					// Give us an error message.  Camera 14 is the only one I've seen hit this.
					std::cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << " cam: " << serials[cam->number] << endl;
					// Hurry up and wait
					sync_point.arrive_and_wait();
				}
				//auto end = chrono::steady_clock::now();
				//long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

				//std::cout << "Taken Time for saving image to ram: " << (int)cam->number << " " << elapsed << "us" << endl;
				
			}
		};

		// struct for write thread;
		write_data mr_write;
		// becomes false after desired frames grabbed.
		mr_write.first = true;
		mr_write.cam_count = total_cams;

		// Write Thread "Lamda Function"
		auto write_thrd = [&](write_data* ftw) {
			
			while (write_count < binary_chunks)  {
				// Takes the lock then decides to take a nap
				// Until the buffer is ready to write
				std::unique_lock<std::mutex> lck(lk);
				cnt_v.wait(lck);
				
				//auto start = chrono::steady_clock::now();
				std::string Filename = strDirectryName + "\\binary_chunk_" + std::to_string(write_count) + ".bin";
				saveBigBuffer(Filename.c_str(), out_buff, ftw->cam_count, buff_size);
				write_count++;
				//auto end = chrono::steady_clock::now();
				//long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

				//std::cout << "Taken Time for writing frame:" << write_count << " " << elapsed << "us" << endl;
			}
		};

		std::cout << "Building Threads: " << std::endl;
		std::vector<std::thread> threads;
		for (int i = 0; i < total_cams; i++) {
			threads.emplace_back(cam_thd, &cam_dat[i]);
		}

		threads.emplace_back(write_thrd, &mr_write);
		auto start = chrono::steady_clock::now();
		// Join the Threads. This should block until capture done
		for (auto& thread: threads) {
			thread.join();
		}

		auto end = chrono::steady_clock::now();
		long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();
		std::cout << "Total Time: " << elapsed << "us" << endl;
		

		std::cout << "Image Aquisition Finished" << std::endl;

		std::cout << "Converting images to tif" << std::endl;
		// This is the binary to tiff image conversion section.  It would probably be a good idea to thread this
		// to boost the write throughput more. It should be noted that we are currently unable to 
		// Write to USB external drives for some odd reason.

		// This is tripple Nested... Is there a better way to do this?

		// Outer loop which CHUNK are we reading
		for (int i = 0; i < binary_chunks; i++) {
			std::string Filename = strDirectryName + "\\binary_chunk" + std::to_string(i) + ".bin";
			uint64_t outNumberofBytes;
			//std::cout << "expected size: " << sizeof(frame_buffer) * total_cams << std::endl;
			readFile(Filename.c_str(), &outNumberofBytes, buff1);
			//std::cout << "size read: " << outNumberofBytes << std::endl;

			// Middle Loop which frame index are we readng from the chunk?
			for (int j = 0; j < fps; j++) {
				std::cout << ".";

				// Inner Loop Which Camera are we reading from the chunk
				for (int k = 0; k < total_cams; k++) {
					//std::string Dir = "camera" + std::to_string(k);
					//std::string tiff_folders = "\\.\PhysicalDrive1\\Ant1 Test\\" + serials[k];
					/*if (*/_mkdir(serials[k].c_str());/*) {
						std::cout << "but why can't I write: " << std::endl;
					}*/
					std::string filename = serials[k] + "\\image" + std::to_string(j + i*fps) + ".tif";

					CPylonImage srcImage;
					if(bitDepth > 8){
						srcImage.AttachUserBuffer((void*)(buff1 + (k * image_size) + (j * frame_size)), image_size, PixelType_Mono16, horz, vert, 0);
					}
					else {
						srcImage.AttachUserBuffer((void*)(buff1 + (k * image_size) + (j * frame_size)), image_size, PixelType_Mono8, horz, vert, 0);
					}
					if (CImagePersistence::CanSaveWithoutConversion(ImageFileFormat_Tiff, srcImage)) {
						CImagePersistence::Save(ImageFileFormat_Tiff, String_t(filename.c_str()), srcImage);
					}
					else {
						std::cout << "not a tiff needs conversion" << std::endl;
					}
					/*if ((events[k][j].missed_frame_count - events[k][0].missed_frame_count)) {
						std::cout << "Missed Frame Count cam: " << serials[k] << " = " << events[k][j].missed_frame_count - events[k][0].missed_frame_count << std::endl;
					}*/
				}
			}
		}

		std::cout << std::endl << "Finished Converting to tif" << std::endl;

		// Checking For Longer than acceptable Frame Times
		for (int i = 0; i < 25; i++) {
			int64_t prev = events[i][0].time_stamp;
			int64_t first_miss_cnt = events[i][0].missed_frame_count;
			for (int j = 1; j < frames; j++) {
				if ((abs(events[i][j].time_stamp - prev)) > float(50000) + 1/((float)fps)*1e9) {
					std::cout << "camera: " << i << std::endl;
					std::cout << "Current: " << events[i][j].time_stamp << " prev: " << prev << std::endl;
					std::cout << "Abnormal Time Diff: " << events[i][j].time_stamp - prev << " at frame: " << events[i][j].frame << std::endl;
					//std::cout << "Sensor Readout: " << events[i][j].sensor_readout << std::endl;
					//std::cout << " Missed Frame Count: " << events[i][j].missed_frame_count - first_miss_cnt << std::endl;
				}
				prev = events[i][j].time_stamp;
			}

		}

		// Free These Aligned Buffers PLEASE!
		_aligned_free(buff1);
		_aligned_free(buff2);
		usb_thread_data.flags |= EXIT_THREAD;
		USB_THD_OBJ.join();
		//auto start = chrono::steady_clock::now();
		//cout << "Before FileName" << endl;
		//FileName << strDirectryName << "MyBigFatGreekWedding" << ".bin";
		//cout << "After FileName: " << FileName.str() << endl;
		//saveBigBuffer("MyBigFatGreekWedding.bin"/*FileName.str().c_str()*/ , test_buffer);
		//auto end = chrono::steady_clock::now();
		//long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();
		//cout << "Time to write out buffer to disk: " << elapsed << "us" << std::endl;
		//cout << "Total time grabbing and writing: " << elapsed + total_time << "us" << std::endl;
		//cout << "Size writen: " << sizeof(frame_buffer) * c_countOfImagesToGrab << endl;
		//cout << "Throughput: " << sizeof(frame_buffer) * c_countOfImagesToGrab / (elapsed * 1e-6) << endl;


		//while (1);
		// after grabbing has been done, write the meta file in same directory 
		//Save_Metadata(Metasdata, strDirectryName + strMetaFileName);
		//if we substitute the while loop for the for loop and the retrieve the method works but it only takes half of the images since it is dumping half.

	}
	catch (const GenericException& e)
	{
		// Error handling
		cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
		exitCode = 1;
		usb_thread_data.flags |= EXIT_THREAD;
		USB_THD_OBJ.join();
	}

	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press Enter to exit." << endl;
	while (cin.get() != '\n');

	// Releases all pylon resources. 
	PylonTerminate();
	

	return exitCode;
}
