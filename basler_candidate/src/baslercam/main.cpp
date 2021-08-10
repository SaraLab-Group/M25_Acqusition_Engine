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
//#pragma once
//#include "project_headers.h"
#include "USB_THREAD.h" // For USB function pointer used for threading.
#include "server_thread.h" // For inter process communication
#include <Windows.h> //Needed For windows CreateFile and WriteFile File Handle libraries.



// For array size
static const uint8_t EIGHT_BIT = 8;
static const uint8_t SIXTEEN_BIT = 16;


// This Value is mostly for testing as we may apply a crop factor
static const uint32_t PIX_OVER_8 = 288000; // This is (1920*1200)/8 makes it easier to multiply by 8 or 16bit


// This Value was for testing manual buffering of std::ofstream
// static const uint32_t FS_BUF_SIZE = 131072;//524288; //2097152;//262144;


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
uint32_t bitDepth = 8;
uint32_t fps = 65;


double old = 0.0; //variable to test trigger delay

// Mutex for thread signals
std::mutex crit;
std::mutex crit2;
std::mutex crit3; // For USB SERVER data;

//A prototype
void SetPixelFormat_unofficial(INodeMap& nodemap, String_t format);


//Directory name images to be saved.
// Should probably be set from pycro or micro manager.
string  strDirectryName = "D:\\Ant1 Test";//\\binaries";
// From Basler Fast Write Example Not currently used;
string  strMetaFileName = "Meta.txt";

// it tworks
string tiff_dir = "D:\\Ant1 Test";// \\Tiff";


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
	uint8_t* pImageBuffer = (uint8_t*)buffer;
	uint64_t bytes_written = 0;

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
		CREATE_ALWAYS,             // create Always
		FILE_FLAG_NO_BUFFERING,  // Nobuffering file
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
{   
	ifstream fs(fileName, std::ifstream::binary);
	fs.seekg(0, fs.end);
	long long int size = fs.tellg();
	fs.seekg(0, fs.beg);
	//fs.read((char*)chunk, size);
	//char* bufferTemp = new char[size];
	// allocate memory for file content
	/*if (fs)
		std::cout << "all characters read successfully.";
	else
		std::cout << "error: only " << fs.gcount() << " could be read" << std::endl;
	std::cout << "size: " << size << std::endl;*/
	//buffer->push_back(bufferTemp);
	//returning the file size, needed for converting the buffer into an Pylon image into a bitmap
	*outNumberofBytes = size;
	fs.close();

	/*HFILE OpenFile(
		LPCSTR     lpFileName,
		LPOFSTRUCT lpReOpenBuff,
		UINT       uStyle
	);*/

	HANDLE hFile = INVALID_HANDLE_VALUE;
	LPOFSTRUCT lpReOpenBuff;

	DWORD dwBytesToRead = (DWORD)MAX_ALIGNED_WRITE;//sizeof(frame_buffer) * c_countOfImagesToGrab;
	if (size < dwBytesToRead) {
		dwBytesToRead = (DWORD)size;//sizeof(frame_buffer) * c_countOfImagesToGrab;
	}

	hFile = CreateFile(fileName,                // name of the read
		GENERIC_READ,          // open for reading
		0,                      // do not share
		NULL,                   // default security
		OPEN_EXISTING,             // open existing file only
		FILE_FLAG_NO_BUFFERING,  // nobuffer
		NULL);                  // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		//DisplayError(TEXT("CreateFile"));
		//_tprintf(TEXT("Terminal failure: Unable to open file \"%s\" for write.\n"), argv[1]);
		//return;
		std::cout << "Awe Shucks Failed to open file" << std::endl;
	}

	uint8_t* pImageBuffer = (uint8_t*)chunk;
	uint64_t bytes_read = 0;

	DWORD dwBytesRead = 0;
	BOOL bErrorFlag = FALSE;

	//printf("Writing %d bytes to %s.\n", dwBytesToWrite, FileName);
	while (bytes_read < size) {
		bErrorFlag = ReadFile(
			hFile,           // open file handle
			pImageBuffer,      // start of data to write
			dwBytesToRead,  // number of bytes to write
			&dwBytesRead, // number of bytes that were written
			NULL);            // no overlapped structure

		if (FALSE == bErrorFlag)
		{
			//DisplayError(TEXT("WriteFile"));
			printf("Terminal failure: Unable to write to file.\n");
		}
		else
		{
			if (dwBytesRead != dwBytesToRead)
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
			bytes_read += dwBytesRead;
			std::cout << "^";
			if (size - bytes_read < dwBytesToRead) {
				dwBytesToRead = size - bytes_read;
			}
		}
	}
	// Sanity Check
	//std::cout << "BytesRead: " << bytes_read << std::endl;
	//std::cout << "file_size: " << size << std::endl;

	CloseHandle(hFile);
}


// Make a directory tree???
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


/* This Class is --Wizardy-- great configures camera parameters for us */
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
/*int scan_opts(int argc, char** argv) {

	std::vector<std::string> opts;
	for (int i = 1; i < argc; i++) {
		std::string incoming = argv[i];
		std::cout << incoming << std::endl;
		opts.push_back(incoming);
	}

	for (int i = 0; i < opts.size(); i++) {
		if (!opts[i].compare("-s")) {
			if (argc > (i) && opts[i + 1].c_str()[0] != '-') {
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
			if (argc > (i) && opts[i + 1].c_str()[0] != '-') {
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
			if (argc > (i) && opts[i + 1].c_str()[0] != '-') {
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
			if (argc > (i) && opts[i + 1].c_str()[0] != '-') {
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
			if (argc >= (i) && opts[i + 1].c_str()[0] != '-') {
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
			if (argc >= (i) && opts[i + 1].c_str()[0] != '-') {
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

}*/


// Prototypes because to many things defined before main and I don't like it
int aquire_cameras(std::vector<std::string>* serials, cam_data* cam_dat, unsigned int* total_cams, uint64_t* image_size);
void start_capture(std::vector<std::string>* serials, cam_data* cam_dat, unsigned int* total_cams, uint64_t* image_size);

// Some More Globals to go with main
USB_THD_DATA usb_thread_data;
usb_data usb_incoming, usb_outgoing;
SERVER_THD_DATA server_thread_data;
std::condition_variable signal_main;
std::mutex sleep_loop;
TCP_IP_DAT incoming, outgoing;
std::vector<cam_event> events[25]; // This is for testing for dropped frames
bool active = true;


int main(int argc, char* argv[])
{
	// The exit code of the sample application.
	int exitCode = 0;

	// Just setting defaults not really needed 
	incoming.horz = horz;
	incoming.vert = vert;
	usb_outgoing.fps = incoming.fps = fps;
	incoming.exp = exposure;
	incoming.bpp = bitDepth;
	incoming.capTime = seconds;
	usb_incoming.flags = incoming.flags = 0;

	server_thread_data.incoming_data = &incoming;
	server_thread_data.outgoing_data = &outgoing;
	server_thread_data.usb_incoming = &usb_incoming;
	server_thread_data.usb_outgoing = &usb_outgoing;
	server_thread_data.signal_ptr = &signal_main;
	server_thread_data.mtx_ptr = &crit2;
	server_thread_data.usb_srv_mtx = &crit3;
	
	usb_thread_data.incoming = &usb_incoming;
	usb_thread_data.outgoing = &usb_outgoing;

	usb_thread_data.crit = &crit;
	usb_thread_data.usb_srv_mtx = &crit3;

	// Start the USB and Server Threads
	std::thread USB_THD_OBJ(USB_THREAD, (void*)&usb_thread_data);
	std::thread SRVR_THD_OBJ(SERVER_THREAD, (void*)&server_thread_data);

    
	cam_data cam_dat[c_maxCamerasToUse];

	// For keeping track of which index has which serial
    // A switch case could be used to Number the cameras to their position
    // to ease readability and processing if desired.

	std::vector<std::string> serials;
	unsigned int total_cams;
	uint64_t image_size = ((horz * vert) / 8) * bitDepth;;


	// This will act as the primary interface to change configurations, aquire cameras, start image capture, etc.
	// I am using bitwise logical comparisons to check status and configuration flags.
	while (active) {
		//printf("Top of Main While\n");
		std::unique_lock slp(sleep_loop);
		//printf("Sleeping Self\n");
		signal_main.wait(slp);
		printf("Waking Up\n");
		std::unique_lock prot(crit2);
		if (incoming.flags & ACQUIRE_CAMERAS) {
			incoming.flags &= ~ACQUIRE_CAMERAS;
			outgoing.flags |= ACQUIRING_CAMERAS;
			printf("Acquiring\n");
			prot.unlock();
			if (aquire_cameras(&serials, cam_dat, &total_cams, &image_size)) {
				prot.lock();
				outgoing.flags |= ACQUIRE_FAIL;
				prot.unlock();
			}
			else {
				prot.lock();
				outgoing.flags |= CAMERAS_ACQUIRED;
				outgoing.flags &= ~ACQUIRING_CAMERAS;
				prot.unlock();
			}
		}
		else if (incoming.flags & RELEASE_CAMERAS && outgoing.flags & CAMERAS_ACQUIRED) {
			incoming.flags &= ~RELEASE_CAMERAS;
			printf("Release Cameras\n");
			prot.unlock();
			// Releases all pylon resources. 
			PylonTerminate();
			prot.lock();

			outgoing.flags &= ~(CAMERAS_ACQUIRED | ACQUIRE_FAIL);
			prot.unlock();
		}
		else if (incoming.flags & CHANGE_CONFIG && ~(outgoing.flags & (CAPTURING |CAMERAS_ACQUIRED))) {

			printf("Change Config\n");

			horz = incoming.horz;
			vert = incoming.vert;
			exposure = incoming.exp;
			bitDepth = incoming.bpp;
			fps = incoming.fps;
			seconds = incoming.capTime;
			tiff_dir = incoming.path;
			image_size = ((horz * vert) / 8) * bitDepth;
			//prot.unlock();

			/*if (outgoing.flags & CAMERAS_ACQUIRED) {
				for (int i = 0; i < total_cams; i++) {
					printf("Does this ever get here?\n");
					cam_dat[i].camPtr->RegisterConfiguration(new CTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);
				}
			}*/
			//prot.lock();
			incoming.flags &= ~CHANGE_CONFIG;
			prot.unlock();
		}
		else if (incoming.flags & START_CAPTURE && ~(outgoing.flags & (CAPTURING | CONVERTING)) && outgoing.flags & CAMERAS_ACQUIRED) {
			incoming.flags &= ~START_CAPTURE;
			outgoing.flags |= CAPTURING;
			prot.unlock();
			start_capture(&serials, cam_dat, &total_cams, &image_size);
		}
		else if (incoming.flags & EXIT_THREAD) {
			usb_outgoing.flags |= EXIT_USB;
			prot.unlock();
			active = false;
		}
		else {
			// Ultimately We shouldn't ever get here
			// Since that means we received a false wakeup signal
			prot.unlock();
		}
		

	}





		// usb_thread_data.incoming->flags |= EXIT_THREAD;
		USB_THD_OBJ.join();
		SRVR_THD_OBJ.join();
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



	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press Enter to exit." << endl;
	while (cin.get() != '\n');
	

	return exitCode;
}

int aquire_cameras(std::vector<std::string> *serials, cam_data* cam_dat, unsigned int* total_cams, uint64_t* image_size) {
	// Before using any pylon methods, the pylon runtime must be initialized. 
	PylonInitialize();

	// Create an example event handler. In the present case, we use one single camera handler for handling multiple camera events.
	// The handler prints a message for each received event.
	//CSampleCameraEventHandler* pHandler1 = new CSampleCameraEventHandler; /*This appears to be unused currently*/

	// Want to create an offset for the cameras for array arithmatic here
	// Once Pycro and Micro Manager TCP IP is implemented we will change this to be user set
	

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
		*total_cams = min(devices.size(), c_maxCamerasToUse);

		/* Lets make a humble array of pointers */
		// May as well make all 25
		CInstantCamera* pcam[c_maxCamerasToUse];
		


		// std::vector<std::string> serials;
		// std::vector<cam_event> events[25];

		// Create and attach all Pylon Devices.
		// We could probably not use the pcam array and just allocate directly to cam_dat
		// Since all of these are declared in the same scope as the lamda functions

		//for (size_t i = 0; i < cameras.GetSize(); ++i) /* Why the pre-increment? */
		for (unsigned int i = 0; i < *total_cams; i++)
		{
			pcam[i] = new CInstantCamera(tlFactory.CreateDevice(devices[i]));
			//pcam[i].Attach(tlFactory.CreateDevice(devices[i]));

			INodeMap& nodemap = pcam[i]->GetNodeMap();

			//pcam[i].RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete); /* Just say no to their threaded stuff */

			// This should set Default Configuration if no configuration change applied.
			pcam[i]->RegisterConfiguration(new CTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);

			//pcam[i]->GrabCameraEvents = true; /* Not sure we need this for our method? */

			if (!IsAvailable(nodemap.GetNode("EventSelector"))) {
				throw RUNTIME_EXCEPTION("The device doesn't support events.");
			}

			serials->push_back(pcam[i]->GetDeviceInfo().GetSerialNumber().c_str());
			std::cout << "Using device " << pcam[i]->GetDeviceInfo().GetModelName() << " SN " << pcam[i]->GetDeviceInfo().GetSerialNumber() << endl;
			cam_dat[i].number = i;
			cam_dat[i].offset = i * (*image_size);
			cam_dat[i].camPtr = pcam[i];
			// Moved this out of the thread initilization stuff
			cam_dat[i].camPtr->MaxNumBuffer = 5; // I haven't played with this but it seems fine
			cam_dat[i].camPtr->StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByUser); // Priming the cameras
		}
		return 0;
	}
	catch (const GenericException& e)
	{
		// Error handling
		cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
		//exitCode = 1;
		//usb_thread_data.incoming_data->flags |= EXIT_THREAD;
		//USB_THD_OBJ.join();
		return 1;
	}
}

// A really Beefy Function.  Handles all of the Capture and Convert.
void start_capture(std::vector<std::string>* serials, cam_data* cam_dat, unsigned int* total_cams, uint64_t* image_size) {
	// Should this be monitored in Write Thread?
	uint32_t ImagesRemain = c_countOfImagesToGrab; // Probably Change to Frames_To_Grab

	// Create the Write Directory "Root"
	_mkdir(strDirectryName.c_str());

	//append path for destination folder
	//strDirectryName += "\\binaries";

	_mkdir(tiff_dir.c_str());

	//tiff_dir += "\\tiff";

	// create subfolders
	_mkdir(strDirectryName.c_str());
	_mkdir(tiff_dir.c_str());

	/**************************************************/
	/* To be put into the body of the capture threads */
	/**************************************************/

	// To Do: rewrite his into a full function, but how to pass data into it easily?
	// Unfortunately standard barrier does not allow pointers to be made of it
	// nor References, so I need to either make it global or initialize it in the scope
	// of the lamda functions I'm using to make my thread loop.

	// frames will probably need to come from the tcp/ip pycromanager interface
	frames = seconds * fps;
	printf("frames: %u\n", frames);
	// How Many Large Binary Chunks of 100 frames we'll Write
	uint64_t binary_chunks = seconds;

	// Our Buffer Size is 100 Frames, which should be 1 second at 100fps
	// Currently we are set to only 8bits and not handling crop factor
	// image_size moved up before cams are attached
	// data_size is the actual size of the data stored which
	// may or may not be byte aligned.

	uint64_t frame_size = (*image_size) * (*total_cams);
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
	std::mutex ded; // Prevent Write getting behind


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
				std::cout << "START_COUNT" << std::endl;
				std::unique_lock<std::mutex> flg(crit);
				usb_thread_data.outgoing->flags |= START_COUNT;
				flg.unlock();
			}
			if (!begin_writing && frame_count == fps - 1) {
				begin_writing = 1;
			}
			frame_count++;
		}


		if (frame_count == frames) {
			capture = false;
			std::cout << "START_COUNT" << std::endl;
			std::unique_lock<std::mutex> flg(crit);
			usb_thread_data.outgoing->flags |= STOP_COUNT;
			flg.unlock();
			//cnt_v.notify_one(); // Wake me up inside
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
				std::unique_lock<std::mutex> mtx(ded); // Really this lock blocks from moving forward until the write thread is ready to be woken.
				//mtx.lock();
				cnt_v.notify_one(); // Wake me up inside
				mtx.unlock(); // unlunk
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
	std::barrier sync_point(*total_cams, buffer_swap);

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
				std::cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << " cam: " << (*serials)[cam->number] << endl;
				// Hurry up and wait
				sync_point.arrive_and_wait();
			}
			//auto end = chrono::steady_clock::now();
			//long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

			//std::cout << "Taken Time for saving image to ram: " << (int)cam->number << " " << elapsed << "us" << endl;

		}
		//std::cout << "thd: " << (int)cam->number << " joining" << std::endl;
	};

	// struct for write thread;
	write_data mr_write;
	// becomes false after desired frames grabbed.
	mr_write.first = true;
	mr_write.cam_count = *total_cams;

	// Write Thread "Lamda Function"
	auto write_thrd = [&](write_data* ftw) {
		// This Mutex is for preventing the write thread from getting 
		// behind the Read Threads and miss it's wake signal from
		// The Barier Completion function
		std::unique_lock<std::mutex> mtx(ded);

		while (write_count < binary_chunks) {
			// Takes the lock then decides to take a nap
			// Until the buffer is ready to write

			// This if statement is a crutch to prevent an early attempt to wake the thread
			// on the last write call.
			if (capture) {
				std::unique_lock<std::mutex> lck(lk); // lock for control signal.
				mtx.unlock(); // ded mutex
				cnt_v.wait(lck); // Woken by Barrier Completion
				mtx.lock(); // ded mutex This is for preventing the buffer swap thread from waking write thread before it's finished
			}
			//std::cout << "Past Lock" << std::endl;

			//auto start = chrono::steady_clock::now();
			std::string Filename = strDirectryName + "\\binaries" + "\\binary_chunk_" + std::to_string(write_count) + ".bin";
			saveBigBuffer(Filename.c_str(), out_buff, ftw->cam_count, buff_size);
			write_count++;
			//auto end = chrono::steady_clock::now();
			//long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

			//std::cout << "Taken Time for writing frame:" << write_count << " " << elapsed << "us" << endl;
		}
		//std::cout << "Write thd joining" << std::endl;
	};

	std::cout << "Building Threads: " << std::endl;
	std::vector<std::thread> threads;
	for (int i = 0; i < *total_cams; i++) {
		threads.emplace_back(cam_thd, &cam_dat[i]);
	}

	threads.emplace_back(write_thrd, &mr_write);
	auto start = chrono::steady_clock::now();
	// Join the Threads. This should block until capture done
	for (auto& thread : threads) {
		thread.join();
	}

	auto end = chrono::steady_clock::now();
	long long elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();
	std::cout << "Total Time: " << elapsed << "us" << endl;
	std::cout << "Total Time Seconds: " << elapsed / (double)1e6 << "s" << std::endl;


	std::cout << "Image Aquisition Finished" << std::endl;

	std::unique_lock<std::mutex> flg(crit2);
	outgoing.flags &= ~CAPTURING;
	flg.unlock();

	// Sanity Check
	//uint32_t val = 255;

	/*for (int i = 0; i < buff_size; i++) {
		buff1[i] = 255;
	}*/

#ifdef CONVERT_TIFF

	flg.lock();
	outgoing.flags |= CONVERTING;
	flg.unlock();

	std::cout << "Converting images to tif" << std::endl;
	// This is the binary to tiff image conversion section.  It would probably be a good idea to thread this
	// to boost the write throughput more. It should be noted that we are currently unable to 
	// Write to USB external drives for some odd reason.

	// I'm reusing the Barrier Mutex concept from earlier

	/* This is the end condition lamda for the barrier mutex it load the Chunk to be
		Split into individual tif files by the worker threads. */

	uint8_t write_files = 1;
	uint32_t chunk_number = 0;
	uint16_t save_threads = 5; // Five seems like a magic number
	if (fps < 5) {
		save_threads = 1;
	}

	std::vector<uint8_t> thread_row;

	for (int i = 0; i < save_threads; i++) {
		uint16_t row = i;
		thread_row.push_back(row);
	}

	auto completion_condition = [&]() noexcept {
		//std::cout << "completion has happened" << std::endl;
		for (int i = 0; i < save_threads; i++) {
			thread_row[i] += save_threads;
		}
		if (thread_row[save_threads - 1] > fps) {
			chunk_number++;
			//std::cout << " " << chunk_number << " ";
			if (chunk_number > binary_chunks - 1) {
				// Don't read more files
				write_files = 0;
				//std::cout << "^";
			}
			else {
				std::string Filename = strDirectryName + "\\binaries" + "\\binary_chunk_" + std::to_string(chunk_number) + ".bin";
				uint64_t outNumberofBytes;
				//std::cout << "expected size: " << sizeof(frame_buffer) * total_cams << std::endl;
				readFile(Filename.c_str(), &outNumberofBytes, buff1);
				for (int i = 0; i < save_threads; i++) {
					thread_row[i] = i;
				}
			}
		}

	};

	std::barrier sync_point2(save_threads, completion_condition);

	// re using cam_data out of convinience

	auto save_img = [&](cam_data* cam) {


		while (write_files) {
			for (int i = 0; i < *total_cams; i++) {
				std::string tiff_path = tiff_dir + "\\" + (*serials)[i];
				_mkdir(tiff_path.c_str()); //make the dir
				std::string filename = tiff_path + "\\image" + std::to_string(i + thread_row[cam->number] + chunk_number * fps) + ".tif";
				//std::string filename = serials[i] + "\\image" + std::to_string(i + thread_row[cam->number] + chunk_number * fps) + ".tif";

				CPylonImage srcImage;
				if (bitDepth > 8) {
					srcImage.AttachUserBuffer((void*)(buff1 + (i * (*image_size)) + (thread_row[cam->number] * frame_size)), *image_size, PixelType_Mono16, horz, vert, 0);
				}
				else {
					srcImage.AttachUserBuffer((void*)(buff1 + (i * (*image_size)) + (thread_row[cam->number] * frame_size)), *image_size, PixelType_Mono8, horz, vert, 0);
				}
				if (CImagePersistence::CanSaveWithoutConversion(ImageFileFormat_Tiff, srcImage)) {
					// Making Write Atomic Just in case.
					// Reusing the Mutext from earlier.
					//std::unique_lock<std::mutex> lck(lk);
					CImagePersistence::Save(ImageFileFormat_Tiff, String_t(filename.c_str()), srcImage);
					//lck.unlock();
				}
				else {
					std::cout << "not a tiff needs conversion" << std::endl;
				}
			}
			std::cout << '.';
			sync_point2.arrive_and_wait();
		}
		//std::cout << "thread: " << (int)cam->number << " exiting." << std::endl;
	};


	/* Load First Buffer Before Starting Threads */
	/* chunk_number should already be set to 0 */

	start = chrono::steady_clock::now();

	std::string Filename = strDirectryName + "\\binaries" + "\\binary_chunk_" + std::to_string(chunk_number) + ".bin";
	uint64_t outNumberofBytes;
	//std::cout << "expected size: " << sizeof(frame_buffer) * total_cams << std::endl;
	readFile(Filename.c_str(), &outNumberofBytes, buff1);

	threads.clear();// purge old threads

	// Being Lazy and recycling the same syntax
	for (int i = 0; i < save_threads; i++) {
		threads.emplace_back(save_img, &cam_dat[i]);
	}

	// Join the Threads. This should block until write done
	for (auto& thread : threads) {
		thread.join();
	}
	end = chrono::steady_clock::now();
	elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();
	std::cout << std::endl;
	std::cout << "Total Time To Write Tiff: " << elapsed << "us" << std::endl;
	std::cout << "Total Time To Write in Seconds: " << elapsed / (double)1e6 << "s" << std::endl;
	flg.lock();
	outgoing.flags &= ~CONVERTING;
	flg.unlock();

#endif



	std::cout << std::endl << "Finished Converting to tif" << std::endl;

	uint32_t max_dropped = 0;
	// Checking For Longer than acceptable Frame Times
	for (int i = 0; i < 25; i++) {
		uint32_t dropped_frames = 0;
		int64_t prev = events[i][0].time_stamp;
		int64_t first_miss_cnt = events[i][0].missed_frame_count;
		for (int j = 1; j < frames; j++) {
			if (events[i].size() > j) {
				if ((abs(events[i][j].time_stamp - prev)) >  5.0 / ((float)fps * 4) * 1e9) {
					std::cout << "camera: " << i << std::endl;
					//std::cout << "Current: " << events[i][j].time_stamp << " prev: " << prev << std::endl;
					std::cout << "Abnormal Time Diff: " << events[i][j].time_stamp - prev << " at frame: " << events[i][j].frame << std::endl;
					dropped_frames++;
					//std::cout << "Sensor Readout: " << events[i][j].sensor_readout << std::endl;
					//std::cout << " Missed Frame Count: " << events[i][j].missed_frame_count - first_miss_cnt << std::endl;
				}

				prev = events[i][j].time_stamp;
			}
			else {
				std::cout << "This Vector has: " << events[i].size() << " elements vs. " << (int)frames << " frames." << std::endl;
			}
		}
		std::cout << ".";
		max_dropped = max(max_dropped, dropped_frames);
	}
	std::cout << std::endl;
	std::cout << "Dropped Frames: " << (int)max_dropped << " Total Frames: " << (int)frames << std::endl;
	std::cout << "Dropped Ratio: " << (double)max_dropped / (double)frames << std::endl;

	// Free These Aligned Buffers PLEASE!
	_aligned_free(buff1);
	_aligned_free(buff2);

	// Clear Events for another Capture
	for (int i = 0; i < 25; i++) {
		events[i].clear();
	}
	// Clear Threads
	threads.clear();
}