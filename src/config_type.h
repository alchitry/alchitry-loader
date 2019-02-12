/*
 * config_def.h
 *
 *  Created on: Feb 11, 2019
 *      Author: justin
 */

#ifndef CONFIG_TYPE_H_
#define CONFIG_TYPE_H_

#ifdef _WIN32
#include <windows.h>
#else
#include "WinTypes.h"
#endif
#include "ftd2xx.h"

typedef struct config_data {

		DWORD Signature1;			// Header - must be 0x00000000
		DWORD Signature2;			// Header - must be 0xffffffff
		DWORD Version;				// Header - FT_PROGRAM_DATA version
		//			0 = original
		//			1 = FT2232 extensions
		//			2 = FT232R extensions
		//			3 = FT2232H extensions
		//			4 = FT4232H extensions
		//			5 = FT232H extensions

		WORD VendorId;				// 0x0403
		WORD ProductId;				// 0x6001
		WORD MaxPower;				// 0 < MaxPower <= 500
		WORD PnP;					// 0 = disabled, 1 = enabled
		WORD SelfPowered;			// 0 = bus powered, 1 = self powered
		WORD RemoteWakeup;			// 0 = not capable, 1 = capable
		//
		// Rev4 (FT232B) extensions
		//
		UCHAR Rev4;					// non-zero if Rev4 chip, zero otherwise
		UCHAR IsoIn;				// non-zero if in endpoint is isochronous
		UCHAR IsoOut;				// non-zero if out endpoint is isochronous
		UCHAR PullDownEnable;		// non-zero if pull down enabled
		UCHAR SerNumEnable;			// non-zero if serial number to be used
		UCHAR USBVersionEnable;		// non-zero if chip uses USBVersion
		WORD USBVersion;			// BCD (0x0200 => USB2)
		//
		// Rev 5 (FT2232) extensions
		//
		UCHAR Rev5;					// non-zero if Rev5 chip, zero otherwise
		UCHAR IsoInA;				// non-zero if in endpoint is isochronous
		UCHAR IsoInB;				// non-zero if in endpoint is isochronous
		UCHAR IsoOutA;				// non-zero if out endpoint is isochronous
		UCHAR IsoOutB;				// non-zero if out endpoint is isochronous
		UCHAR PullDownEnable5;		// non-zero if pull down enabled
		UCHAR SerNumEnable5;		// non-zero if serial number to be used
		UCHAR USBVersionEnable5;	// non-zero if chip uses USBVersion
		WORD USBVersion5;			// BCD (0x0200 => USB2)
		UCHAR AIsHighCurrent;		// non-zero if interface is high current
		UCHAR BIsHighCurrent;		// non-zero if interface is high current
		UCHAR IFAIsFifo;			// non-zero if interface is 245 FIFO
		UCHAR IFAIsFifoTar;			// non-zero if interface is 245 FIFO CPU target
		UCHAR IFAIsFastSer;			// non-zero if interface is Fast serial
		UCHAR AIsVCP;				// non-zero if interface is to use VCP drivers
		UCHAR IFBIsFifo;			// non-zero if interface is 245 FIFO
		UCHAR IFBIsFifoTar;			// non-zero if interface is 245 FIFO CPU target
		UCHAR IFBIsFastSer;			// non-zero if interface is Fast serial
		UCHAR BIsVCP;				// non-zero if interface is to use VCP drivers
		//
		// Rev 6 (FT232R) extensions
		//
		UCHAR UseExtOsc;			// Use External Oscillator
		UCHAR HighDriveIOs;			// High Drive I/Os
		UCHAR EndpointSize;			// Endpoint size
		UCHAR PullDownEnableR;		// non-zero if pull down enabled
		UCHAR SerNumEnableR;		// non-zero if serial number to be used
		UCHAR InvertTXD;			// non-zero if invert TXD
		UCHAR InvertRXD;			// non-zero if invert RXD
		UCHAR InvertRTS;			// non-zero if invert RTS
		UCHAR InvertCTS;			// non-zero if invert CTS
		UCHAR InvertDTR;			// non-zero if invert DTR
		UCHAR InvertDSR;			// non-zero if invert DSR
		UCHAR InvertDCD;			// non-zero if invert DCD
		UCHAR InvertRI;				// non-zero if invert RI
		UCHAR Cbus0;				// Cbus Mux control
		UCHAR Cbus1;				// Cbus Mux control
		UCHAR Cbus2;				// Cbus Mux control
		UCHAR Cbus3;				// Cbus Mux control
		UCHAR Cbus4;				// Cbus Mux control
		UCHAR RIsD2XX;				// non-zero if using D2XX driver
		//
		// Rev 7 (FT2232H) Extensions
		//
		UCHAR PullDownEnable7;		// non-zero if pull down enabled
		UCHAR SerNumEnable7;		// non-zero if serial number to be used
		UCHAR ALSlowSlew;			// non-zero if AL pins have slow slew
		UCHAR ALSchmittInput;		// non-zero if AL pins are Schmitt input
		UCHAR ALDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR AHSlowSlew;			// non-zero if AH pins have slow slew
		UCHAR AHSchmittInput;		// non-zero if AH pins are Schmitt input
		UCHAR AHDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BLSlowSlew;			// non-zero if BL pins have slow slew
		UCHAR BLSchmittInput;		// non-zero if BL pins are Schmitt input
		UCHAR BLDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BHSlowSlew;			// non-zero if BH pins have slow slew
		UCHAR BHSchmittInput;		// non-zero if BH pins are Schmitt input
		UCHAR BHDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR IFAIsFifo7;			// non-zero if interface is 245 FIFO
		UCHAR IFAIsFifoTar7;		// non-zero if interface is 245 FIFO CPU target
		UCHAR IFAIsFastSer7;		// non-zero if interface is Fast serial
		UCHAR AIsVCP7;				// non-zero if interface is to use VCP drivers
		UCHAR IFBIsFifo7;			// non-zero if interface is 245 FIFO
		UCHAR IFBIsFifoTar7;		// non-zero if interface is 245 FIFO CPU target
		UCHAR IFBIsFastSer7;		// non-zero if interface is Fast serial
		UCHAR BIsVCP7;				// non-zero if interface is to use VCP drivers
		UCHAR PowerSaveEnable;		// non-zero if using BCBUS7 to save power for self-powered designs
		//
		// Rev 8 (FT4232H) Extensions
		//
		UCHAR PullDownEnable8;		// non-zero if pull down enabled
		UCHAR SerNumEnable8;		// non-zero if serial number to be used
		UCHAR ASlowSlew;			// non-zero if A pins have slow slew
		UCHAR ASchmittInput;		// non-zero if A pins are Schmitt input
		UCHAR ADriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BSlowSlew;			// non-zero if B pins have slow slew
		UCHAR BSchmittInput;		// non-zero if B pins are Schmitt input
		UCHAR BDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR CSlowSlew;			// non-zero if C pins have slow slew
		UCHAR CSchmittInput;		// non-zero if C pins are Schmitt input
		UCHAR CDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR DSlowSlew;			// non-zero if D pins have slow slew
		UCHAR DSchmittInput;		// non-zero if D pins are Schmitt input
		UCHAR DDriveCurrent;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR ARIIsTXDEN;			// non-zero if port A uses RI as RS485 TXDEN
		UCHAR BRIIsTXDEN;			// non-zero if port B uses RI as RS485 TXDEN
		UCHAR CRIIsTXDEN;			// non-zero if port C uses RI as RS485 TXDEN
		UCHAR DRIIsTXDEN;			// non-zero if port D uses RI as RS485 TXDEN
		UCHAR AIsVCP8;				// non-zero if interface is to use VCP drivers
		UCHAR BIsVCP8;				// non-zero if interface is to use VCP drivers
		UCHAR CIsVCP8;				// non-zero if interface is to use VCP drivers
		UCHAR DIsVCP8;				// non-zero if interface is to use VCP drivers
		//
		// Rev 9 (FT232H) Extensions
		//
		UCHAR PullDownEnableH;		// non-zero if pull down enabled
		UCHAR SerNumEnableH;		// non-zero if serial number to be used
		UCHAR ACSlowSlewH;			// non-zero if AC pins have slow slew
		UCHAR ACSchmittInputH;		// non-zero if AC pins are Schmitt input
		UCHAR ACDriveCurrentH;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR ADSlowSlewH;			// non-zero if AD pins have slow slew
		UCHAR ADSchmittInputH;		// non-zero if AD pins are Schmitt input
		UCHAR ADDriveCurrentH;		// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR Cbus0H;				// Cbus Mux control
		UCHAR Cbus1H;				// Cbus Mux control
		UCHAR Cbus2H;				// Cbus Mux control
		UCHAR Cbus3H;				// Cbus Mux control
		UCHAR Cbus4H;				// Cbus Mux control
		UCHAR Cbus5H;				// Cbus Mux control
		UCHAR Cbus6H;				// Cbus Mux control
		UCHAR Cbus7H;				// Cbus Mux control
		UCHAR Cbus8H;				// Cbus Mux control
		UCHAR Cbus9H;				// Cbus Mux control
		UCHAR IsFifoH;				// non-zero if interface is 245 FIFO
		UCHAR IsFifoTarH;			// non-zero if interface is 245 FIFO CPU target
		UCHAR IsFastSerH;			// non-zero if interface is Fast serial
		UCHAR IsFT1248H;			// non-zero if interface is FT1248
		UCHAR FT1248CpolH;			// FT1248 clock polarity - clock idle high (1) or clock idle low (0)
		UCHAR FT1248LsbH;			// FT1248 data is LSB (1) or MSB (0)
		UCHAR FT1248FlowControlH;	// FT1248 flow control enable
		UCHAR IsVCPH;				// non-zero if interface is to use VCP drivers
		UCHAR PowerSaveEnableH;		// non-zero if using ACBUS7 to save power for self-powered designs

	} CONFIG_DATA;

	void ft_to_config(CONFIG_DATA*, PFT_PROGRAM_DATA);
	void config_to_ft(PFT_PROGRAM_DATA, CONFIG_DATA*);

#endif /* CONFIG_TYPE_H_ */
