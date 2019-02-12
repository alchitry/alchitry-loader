/*
 * jtag_fsm.h
 *
 *  Created on: May 24, 2017
 *      Author: justin
 */

#ifndef JTAG_FSM_H_
#define JTAG_FSM_H_

#include <cstdint>
#include <string>

using namespace std;

class Jtag_fsm {
public:
	enum State {
		TEST_LOGIC_RESET,
		RUN_TEST_IDLE,
		SELECT_DR_SCAN,
		CAPTURE_DR,
		SHIFT_DR,
		EXIT1_DR,
		PAUSE_DR,
		EXIT2_DR,
		UPDATE_DR,
		SELECT_IR_SCAN,
		CAPTURE_IR,
		SHIFT_IR,
		EXIT1_IR,
		PAUSE_IR,
		EXIT2_IR,
		UPDATE_IR
	};
	class Transistions {
	public:
		State currentState;
		uint8_t tms;
		uint8_t moves;
	};

	static Transistions getTransitions(State, State);
	static State getStateFromName(string);


private:
	static State getTransition(State, bool);

};

#endif /* JTAG_FSM_H_ */
