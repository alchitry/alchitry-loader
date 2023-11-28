/*
 * jtag.cpp
 *
 *  Created on: May 24, 2017
 *      Author: justin
 */

#include "jtag.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#ifdef _WIN32
#include "mingw.thread.h"
#else
#include <thread>
#endif

#include <chrono>

using namespace std;

Jtag::Jtag() {
	ftHandle = 0;
	active = false;
}

FT_STATUS Jtag::connect(unsigned int devNumber) {
	return FT_Open(devNumber, &ftHandle);
}

FT_STATUS Jtag::disconnect() {
	active = false;
	return FT_Close(ftHandle);
}

bool Jtag::initialize() {
	BYTE byInputBuffer[1024]; // Buffer to hold data read from the FT2232H
	DWORD dwNumBytesToRead = 0; // Number of bytes available to read in the driver's input buffer
	DWORD dwNumBytesRead = 0; // Count of actual bytes read - used with FT_Read
	FT_STATUS ftStatus;
	ftStatus = FT_ResetDevice(ftHandle);
	//Reset USB device
	//Purge USB receive buffer first by reading out all old data from FT2232H receive buffer
	ftStatus |= FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);	// Get the number of bytes in the FT2232H receive buffer
	if ((ftStatus == FT_OK) && (dwNumBytesToRead > 0))
		FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);//Read out the data from FT2232H receive buffer
	ftStatus |= FT_SetUSBParameters(ftHandle, 65536, 65535);//Set USB request transfer sizes to 64K
	ftStatus |= FT_SetChars(ftHandle, false, 0, false, 0); //Disable event and error characters
	ftStatus |= FT_SetTimeouts(ftHandle, 0, 5000); //Sets the read and write timeouts in milliseconds
	ftStatus |= FT_SetLatencyTimer(ftHandle, 16); //Set the latency timer (default is 16mS)
	ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x00);	        //Reset controller
	ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x02);	        //Enable MPSSE mode

	if (ftStatus != FT_OK) {
		cerr << "Failed to set initial configuration!" << endl;
		return false;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for all the USB stuff to complete and work

	if (!sync_mpsse()) {
		cerr << "Failed to sync with MPSSE!" << endl;
		return false;
	}

	if (!config_jtag()) {
		cerr << "Failed to set JTAG configuration!" << endl;
		return false;
	}

	active = true;

	return true;
}

bool Jtag::sync_mpsse() {
	BYTE byOutputBuffer[8]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	BYTE byInputBuffer[8]; // Buffer to hold data read from the FT2232H
	DWORD dwCount = 0; // General loop index
	DWORD dwNumBytesToSend = 0; // Index to the output buffer
	DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	DWORD dwNumBytesToRead = 0; // Number of bytes available to read in the driver's input buffer
	DWORD dwNumBytesRead = 0; // Count of actual bytes read - used with FT_Read
	FT_STATUS ftStatus;

	byOutputBuffer[dwNumBytesToSend++] = 0xAA;	//'\xAA';
	//Add bogus command ‘xAA’ to the queue
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	if (ftStatus != FT_OK)
		cerr << "Failed to send bad command" << endl;
	// Send off the BAD commands
	dwNumBytesToSend = 0; // Reset output buffer pointer
	do {
		ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
		if (ftStatus != FT_OK)
			cerr << "Failed to get queue status " << ftStatus << endl;
		// Get the number of bytes in the device input buffer
	} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
	if (dwNumBytesRead > 8) {
		cerr << "Input buffer too small in sync_mpsse()!" << endl;
		return false;
	}
	//or Timeout
	bool bCommandEchod = false;
	ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead,
			&dwNumBytesRead);

	if (ftStatus != FT_OK)
		cerr << "Failed to read data" << endl;

	//Read out the data from input buffer
	for (dwCount = 0; dwCount < dwNumBytesRead - 1; dwCount++)
	//Check if Bad command and echo command received
			{
		if ((byInputBuffer[dwCount] == 0xFA)
				&& (byInputBuffer[dwCount + 1] == 0xAA)) {
			bCommandEchod = true;
			break;
		}
	}

	return bCommandEchod;
}

bool Jtag::config_jtag() {
	FT_STATUS ftStatus;
	BYTE byOutputBuffer[64]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	DWORD dwNumBytesToSend = 0; // Index to the output buffer
	DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	DWORD dwClockDivisor = 0x05DB; // Value of clock divisor, SCL Frequency = 60/((1+0x05DB)*2) (MHz) = 20khz
	// -----------------------------------------------------------
	// Configure the MPSSE settings for JTAG
	// Multiple commands can be sent to the MPSSE with one FT_Write
	// -----------------------------------------------------------
	dwNumBytesToSend = 0; // Start with a fresh index
	// Set up the Hi-Speed specific commands for the FTx232H
	byOutputBuffer[dwNumBytesToSend++] = 0x8A;
	// Use 60MHz master clock (disable divide by 5)
	byOutputBuffer[dwNumBytesToSend++] = 0x97;
	// Turn off adaptive clocking (may be needed for ARM)
	byOutputBuffer[dwNumBytesToSend++] = 0x8D;
	// Disable three-phase clocking
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	// Send off the HS-specific commands

	if (ftStatus != FT_OK)
		return false;

	dwNumBytesToSend = 0; // Reset output buffer pointer
	// Set initial states of the MPSSE interface - low byte, both pin directions and output values
	// Pin name Signal Direction Config Initial State Config
	// ADBUS0 TCK output 1 low 0
	// ADBUS1 TDI output 1 low 0
	// ADBUS2 TDO input 0 0
	// ADBUS3 TMS output 1 high 1
	// ADBUS4 GPIOL0 input 0 0
	// ADBUS5 GPIOL1 input 0 0
	// ADBUS6 GPIOL2 input 0 0
	// ADBUS7 GPIOL3 input 0 0
	byOutputBuffer[dwNumBytesToSend++] = 0x80;
	// Set data bits low-byte of MPSSE port
	byOutputBuffer[dwNumBytesToSend++] = 0x08;
	// Initial state config above
	byOutputBuffer[dwNumBytesToSend++] = 0x0B;
	// Direction config above
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	// Send off the low GPIO config commands

	if (ftStatus != FT_OK)
		return false;

	dwNumBytesToSend = 0; // Reset output buffer pointer
	// Set initial states of the MPSSE interface - high byte, both pin directions and output values
	// Pin name Signal Direction Config Initial State Config
	// ACBUS0 GPIOH0 input 0 0
	// ACBUS1 GPIOH1 input 0 0
	// ACBUS2 GPIOH2 input 0 0
	// ACBUS3 GPIOH3 input 0 0
	// ACBUS4 GPIOH4 input 0 0
	// ACBUS5 GPIOH5 input 0 0
	// ACBUS6 GPIOH6 input 0 0
	// ACBUS7 GPIOH7 input 0 0
	byOutputBuffer[dwNumBytesToSend++] = 0x82;
	// Set data bits low-byte of MPSSE port
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	// Initial state config above
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	// Direction config above
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	// Send off the high GPIO config commands

	if (ftStatus != FT_OK)
		return false;

	dwNumBytesToSend = 0; // Reset output buffer pointer
	// Set TCK frequency
	// TCK = 60MHz /((1 + [(1 +0xValueH*256) OR 0xValueL])*2)
	byOutputBuffer[dwNumBytesToSend++] = 0x86;
	//Command to set clock divisor
	byOutputBuffer[dwNumBytesToSend++] = dwClockDivisor & 0xFF;
	//Set 0xValueL of clock divisor
	byOutputBuffer[dwNumBytesToSend++] = (dwClockDivisor >> 8) & 0xFF;
	//Set 0xValueH of clock divisor
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	// Send off the clock divisor commands

	if (ftStatus != FT_OK)
		return false;

	dwNumBytesToSend = 0; // Reset output buffer pointer
	// Disable internal loop-back
	byOutputBuffer[dwNumBytesToSend++] = 0x85;
	// Disable loopback
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	// Send off the loopback command

	if (ftStatus != FT_OK)
		return false;

	return true;
}

bool Jtag::setFreq(double freq) {
	if (!active) {
		cerr
				<< "Jtag must be connected and initialized before calling setFreq()!"
				<< endl;
		return false;
	}
	FT_STATUS ftStatus;
	BYTE byOutputBuffer[8]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	DWORD dwNumBytesToSend = 0; // Index to the output buffer
	DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	DWORD dwClockDivisor; // Value of clock divisor, SCL Frequency = 60/((1+clkDiv)*2) (MHz)

	dwClockDivisor = 30.0 / (freq / 1000000.0) - 1.0;

	// Set TCK frequency
	// TCK = 60MHz /((1 + [(1 +0xValueH*256) OR 0xValueL])*2)
	byOutputBuffer[dwNumBytesToSend++] = 0x86;
	//Command to set clock divisor
	byOutputBuffer[dwNumBytesToSend++] = dwClockDivisor & 0xFF;
	//Set 0xValueL of clock divisor
	byOutputBuffer[dwNumBytesToSend++] = (dwClockDivisor >> 8) & 0xFF;
	//Set 0xValueH of clock divisor
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend,
			&dwNumBytesSent);
	// Send off the clock divisor commands

	if (ftStatus != FT_OK)
		return false;
	return true;
}

bool Jtag::navigateToState(Jtag_fsm::State init, Jtag_fsm::State dest) {
	FT_STATUS ftStatus;
	BYTE byOutputBuffer[3]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write

	Jtag_fsm::Transistions transistions = Jtag_fsm::getTransitions(init, dest);

	if (transistions.moves > 0) {
		if (transistions.moves < 8) {
			byOutputBuffer[0] = 0x4B;
			byOutputBuffer[1] = transistions.moves - 1;
			byOutputBuffer[2] = 0x7f & transistions.tms;
			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
			if (ftStatus != FT_OK)
				return false;
		} else {
			cout << "Transition of 8 moves!" << endl;
			byOutputBuffer[0] = 0x4B;
			byOutputBuffer[1] = 6;
			byOutputBuffer[2] = 0x7f & transistions.tms;
			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
			if (ftStatus != FT_OK)
				return false;
			byOutputBuffer[0] = 0x4B;
			byOutputBuffer[1] = transistions.moves - 8;
			byOutputBuffer[2] = 0x7f & (transistions.tms >> 7);
			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
			if (ftStatus != FT_OK)
				return false;
		}
	}
	return true;
}

bool Jtag::shiftData(unsigned int bitCount, string tdi, string tdo,
		string mask) {
	FT_STATUS ftStatus;
	unsigned int reqBytes = bitCount / 8 + (bitCount % 8 > 0);
	BYTE *byOutputBuffer = new BYTE[reqBytes + 3];
	BYTE *tdoBuffer = new BYTE[reqBytes + 3];
	BYTE *byInputBuffer = new BYTE[reqBytes + 6];
	DWORD dwNumBytesSent = 0;
	DWORD dwNumBytesToRead = 0;
	DWORD dwNumBytesRead = 0;
	DWORD tdoBytes = 0;

	unsigned int reqHex = bitCount / 4 + (bitCount % 4 > 0);

	if (tdi.length() < reqHex)
		return false;
	bool read = tdo != "";
	if (read) {
		if (tdo.length() < reqHex) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return false;
		}
		if (mask != "" && mask.length() < reqHex) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return false;
		}
	}

	if (!flush())
		return false;

	if (bitCount < 9) {
		int data = stoi(tdi, 0, 16);
		byOutputBuffer[0] = read ? 0x3B : 0x1B;
		byOutputBuffer[1] = bitCount - 2;
		byOutputBuffer[2] = data & 0xff;
		ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return false;
		}

		BYTE lastBit = (data >> ((bitCount - 1) % 8)) & 0x01;

		byOutputBuffer[0] = read ? 0x6E : 0x4E;
		byOutputBuffer[1] = 0x00;
		byOutputBuffer[2] = 0x03 | (lastBit << 7);
		ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return false;
		}

		if (read) {
			do {
				ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
				// Get the number of bytes in the device input buffer
			} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
			//or Timeout
			ftStatus = FT_Read(ftHandle, byInputBuffer, dwNumBytesToRead,
					&dwNumBytesRead);
			if (ftStatus != FT_OK) {
				delete[] byOutputBuffer;
				delete[] tdoBuffer;
				delete[] byInputBuffer;
				return false;
			}

			tdoBuffer[0] = byInputBuffer[0] >> (8 - (bitCount - 1));
			tdoBuffer[0] |= byInputBuffer[1] >> (7 - (bitCount - 1));
			tdoBytes = 1;
		}
	} else {
		BYTE *tdiBytes = new BYTE[reqBytes + 1];
		hexToByte(tdi, tdiBytes);

		unsigned int fullBytes = (bitCount - 1) / 8;
		unsigned int remBytes = fullBytes;
		unsigned int offset = 0;

		if (fullBytes > 65536 && read) {
			cout << "Large transfers with reads may not work!" << endl;
		}

		while (remBytes > 0) {
			unsigned int bct = remBytes > 65536 ? 65536 : remBytes;
			byOutputBuffer[0] = read ? 0x39 : 0x19;
			byOutputBuffer[1] = (bct - 1) & 0xff;
			byOutputBuffer[2] = ((bct - 1) >> 8) & 0xff;

			for (unsigned int i = 0; i < bct; i++) {
				byOutputBuffer[3 + i] = tdiBytes[i + offset];
			}

			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3 + bct,
					&dwNumBytesSent);
			if (ftStatus != FT_OK) {
				delete[] byOutputBuffer;
				delete[] tdoBuffer;
				delete[] byInputBuffer;
				delete[] tdiBytes;
				return false;
			}

			remBytes -= bct;
			offset += bct;
		}

		unsigned int partialBits = bitCount - 1 - (fullBytes * 8);

		if (fullBytes * 8 + 1 != bitCount) {
			byOutputBuffer[0] = read ? 0x3B : 0x1B;
			byOutputBuffer[1] = partialBits - 1;
			byOutputBuffer[2] = tdiBytes[reqBytes - 1] & 0xff;
			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
			if (ftStatus != FT_OK) {
				delete[] byOutputBuffer;
				delete[] tdoBuffer;
				delete[] byInputBuffer;
				delete[] tdiBytes;
				return false;
			}
		}

		BYTE lastBit = (tdiBytes[reqBytes - 1] >> ((bitCount - 1) % 8)) & 0x01;

		byOutputBuffer[0] = read ? 0x6E : 0x4E;
		byOutputBuffer[1] = 0x00;
		byOutputBuffer[2] = 0x03 | (lastBit << 7);
		ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			delete[] tdiBytes;
			return false;
		}

		if (read) {
			DWORD bytesToRead = fullBytes
					+ ((fullBytes * 8 + 1 != bitCount) ? 2 : 1);
			do {
				ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
				// Get the number of bytes in the device input buffer
			} while ((dwNumBytesToRead != bytesToRead) && (ftStatus == FT_OK));
			//or Timeout
			ftStatus = FT_Read(ftHandle, byInputBuffer, dwNumBytesToRead,
					&dwNumBytesRead);
			if (ftStatus != FT_OK) {
				delete[] byOutputBuffer;
				delete[] tdoBuffer;
				delete[] byInputBuffer;
				delete[] tdiBytes;
				return false;
			}

			for (unsigned int i = 0; i < fullBytes; i++)
				tdoBuffer[tdoBytes++] = byInputBuffer[i];

			if (fullBytes * 8 + 1 != bitCount) {
				tdoBuffer[tdoBytes] = byInputBuffer[tdoBytes]
						>> (8 - partialBits);
				tdoBuffer[tdoBytes++] |= byInputBuffer[dwNumBytesRead - 1]
						>> (7 - partialBits);
			} else {
				tdoBuffer[tdoBytes++] = byInputBuffer[dwNumBytesRead - 1] >> 7;
			}
		}

		delete[] tdiBytes;
	}

	if (read) {
		//Read out the data from input buffer
		std::stringstream ss;
		for (int i = tdoBytes - 1; i >= 0; i--)
			ss << setfill('0') << setw(2) << hex << (int) tdoBuffer[i];
		string hexTdo = ss.str();
		if (hexTdo.length() - 1 == mask.length())
			hexTdo = hexTdo.substr(1, hexTdo.length() - 1);
		if (!compareHexString(hexTdo, tdo, mask)) {
			cerr << "TDO didn't match expected string. Got " << hexTdo
					<< " expected " << tdo << " with mask " << mask << endl;

			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return false;

		}
	}

	delete[] byOutputBuffer;
	delete[] tdoBuffer;
	delete[] byInputBuffer;
	return true;
}

string Jtag::shiftData(unsigned int bitCount, string tdi) {
	FT_STATUS ftStatus;
	unsigned int reqBytes = bitCount / 8 + (bitCount % 8 > 0);
	BYTE *byOutputBuffer = new BYTE[reqBytes + 3];
	BYTE *tdoBuffer = new BYTE[reqBytes + 3];
	BYTE *byInputBuffer = new BYTE[reqBytes + 6];
	DWORD dwNumBytesSent = 0;
	DWORD dwNumBytesToRead = 0;
	DWORD dwNumBytesRead = 0;
	DWORD tdoBytes = 0;

	unsigned int reqHex = bitCount / 4 + (bitCount % 4 > 0);

	if (tdi.length() < reqHex)
		return "";

	if (!flush())
		return "";

	if (bitCount < 9) {
		int data = stoi(tdi, 0, 16);
		byOutputBuffer[0] = 0x3B;
		byOutputBuffer[1] = bitCount - 2;
		byOutputBuffer[2] = data & 0xff;
		ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return "";
		}

		BYTE lastBit = (data >> ((bitCount - 1) % 8)) & 0x01;

		byOutputBuffer[0] = 0x6E;
		byOutputBuffer[1] = 0x00;
		byOutputBuffer[2] = 0x03 | (lastBit << 7);
		ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return "";
		}

		do {
			ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
			// Get the number of bytes in the device input buffer
		} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
		//or Timeout
		ftStatus = FT_Read(ftHandle, byInputBuffer, dwNumBytesToRead,
				&dwNumBytesRead);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			return "";
		}

		tdoBuffer[0] = byInputBuffer[0] >> (8 - (bitCount - 1));
		tdoBuffer[0] |= byInputBuffer[1] >> (7 - (bitCount - 1));
		tdoBytes = 1;

	} else {
		BYTE *tdiBytes = new BYTE[reqBytes + 1];
		hexToByte(tdi, tdiBytes);

		unsigned int fullBytes = (bitCount - 1) / 8;
		unsigned int remBytes = fullBytes;
		unsigned int offset = 0;

		if (fullBytes > 65536) {
			cout << "Large transfers with reads may not work!" << endl;
		}

		while (remBytes > 0) {
			unsigned int bct = remBytes > 65536 ? 65536 : remBytes;
			byOutputBuffer[0] = 0x39;
			byOutputBuffer[1] = (bct - 1) & 0xff;
			byOutputBuffer[2] = ((bct - 1) >> 8) & 0xff;

			for (unsigned int i = 0; i < bct; i++) {
				byOutputBuffer[3 + i] = tdiBytes[i + offset];
			}

			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3 + bct,
					&dwNumBytesSent);
			if (ftStatus != FT_OK) {
				delete[] byOutputBuffer;
				delete[] tdoBuffer;
				delete[] byInputBuffer;
				delete[] tdiBytes;
				return "";
			}

			remBytes -= bct;
			offset += bct;
		}

		unsigned int partialBits = bitCount - 1 - (fullBytes * 8);

		if (fullBytes * 8 + 1 != bitCount) {
			byOutputBuffer[0] = 0x3B;
			byOutputBuffer[1] = partialBits - 1;
			byOutputBuffer[2] = tdiBytes[reqBytes - 1] & 0xff;
			ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
			if (ftStatus != FT_OK) {
				delete[] byOutputBuffer;
				delete[] tdoBuffer;
				delete[] byInputBuffer;
				delete[] tdiBytes;
				return "";
			}
		}

		BYTE lastBit = (tdiBytes[reqBytes - 1] >> ((bitCount - 1) % 8)) & 0x01;

		byOutputBuffer[0] = 0x6E;
		byOutputBuffer[1] = 0x00;
		byOutputBuffer[2] = 0x03 | (lastBit << 7);
		ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			delete[] tdiBytes;
			return "";
		}

		DWORD bytesToRead = fullBytes
				+ ((fullBytes * 8 + 1 != bitCount) ? 2 : 1);
		do {
			ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
			// Get the number of bytes in the device input buffer
		} while ((dwNumBytesToRead != bytesToRead) && (ftStatus == FT_OK));
		//or Timeout
		ftStatus = FT_Read(ftHandle, byInputBuffer, dwNumBytesToRead,
				&dwNumBytesRead);
		if (ftStatus != FT_OK) {
			delete[] byOutputBuffer;
			delete[] tdoBuffer;
			delete[] byInputBuffer;
			delete[] tdiBytes;
			return "";
		}

		for (unsigned int i = 0; i < fullBytes; i++)
			tdoBuffer[tdoBytes++] = byInputBuffer[i];

		if (fullBytes * 8 + 1 != bitCount) {
			tdoBuffer[tdoBytes] = byInputBuffer[tdoBytes] >> (8 - partialBits);
			tdoBuffer[tdoBytes++] |= byInputBuffer[dwNumBytesRead - 1]
					>> (7 - partialBits);
		} else {
			tdoBuffer[tdoBytes++] = byInputBuffer[dwNumBytesRead - 1] >> 7;
		}

		delete[] tdiBytes;
	}

	//Read out the data from input buffer
	std::stringstream ss;
	for (int i = tdoBytes - 1; i >= 0; i--)
		ss << setfill('0') << setw(2) << hex << (int) tdoBuffer[i];
	string hexTdo = ss.str();
	if (hexTdo.length() - 1 == tdi.length())
		hexTdo = hexTdo.substr(1, hexTdo.length() - 1);

	delete[] byOutputBuffer;
	delete[] tdoBuffer;
	delete[] byInputBuffer;
	return hexTdo;
}

bool Jtag::sendClocks(unsigned long cycles) {
	BYTE byOutputBuffer[3];
	DWORD dwNumBytesSent;
	FT_STATUS ftStatus;

	if (cycles / 8 > 65536) {
		if (!sendClocks(cycles - 65536 * 8))
			return false;
		cycles = 65536 * 8;
	}

	cycles /= 8;

	byOutputBuffer[0] = 0x8F;
	byOutputBuffer[1] = (cycles - 1) & 0xff;
	byOutputBuffer[2] = ((cycles - 1) >> 8) & 0xff;
	ftStatus = FT_Write(ftHandle, byOutputBuffer, 3, &dwNumBytesSent);
	if (ftStatus != FT_OK)
		return false;

	return true;
}

bool Jtag::compareHexString(string a, string b, string mask) {
	unsigned int length = a.length();

	if (length != b.length()) {
		cout << "length mismatch!" << endl;
		return false;
	}

	if (mask == "")
		return a == b;

	if (length != mask.length()) {
		cout << "length and mask mismatch! " << length << " and "
				<< mask.length() << " of strings " << a << " and " << mask
				<< endl;
		return false;
	}

	for (unsigned int i = 0; i < length / 2; i++) {
		BYTE m = stoi(mask.substr(length - 2 - i * 2, 2), 0, 16);
		BYTE pa = stoi(a.substr(length - 2 - i * 2, 2), 0, 16);
		BYTE pb = stoi(b.substr(length - 2 - i * 2, 2), 0, 16);
		if ((pa & m) != (pb & m)) {
			cout << "Mismatch at " << i << endl;
			return false;
		}

	}
	if ((length & 1) != 0) {
		BYTE m = stoi(mask.substr(0, 1), 0, 16);
		BYTE pa = stoi(a.substr(0, 1), 0, 16);
		BYTE pb = stoi(b.substr(0, 1), 0, 16);
		if ((pa & m) != (pb & m)) {
			cout << "Mismatch at last bit" << endl;
			return false;
		}
	}

	return true;
}

void Jtag::hexToByte(string hex, BYTE* out) {
	int length = hex.length();
	for (int i = 0; i < length / 2; i++) {
		try {
			out[i] = stoi(hex.substr(length - 2 - i * 2, 2), 0, 16);
		} catch (exception& e) {
			cerr << "Failed at " << i << " with length " << length << endl;
			cerr.flush();
			exit(2);
		}
	}
	if ((length & 1) != 0)
		try {
			out[length / 2] = stoi(hex.substr(0, 1), 0, 16);
		} catch (exception& e) {
			cerr << "Failed to convert string " << hex.substr(0, 1)
					<< " to an int!" << endl;
			cerr.flush();
			exit(2);
		}
}

bool Jtag::flush() {
	FT_STATUS ftStatus;
	BYTE byInputBuffer[1024];
	DWORD dwNumBytesToRead = 0;
	DWORD dwNumBytesRead = 0;

	ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
	if (ftStatus != FT_OK)
		return false;
	if (dwNumBytesToRead == 0)
		return true;

//or Timeout
	ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead,
			&dwNumBytesRead);
	if (ftStatus != FT_OK)
		return false;
	return true;
}
