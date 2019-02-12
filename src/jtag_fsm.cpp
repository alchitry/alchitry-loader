/*
 * jtag_fsm.cpp
 *
 *  Created on: May 24, 2017
 *      Author: justin
 */

#include "jtag_fsm.h"
#include <queue>
#include <iostream>

using namespace std;

Jtag_fsm::State Jtag_fsm::getTransition(State state, bool tms) {
	switch (state) {
	case TEST_LOGIC_RESET:
		return tms ? TEST_LOGIC_RESET : RUN_TEST_IDLE;
	case RUN_TEST_IDLE:
		return tms ? SELECT_DR_SCAN : RUN_TEST_IDLE;
	case SELECT_DR_SCAN:
		return tms ? SELECT_IR_SCAN : CAPTURE_DR;
	case CAPTURE_DR:
		return tms ? EXIT1_DR : SHIFT_DR;
	case SHIFT_DR:
		return tms ? EXIT1_DR : SHIFT_DR;
	case EXIT1_DR:
		return tms ? UPDATE_DR : PAUSE_DR;
	case PAUSE_DR:
		return tms ? EXIT2_DR : PAUSE_DR;
	case EXIT2_DR:
		return tms ? UPDATE_DR : SHIFT_DR;
	case UPDATE_DR:
		return tms ? SELECT_DR_SCAN : RUN_TEST_IDLE;
	case SELECT_IR_SCAN:
		return tms ? TEST_LOGIC_RESET : CAPTURE_IR;
	case CAPTURE_IR:
		return tms ? EXIT1_IR : SHIFT_IR;
	case SHIFT_IR:
		return tms ? EXIT1_IR : SHIFT_IR;
	case EXIT1_IR:
		return tms ? UPDATE_IR : PAUSE_IR;
	case PAUSE_IR:
		return tms ? EXIT2_IR : PAUSE_IR;
	case EXIT2_IR:
		return tms ? UPDATE_IR : SHIFT_IR;
	case UPDATE_IR:
		return tms ? SELECT_DR_SCAN : RUN_TEST_IDLE;
	}

	return TEST_LOGIC_RESET;
}

Jtag_fsm::Transistions Jtag_fsm::getTransitions(State init, State final) {
	queue<Transistions> queue;
	Transistions t;
	t.currentState = init;
	t.moves = 0;
	t.tms = 0;
	queue.push(t);
	while (!queue.empty()) {
		t = queue.front();
		queue.pop();
		if (t.currentState == final) {
			break;
		}

		State s0 = getTransition(t.currentState, false);
		State s1 = getTransition(t.currentState, true);

		t.moves++;
		t.tms &= ~(1 << (t.moves-1)); // clear bit
		t.currentState = s0;
		queue.push(t);

		t.tms |= (1 << (t.moves-1));
		t.currentState = s1;
		queue.push(t);
	}

	return t;
}

Jtag_fsm::State Jtag_fsm::getStateFromName(string s) {
	if (s == "RESET")
		return TEST_LOGIC_RESET;
	if (s == "IDLE")
		return RUN_TEST_IDLE;
	if (s == "DRSELECT")
		return SELECT_DR_SCAN;
	if (s == "DRCAPTURE")
		return CAPTURE_DR;
	if (s == "DRSHIFT")
		return SHIFT_DR;
	if (s == "DREXIT1")
		return EXIT1_DR;
	if (s == "DRPAUSE")
		return PAUSE_DR;
	if (s == "DREXIT2")
		return EXIT2_DR;
	if (s == "DRUPDATE")
		return UPDATE_DR;
	if (s == "IRSELECT")
		return SELECT_IR_SCAN;
	if (s == "IRCAPTURE")
		return CAPTURE_IR;
	if (s == "IRSHIFT")
		return SHIFT_IR;
	if (s == "IREXIT1")
		return EXIT1_IR;
	if (s == "IRPAUSE")
		return PAUSE_IR;
	if (s == "IREXIT2")
		return EXIT2_IR;
	if (s == "IRUPDATE")
		return UPDATE_IR;

	cerr << "ERROR! Invalid state name " << s << endl;
	return TEST_LOGIC_RESET;
}
