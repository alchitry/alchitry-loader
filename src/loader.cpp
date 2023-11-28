/*
 * Loader.cpp
 *
 *  Created on: Nov 6, 2017
 *      Author: justin
 */

#include "loader.h"
#include "jtag.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include "config_type.h"
#ifdef _WIN32
#include "mingw.thread.h"
#else
#include <thread>
#endif


using namespace std;

Loader::Loader(Jtag *dev) {
	device = dev;
	currentState = Jtag_fsm::TEST_LOGIC_RESET;
}
bool Loader::setState(Jtag_fsm::State state) {
	if (!device->navigateToState(currentState, state))
		return false;
	currentState = state;
	return true;

}

bool Loader::resetState() {
	currentState = Jtag_fsm::TEST_LOGIC_RESET;
	return device->navigateToState(Jtag_fsm::CAPTURE_DR,
			Jtag_fsm::TEST_LOGIC_RESET);
}

bool Loader::setIR(Instruction inst) {
	stringstream inst_str;
	inst_str << std::setfill('0') << std::setw(2) << hex
			<< static_cast<int>(inst);

	if (!device->navigateToState(currentState, Jtag_fsm::SHIFT_IR)) {
		cerr << "Failed to change to SHIFT_IR state!" << endl;
		return false;
	}
	if (!device->shiftData(6, inst_str.str(), "", "")) {
		cerr << "Failed to shift instruction data!" << endl;
		return false;
	}
	if (!device->navigateToState(Jtag_fsm::EXIT1_IR, Jtag_fsm::RUN_TEST_IDLE)) {
		cerr << "Failed to change to RUN_TEST_IDLE state!" << endl;
		return false;
	}
	currentState = Jtag_fsm::RUN_TEST_IDLE;
	return true;
}

// basically the same as shiftDR but ignores the first four bits
bool Loader::shiftUDR(int bits, string write, string read, string mask) {
	string uread = read;
	string umask = mask;
	string uwrite = write;

	if (!read.empty()) {
		uread = read + "0";
		umask = mask + "0";
		uwrite = "0" + write;
		bits += 4;
	}

	return shiftDR(bits, uwrite, uread, umask);
}

bool Loader::shiftDR(int bits, string write, string read, string mask) {
	if (!device->navigateToState(currentState, Jtag_fsm::SHIFT_DR)) {
		cerr << "Failed to change to SHIFT_DR state!" << endl;
		return false;
	}
	if (!device->shiftData(bits, write, read, mask)) {
		cerr << "Failed to shift data!" << endl;
		return false;
	}
	if (!device->navigateToState(Jtag_fsm::EXIT1_DR, Jtag_fsm::RUN_TEST_IDLE)) {
		cerr << "Failed to change to RUN_TEST_IDLE state!" << endl;
		return false;
	}
	currentState = Jtag_fsm::RUN_TEST_IDLE;
	return true;
}

bool Loader::shiftIR(int bits, string write, string read, string mask) {
	if (!device->navigateToState(currentState, Jtag_fsm::SHIFT_IR)) {
		cerr << "Failed to change to SHIFT_IR state!" << endl;
		return false;
	}
	if (!device->shiftData(bits, write, read, mask)) {
		cerr << "Failed to shift data!" << endl;
		return false;
	}
	if (!device->navigateToState(Jtag_fsm::EXIT1_IR, Jtag_fsm::RUN_TEST_IDLE)) {
		cerr << "Failed to change to RUN_TEST_IDLE state!" << endl;
		return false;
	}
	currentState = Jtag_fsm::RUN_TEST_IDLE;
	return true;
}

string Loader::shiftDR(int bits, string write) {
	if (!device->navigateToState(currentState, Jtag_fsm::SHIFT_DR)) {
		cerr << "Failed to change to SHIFT_DR state!" << endl;
		return NULL;
	}

	string data = device->shiftData(bits, write);
	if (data.empty()) {
		cerr << "Failed to shift data!" << endl;
		return NULL;
	}
	if (!device->navigateToState(Jtag_fsm::EXIT1_DR, Jtag_fsm::RUN_TEST_IDLE)) {
		cerr << "Failed to change to RUN_TEST_IDLE state!" << endl;
		return NULL;
	}
	currentState = Jtag_fsm::RUN_TEST_IDLE;
	return data;
}

bool Loader::loadBin(string file) {
	string binStr = fileToBinStr(file);

    if (binStr.empty()) {
        cerr << "Failed to read bin file: "+ file << endl;
        return false;
    }

	string reversedBinStr = reverseBytes(binStr);

	if (!device->setFreq(10000000)) {
		cerr << "Failed to set JTAG frequency!" << endl;
		return false;
	}
	if (!resetState())
		return false;
	if (!setState(Jtag_fsm::RUN_TEST_IDLE))
		return false;

	if (!setIR(JPROGRAM))
		return false;
	if (!setIR(ISC_NOOP))
		return false;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// config/jprog/poll
	if (!device->sendClocks(10000))
		return false;
	if (!shiftIR(6, "14", "11", "31"))
		return false;

	// config/slr
	if (!setIR(CFG_IN))
		return false;
	if (!shiftDR(binStr.length() * 4, reversedBinStr, "", ""))
		return false;

	// config/start
	if (!setState(Jtag_fsm::RUN_TEST_IDLE))
		return false;
	if (!device->sendClocks(100000))
		return false;
	if (!setIR(JSTART))
		return false;
	if (!setState(Jtag_fsm::RUN_TEST_IDLE))
		return false;
	if (!device->sendClocks(100))
		return false;
	if (!shiftIR(6, "09", "31", "11"))
		return false;

	// config/status
	if (!setState(Jtag_fsm::TEST_LOGIC_RESET))
		return false;
	if (!device->sendClocks(5))
		return false;
	if (!setIR(CFG_IN))
		return false;
	if (!shiftDR(160, "0000000400000004800700140000000466aa9955", "", ""))
		return false;
	if (!setIR(CFG_OUT))
		return false;
	if (!shiftDR(32, "00000000", "3f5e0d40", "08000000"))
		return false;
	if (!setState(Jtag_fsm::TEST_LOGIC_RESET))
		return false;
	if (!device->sendClocks(5))
		return false;

	return true;
}

string Loader::fileToBinStr(string file) {
	ifstream binFile(file);
	stringstream hexString;

	char *buffer;
	binFile.seekg(0, ios::end);
	unsigned int byteCount = binFile.tellg();
	binFile.seekg(0, ios::beg);
	buffer = new char[byteCount];
	binFile.read(buffer, byteCount);
	binFile.close();

	for (int i = byteCount - 1; i >= 0; i--) {
		hexString << setfill('0') << setw(2) << std::hex
				<< (int) ((unsigned char) buffer[i]);
	}

	delete buffer;

	return hexString.str();
}

bool Loader::eraseFlash(string loaderFile) {
	cout << "Initializing FPGA..." << endl;
	if (!loadBin(loaderFile)) {
		cerr << "Failed to initialize FPGA!" << endl;
		return false;
	}

	if (!device->setFreq(1500000)) {
		cerr << "Failed to set JTAG frequency!" << endl;
		return false;
	}

	cout << "Erasing..." << endl;

	// Erase the flash
	if (!setIR(USER1))
		return false;

	if (!shiftDR(1, "0", "", ""))
		return false;

	std::this_thread::sleep_for(std::chrono::seconds(1)); // wait for erase

	if (!setIR(JPROGRAM))
		return false;

	// reset just for good measure
	if (!resetState())
		return false;

	return true;
}

bool Loader::writeBin(string binFile, bool flash, string loaderFile) {
	if (flash) {
		string binStr = fileToBinStr(binFile);

		cout << "Initializing FPGA..." << endl;
		if (!loadBin(loaderFile)) {
			cerr << "Failed to initialize FPGA!" << endl;
			return false;
		}

		if (!device->setFreq(1500000)) {
			cerr << "Failed to set JTAG frequency!" << endl;
			return false;
		}

		cout << "Erasing..." << endl;

		// Erase the flash
		if (!setIR(USER1))
			return false;

		if (!shiftDR(1, "0", "", ""))
			return false;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		cout << "Writing..." << endl;

		// Write the flash
		if (!setIR(USER2))
			return false;

		if (!shiftDR(binStr.length() * 4, binStr, "", ""))
			return false;

		// If you enter the reset state after a write
		// the loader firmware resets the flash into
		// regular SPI mode and gets stuck in a dead FSM
		// state. You need to do this before issuing a
		// JPROGRAM command or the FPGA can't read the
		// flash.
		if (!resetState())
			return false;

		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms delay is required before issuing JPROGRAM

		cout << "Resetting FPGA..." << endl;
		// JPROGRAM resets the FPGA configuration and will
		// cause it to read the flash memory
		if (!setIR(JPROGRAM))
			return false;
	} else {
		cout << "Programming FPGA..." << endl;
		if (!loadBin(binFile)) {
			cerr << "Failed to initialize FPGA!" << endl;
			return false;
		}
	}

	// reset just for good measure
	if (!resetState())
		return false;

	cout << "Done." << endl;
	return true;
}

bool Loader::checkIDCODE() {
	if (!setIR(IDCODE))
		return false;

	if (!shiftDR(32, "00000000", "0362D093", "0FFFFFFF")) // FPGA IDCODE
		return false;

	return true;
}

BYTE reverse(BYTE b) {
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

bool Loader::setWREN() {
	if (!setIR(USER1))
		return false;
	if (!shiftDR(8, reverseBytes("06"), "", ""))
		return false;
	return true;
}

int Loader::getStatus() {
	if (!setIR(USER1))
		return -1;
	string data = shiftDR(17, reverseBytes("00005"));
	cout << data << endl;
	if (data.empty())
		return -1;
	int status = stoi(data, 0, 16);
	status >>= 9;
	return reverse(status);
}

void hexToByte(string hex, BYTE* out) {
	int length = hex.length();
	for (int i = 0; i < length / 2; i++) {
		out[i] = stoi(hex.substr(length - 2 - i * 2, 2), 0, 16);
	}
	if ((length & 1) != 0)
		out[length / 2] = stoi(hex.substr(0, 1), 0, 16);
}

string Loader::reverseBytes(string start) {
	unsigned long l = start.length();
	if (l & 1)
		l++;
	l /= 2;
	BYTE* bytes = new BYTE[l];
	hexToByte(start, bytes);
	for (unsigned long i = 0; i < l; i++) {
		bytes[i] = reverse(bytes[i]);
	}
	std::stringstream ss;
	for (long i = l - 1; i >= 0; i--)
		ss << setfill('0') << setw(2) << hex << (unsigned int) bytes[i];
	string out = ss.str();
	if (out.length() - 1 == start.length())
		out = out.substr(1, out.length() - 1);
	delete[] bytes;
	return out;
}

