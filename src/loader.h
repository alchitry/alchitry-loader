/*
 * Loader.h
 *
 *  Created on: Nov 6, 2017
 *      Author: justin
 */

#ifndef LOADER_H_
#define LOADER_H_

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iostream>
#include <iomanip>
#include<algorithm>
#include "jtag.h"
#include "jtag_fsm.h"

class Loader {
	Jtag* device;
	Jtag_fsm::State currentState;

	public:
	enum Instruction {
		EXTEST = 0x26,
		EXTEST_PULSE = 0x3C,
		EXTEST_TRAIN = 0x3D,
		SAMPLE = 0x01,
		USER1 = 0x02,
		USER2 = 0x03,
		USER3 = 0x22,
		USER4 = 0x23,
		CFG_OUT = 0x04,
		CFG_IN = 0x05,
		USERCODE = 0x08,
		IDCODE = 0x09,
		HIGHZ_IO = 0x0A,
		JPROGRAM = 0x0B,
		JSTART = 0x0C,
		JSHUTDOWN = 0x0D,
		XADC_DRP = 0x37,
		ISC_ENABLE = 0x10,
		ISC_PROGRAM = 0x11,
		XSC_PROGRAM_KEY = 0x12,
		XSC_DNA = 0x17,
		FUSE_DNA = 0x32,
		ISC_NOOP = 0x14,
		ISC_DISABLE = 0x16,
		BYPASS = 0x2F,
	};

public:
	Loader(Jtag*);
	bool resetState();
	bool checkIDCODE();
	bool eraseFlash(string);
	bool writeBin(string, bool, string);

private:
	bool setWREN();
	bool setIR(Instruction);
	bool shiftUDR(int, string, string, string);
	bool shiftDR(int, string, string, string);
	string shiftDR(int, string);
	bool shiftIR(int, string, string, string);
	int getStatus();
	string reverseBytes(string);
	string fileToBinStr(string);
	bool loadBin(string);
	bool setState(Jtag_fsm::State);
};



#endif /* LOADER_H_ */
