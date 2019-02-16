#include <iostream>
#include <stdio.h>
#include "ftd2xx.h"
#include "jtag.h"
#include "jtag_fsm.h"
#include "loader.h"
#include "spi.h"
#include <unistd.h>
#include <stack>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>
#include <iterator>
#include "loader.h"
#include <chrono>
#ifdef _WIN32
#include "mingw.thread.h"
#else
#include <thread>
#endif
#include <cstring>
#include "config_type.h"

#define BOARD_ERROR -2
#define BOARD_UNKNOWN -1
#define BOARD_AU 0
#define BOARD_CU 1

using namespace std;
using get_time = chrono::steady_clock;

char ManufacturerBuf[32];
char ManufacturerIdBuf[16];
char DescriptionBuf[64];
char SerialNumberBuf[16];

string getErrorName(int error) {
	switch (error) {
	case FT_OK:
		return "FT_OK";
	case FT_INVALID_HANDLE:
		return "FT_INVALID_HANDLE";
	case FT_DEVICE_NOT_FOUND:
		return "FT_DEVICE_NOT_FOUND";
	case FT_DEVICE_NOT_OPENED:
		return "FT_DEVICE_NOT_OPENED";
	case FT_IO_ERROR:
		return "FT_IO_ERROR";
	case FT_INSUFFICIENT_RESOURCES:
		return "FT_INSUFFICIENT_RESOURCES";
	case FT_INVALID_PARAMETER:
		return "FT_INVALID_PARAMETER";
	case FT_INVALID_BAUD_RATE:
		return "FT_INVALID_BAUD_RATE";

	case FT_DEVICE_NOT_OPENED_FOR_ERASE:
		return "FT_DEVICE_NOT_OPENED_FOR_ERASE";
	case FT_DEVICE_NOT_OPENED_FOR_WRITE:
		return "FT_DEVICE_NOT_OPENED_FOR_WRITE";
	case FT_FAILED_TO_WRITE_DEVICE:
		return "FT_FAILED_TO_WRITE_DEVICE";
	case FT_EEPROM_READ_FAILED:
		return "FT_EEPROM_READ_FAILED";
	case FT_EEPROM_WRITE_FAILED:
		return "FT_EEPROM_WRITE_FAILED";
	case FT_EEPROM_ERASE_FAILED:
		return "FT_EEPROM_ERASE_FAILED";
	case FT_EEPROM_NOT_PRESENT:
		return "FT_EEPROM_NOT_PRESENT";
	case FT_EEPROM_NOT_PROGRAMMED:
		return "FT_EEPROM_NOT_PROGRAMMED";
	case FT_INVALID_ARGS:
		return "FT_INVALID_ARGS";
	case FT_NOT_SUPPORTED:
		return "FT_NOT_SUPPORTED";
	case FT_OTHER_ERROR:
		return "FT_OTHER_ERROR";
	case FT_DEVICE_LIST_NOT_READY:
		return "FT_DEVICE_LIST_NOT_READY";
	}
	return "Unknown";
}

void write_to_file(string file, PFT_PROGRAM_DATA ftData) {
	ofstream output_file(file, ios::binary);
	CONFIG_DATA config;
	ft_to_config(&config, ftData);
	output_file.write((char*) &config, sizeof(CONFIG_DATA));
	output_file.write(ManufacturerBuf, sizeof(ManufacturerBuf));
	output_file.write(ManufacturerIdBuf, sizeof(ManufacturerIdBuf));
	output_file.write(DescriptionBuf, sizeof(DescriptionBuf));
	output_file.write(SerialNumberBuf, sizeof(SerialNumberBuf));
	output_file.close();
}

bool read_from_file(string file, PFT_PROGRAM_DATA ftData) {
	cout << "Reading " << file << endl;
	try {
		ifstream input_file(file, ios::in | ios::binary);
		if (!input_file.is_open()) {
			cerr << "Failed to open file " << file << endl;
			return false;
		}
		CONFIG_DATA config;

		input_file.read((char*) &config, sizeof(CONFIG_DATA));
		config_to_ft(ftData, &config);
		input_file.read(ManufacturerBuf, sizeof(ManufacturerBuf));
		input_file.read(ManufacturerIdBuf, sizeof(ManufacturerIdBuf));
		input_file.read(DescriptionBuf, sizeof(DescriptionBuf));
		input_file.read(SerialNumberBuf, sizeof(SerialNumberBuf));
		input_file.close();
		ftData->Manufacturer = ManufacturerBuf;
		ftData->ManufacturerId = ManufacturerIdBuf;
		ftData->Description = DescriptionBuf;
		ftData->SerialNumber = SerialNumberBuf;
	} catch (...) {
		cerr << "Failed to read file " << file << endl;
		return false;
	}
	return true;
}

FT_STATUS read_from_device(FT_HANDLE ftHandle, PFT_PROGRAM_DATA ftData) {
	ftData->Signature1 = 0x00000000;
	ftData->Signature2 = 0xffffffff;
	ftData->Version = 0x00000005;
	ftData->Manufacturer = ManufacturerBuf;
	ftData->ManufacturerId = ManufacturerIdBuf;
	ftData->Description = DescriptionBuf;
	ftData->SerialNumber = SerialNumberBuf;

	return FT_EE_Read(ftHandle, ftData);
}

void print_info() {
	cout << "Manufacture: " << ManufacturerBuf << endl;
	cout << "ManufacturerId: " << ManufacturerIdBuf << endl;
	cout << "Description: " << DescriptionBuf << endl;
	cout << "SerialNumber: " << SerialNumberBuf << endl;
}

void erase(FT_HANDLE ftHandle) {
	cout << "Erasing... ";
	FT_STATUS ftStatus = FT_EraseEE(ftHandle);
	if (ftStatus != FT_OK) // Did the command execute OK?
			{
		cerr << "Error in erasing device!" << endl;
		FT_Close(ftHandle);
		return;
	}
	cout << "Done." << endl;
}

bool programDevice(unsigned int devNumber, string file) {
	FT_HANDLE ftHandle;

	cout << "Opening device... ";
	FT_STATUS ftStatus = FT_Open(devNumber, &ftHandle);
	if (ftStatus != FT_OK) // Did the command execute OK?
			{
		printf("Error in opening device!\n");
		return false; // Exit with error
	}
	cout << "Done." << endl;
	FT_PROGRAM_DATA ftData;

	cout << "Checking EEPROM... ";
	ftStatus = read_from_device(ftHandle, &ftData);

	if (ftStatus != FT_EEPROM_NOT_PROGRAMMED) { // device isn't blank
		if (ftStatus == FT_OK) {
			cout << "Not blank." << endl;
			erase(ftHandle);
		} else if (ftStatus == FT_EEPROM_NOT_PRESENT) {
			cout << "Not present!" << endl;
			FT_Close(ftHandle);
			return false;
		} else {
			cout << getErrorName(ftStatus) << endl;
			FT_Close(ftHandle);
			return false;
		}
	} else {
		cout << "Blank." << endl;
	}

	if (!read_from_file(file, &ftData)) {
		FT_Close(ftHandle);
		return false;
	}

	print_info();

	cout << "Programming... ";
	ftStatus = FT_EE_Program(ftHandle, &ftData);
	if (ftStatus != FT_OK) // Did the command execute OK?
			{
		cout << "ERROR: " << getErrorName(ftStatus) << endl;
		FT_Close(ftHandle);
		return false;
	}
	cout << "Done." << endl;

	FT_Close(ftHandle);
	return true;
}

string descriptionToName(string des) {
	if (des == "Alchitry Cu A") {
		return "Alchitry Cu";
	} else if (des == "Alchitry Au A") {
		return "Alchitry Au";
	} else {
		return "Unknown";
	}
}

int desciptionToType(string des) {
	if (des == "Alchitry Cu A") {
		return BOARD_CU;
	} else if (des == "Alchitry Au A") {
		return BOARD_AU;
	} else {
		return BOARD_UNKNOWN;
	}
}

void printDeviceList() {
	FT_STATUS ftStatus;
	FT_DEVICE_LIST_INFO_NODE *devInfo;
	DWORD numDevs = 0;
	// create the device information list
	ftStatus = FT_CreateDeviceInfoList(&numDevs);
	if (ftStatus != FT_OK) {
		cerr << "Could not read device list!" << endl;
		return;
	}

	if (numDevs > 0) {
		cout << "Devices: " << endl;
		// allocate storage for list based on numDevs
		devInfo = (FT_DEVICE_LIST_INFO_NODE*) malloc(
				sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
		// get the device information list
		ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
		if (ftStatus == FT_OK) {
			for (unsigned int i = 0; i < numDevs; i++) {
				cout << "  " << i << ": "
						<< descriptionToName(devInfo[i].Description) << endl;
			}
		} else {
			cerr << "Error getting device list!" << endl;
		}
		free(devInfo);
	} else {
		cout << "No devices found!" << endl;
	}
}

int getDeviceType(unsigned int devNumber) {
	FT_STATUS ftStatus;
	FT_DEVICE_LIST_INFO_NODE *devInfo;
	DWORD numDevs = 0;
	int board = BOARD_ERROR;
	// create the device information list
	ftStatus = FT_CreateDeviceInfoList(&numDevs);
	if (ftStatus != FT_OK) {
		cerr << "Could not read device list!" << endl;
		return BOARD_ERROR;
	}

	if (numDevs <= devNumber) {
		cerr << "Invalid device number!" << endl;
		return BOARD_ERROR;
	}

	// allocate storage for list based on numDevs
	devInfo = (FT_DEVICE_LIST_INFO_NODE*) malloc(
			sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
	// get the device information list
	ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
	if (ftStatus == FT_OK) {
		string boardDescription = devInfo[devNumber].Description;
		if (boardDescription == "Alchitry Cu A") {
			board = BOARD_CU;
		} else if (boardDescription == "Alchitry Au A") {
			board = BOARD_AU;
		} else {
			board = BOARD_UNKNOWN;
		}
	} else {
		cerr << "Error getting device list!" << endl;
	}
	free(devInfo);

	return board;
}

int getFirstDeviceOfType(int board) {
	FT_STATUS ftStatus;
	FT_DEVICE_LIST_INFO_NODE *devInfo;
	DWORD numDevs = 0;

	// create the device information list
	ftStatus = FT_CreateDeviceInfoList(&numDevs);
	if (ftStatus != FT_OK) {
		cerr << "Could not read device list!" << endl;
		return -1;
	}

	if (numDevs < 1) {
		cerr << "No devices found!" << endl;
		return -1;
	}

	// allocate storage for list based on numDevs
	devInfo = (FT_DEVICE_LIST_INFO_NODE*) malloc(
			sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
	// get the device information list
	ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);

	if (ftStatus == FT_OK) {
		for (int devNumber = 0; devNumber < numDevs; devNumber++) {
			string boardDescription = devInfo[devNumber].Description;
			int type = desciptionToType(boardDescription);
			if (type == board) {
				free(devInfo);
				return devNumber;
			}
		}
	} else {
		cerr << "Error getting device list!" << endl;
	}
	free(devInfo);
	return -1;
}

bool readAndSaveFTDI(string file) {
	FT_HANDLE ftHandle;

	cout << "Opening device... ";
	FT_STATUS ftStatus = FT_Open(0, &ftHandle);
	if (ftStatus != FT_OK) // Did the command execute OK?
			{
		printf("Error in opening device!\n");
		return false; // Exit with error
	}
	cout << "Done." << endl;
	FT_PROGRAM_DATA ftData;

	cout << "Checking EEPROM... ";
	ftStatus = read_from_device(ftHandle, &ftData);

	if (ftStatus != FT_OK) {
		if (ftStatus == FT_EEPROM_NOT_PROGRAMMED) {
			cout << "Blank." << endl;
			return false;
		} else if (ftStatus == FT_EEPROM_NOT_PRESENT) {
			cout << "Not present!" << endl;
			FT_Close(ftHandle);
			return false;
		} else {
			cout << "Unknown error " << ftStatus << endl;
			FT_Close(ftHandle);
			return false;
		}
	} else { // eeprom not blank
		cout << "Done." << endl;
	}

	cout << "Writing to file... ";

	write_to_file(file, &ftData);

	cout << "Done." << endl;

	FT_Close(ftHandle);
	return true;
}

void printUsage() {
	cout << "Usage: \"loader arguments\"" << endl;
	cout << endl;
	cout << "Arguments:" << endl;
	cout << "  -e : erase FPGA flash" << endl;
	cout << "  -l : list detected boards" << endl;
	cout << "  -h : print this help message" << endl;
	cout << "  -f config.bin : write FPGA flash" << endl;
	cout << "  -r config.bin : write FPGA RAM" << endl;
	cout << "  -u config.data : write FTDI eeprom" << endl;
	cout << "  -b n : select board \"n\" (defaults to 0)" << endl;
	cout << "  -p loader.bin : Au bridge bin" << endl;
	cout << "  -t TYPE : TYPE can be au or cu (defaults to au)" << endl;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printUsage();
		return 1;
	}

	bool fpgaFlash = false;
	bool fpgaRam = false;
	bool eeprom = false;
	string eepromConfig;
	string fpgaBinFlash;
	string fpgaBinRam;
	bool erase = false;
	bool list = false;
	bool print = false;
	int deviceNumber = -1;
	bool bridgeProvided = false;
	string auBridgeBin;
	bool isAu = true;

	for (int i = 1; i < argc;) {
		string arg = argv[i];
		if (arg == "-e") {
			i++;
			erase = true;
		} else if (arg == "-l") {
			i++;
			list = true;
		} else if (arg == "-h") {
			i++;
			print = true;
		} else if (arg == "-f") {
			if (argc <= i + 1) {
				cerr << "Missing bin file!" << endl;
				printUsage();
				return 1;
			}
			fpgaFlash = true;
			fpgaBinFlash = argv[i + 1];
			i += 2;
		} else if (arg == "-r") {
			if (argc <= i + 1) {
				cerr << "Missing bin file!" << endl;
				printUsage();
				return 1;
			}
			fpgaRam = true;
			fpgaBinRam = argv[i + 1];
			i += 2;
		} else if (arg == "-u") {
			if (argc <= i + 1) {
				cerr << "Missing data file!" << endl;
				printUsage();
				return 1;
			}
			eeprom = true;
			eepromConfig = argv[i + 1];
			i += 2;
		} else if (arg == "-b") {
			if (argc <= i + 1) {
				cerr << "Missing board number!" << endl;
				printUsage();
				return 1;
			}
			try {
				deviceNumber = stoi(argv[i + 1]);
			} catch (const std::invalid_argument& ia) {
				cerr << argv[i + 1] << " is not a number!" << endl;
				printUsage();
				return 1;
			}
			if (deviceNumber < 0) {
				cerr << "Device numbers can't be negative!" << endl;
				printUsage();
				return 1;
			}
			i += 2;
		} else if (arg == "-p") {
			if (argc <= i + 1) {
				cerr << "Missing bin file!" << endl;
				printUsage();
				return 1;
			}
			bridgeProvided = true;
			auBridgeBin = argv[i + 1];
			i += 2;
		} else if (arg == "-t") {
			if (argc <= i + 1) {
				cerr << "Missing board type!" << endl;
				printUsage();
				return 1;
			}
			if (strcmp(argv[i + 1], "au") == 0) {
				isAu = true;
			} else if (strcmp(argv[i + 1], "cu") == 0) {
				isAu = false;
			} else {
				cerr << "Invalid board type: " << argv[i + 1] << endl;
				printUsage();
				return 1;
			}
			i += 2;

		} else {
			cerr << "Unknown argument " << arg << endl;
			printUsage();
			return 1;
		}
	}

	if (print)
		printUsage();

	if (list)
		printDeviceList();

	if (deviceNumber < 0)
		deviceNumber = getFirstDeviceOfType(isAu ? BOARD_AU : BOARD_CU);

	if (deviceNumber < 0) {
		cerr << "Couldn't find device!" << endl;
		return 2;
	}

	if (eeprom)
		programDevice(deviceNumber, eepromConfig);

	if (erase || fpgaFlash || fpgaRam) {
		int boardType = getDeviceType(deviceNumber);
		if ((isAu && boardType != BOARD_AU)
				|| (!isAu && boardType != BOARD_CU)) {
			cerr << "Invalid board type detected!" << endl;
			return 2;
		}

		if (boardType == BOARD_AU) {
			if (bridgeProvided == false && (erase || fpgaFlash)) {
				cerr << "No Au bridge bin provided!" << endl;
				return 2;
			}
			Jtag jtag;
			if (jtag.connect(deviceNumber) != FT_OK) {
				cerr << "Failed to connect to JTAG!" << endl;
				return 2;
			}
			if (jtag.initialize() == false) {
				cerr << "Failed to initialize JTAG!" << endl;
				return 2;
			}
			Loader loader(&jtag);

			if (erase)
				if (!loader.eraseFlash(auBridgeBin)) {
					cerr << "Failed to erase flash!" << endl;
				}

			if (fpgaFlash) {
				if (!loader.writeBin(fpgaBinFlash, true, auBridgeBin)) {
					cerr << "Failed to write FPGA flash!" << endl;
				}
			}

			if (fpgaRam) {
				if (!loader.writeBin(fpgaBinRam, false, "")) {
					cerr << "Failed to write FPGA RAM!" << endl;
				}
			}

			jtag.disconnect();
		} else if (boardType == BOARD_CU) {
			Spi spi;
			if (spi.connect(deviceNumber) != FT_OK) {
				cerr << "Failed to connect to SPI!" << endl;
				return 2;
			}
			if (spi.initialize() == false) {
				cerr << "Failed to initialize SPI!" << endl;
				return 2;
			}

			if (erase) {
				if (!spi.eraseFlash()) {
					cerr << "Failed to erase flash!" << endl;
				}
			}

			if (fpgaFlash) {
				if (!spi.writeBin(fpgaBinFlash)) {
					cerr << "Failed to write FPGA flash!" << endl;
				}
			}

			if (fpgaRam) {
				cerr << "Alchitry Cu doesn't support RAM only programming!"
						<< endl;
				return 1;
			}
		} else {
			cerr << "Unknown board type!" << endl;
			return 2;
		}
	}

	return 0;
}

