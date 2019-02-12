/*
 * jtag.h
 *
 *  Created on: May 24, 2017
 *      Author: justin
 */

#ifndef JTAG_H_
#define JTAG_H_

#include "ftd2xx.h"
#include "jtag_fsm.h"
#include <unistd.h>

class Jtag {
	FT_HANDLE ftHandle;
	unsigned int uiDevIndex = 0xF; // The device in the list that is used
	bool active;

public:
	Jtag();
	FT_STATUS connect(unsigned int);
	FT_STATUS disconnect();
	bool initialize();
	bool setFreq(double);
	bool navigateToState(Jtag_fsm::State, Jtag_fsm::State);
	bool shiftData(unsigned int, string, string, string);
	string shiftData(unsigned int, string);
	bool sendClocks(unsigned long);

private:
	bool sync_mpsse();
	bool config_jtag();
	static void hexToByte(string, BYTE*);
	bool flush();
	bool compareHexString(string, string, string);

};

#endif /* JTAG_H_ */
