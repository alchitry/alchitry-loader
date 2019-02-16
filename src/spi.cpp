/*
 *  iceprog -- simple programming tool for FTDI-based Lattice iCE programmers
 *
 *  Copyright (C) 2015  Clifford Wolf <clifford@clifford.at>
 *  Copyright (C) 2018  Piotr Esden-Tempski <piotr@esden.net>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Relevant Documents:
 *  -------------------
 *  http://www.latticesemi.com/~/media/Documents/UserManuals/EI/icestickusermanual.pdf
 *  http://www.micron.com/~/media/documents/products/data-sheet/nor-flash/serial-nor/n25q/n25q_32mb_3v_65nm.pdf
 *  http://www.ftdichip.com/Support/Documents/AppNotes/AN_108_Command_Processor_for_MPSSE_and_MCU_Host_Bus_Emulation_Modes.pdf
 */

#include "spi.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "mingw.thread.h"
#else
#include <thread>
#endif

#include <chrono>

using namespace std;

/* MPSSE engine command definitions */
enum mpsse_cmd {
	/* Mode commands */
	MC_SETB_LOW = 0x80, /* Set Data bits LowByte */
	MC_READB_LOW = 0x81, /* Read Data bits LowByte */
	MC_SETB_HIGH = 0x82, /* Set Data bits HighByte */
	MC_READB_HIGH = 0x83, /* Read data bits HighByte */
	MC_LOOPBACK_EN = 0x84, /* Enable loopback */
	MC_LOOPBACK_DIS = 0x85, /* Disable loopback */
	MC_SET_CLK_DIV = 0x86, /* Set clock divisor */
	MC_FLUSH = 0x87, /* Flush buffer fifos to the PC. */
	MC_WAIT_H = 0x88, /* Wait on GPIOL1 to go high. */
	MC_WAIT_L = 0x89, /* Wait on GPIOL1 to go low. */
	MC_TCK_X5 = 0x8A, /* Disable /5 div, enables 60MHz master clock */
	MC_TCK_D5 = 0x8B, /* Enable /5 div, backward compat to FT2232D */
	MC_EN_3PH_CLK = 0x8C, /* Enable 3 phase clk, DDR I2C */
	MC_DIS_3PH_CLK = 0x8D, /* Disable 3 phase clk */
	MC_CLK_N = 0x8E, /* Clock every bit, used for JTAG */
	MC_CLK_N8 = 0x8F, /* Clock every byte, used for JTAG */
	MC_CLK_TO_H = 0x94, /* Clock until GPIOL1 goes high */
	MC_CLK_TO_L = 0x95, /* Clock until GPIOL1 goes low */
	MC_EN_ADPT_CLK = 0x96, /* Enable adaptive clocking */
	MC_DIS_ADPT_CLK = 0x97, /* Disable adaptive clocking */
	MC_CLK8_TO_H = 0x9C, /* Clock until GPIOL1 goes high, count bytes */
	MC_CLK8_TO_L = 0x9D, /* Clock until GPIOL1 goes low, count bytes */
	MC_TRI = 0x9E, /* Set IO to only drive on 0 and tristate on 1 */
	/* CPU mode commands */
	MC_CPU_RS = 0x90, /* CPUMode read short address */
	MC_CPU_RE = 0x91, /* CPUMode read extended address */
	MC_CPU_WS = 0x92, /* CPUMode write short address */
	MC_CPU_WE = 0x93, /* CPUMode write extended address */
};

// ---------------------------------------------------------
// FLASH definitions
// ---------------------------------------------------------

/* Transfer Command bits */

/* All byte based commands consist of:
 * - Command byte
 * - Length lsb
 * - Length msb
 *
 * If data out is enabled the data follows after the above command bytes,
 * otherwise no additional data is needed.
 * - Data * n
 *
 * All bit based commands consist of:
 * - Command byte
 * - Length
 *
 * If data out is enabled a byte containing bitst to transfer follows.
 * Otherwise no additional data is needed. Only up to 8 bits can be transferred
 * per transaction when in bit mode.
 */

/* b 0000 0000
 *   |||| |||`- Data out negative enable. Update DO on negative clock edge.
 *   |||| ||`-- Bit count enable. When reset count represents bytes.
 *   |||| |`--- Data in negative enable. Latch DI on negative clock edge.
 *   |||| `---- LSB enable. When set clock data out LSB first.
 *   ||||
 *   |||`------ Data out enable
 *   ||`------- Data in enable
 *   |`-------- TMS mode enable
 *   `--------- Special command mode enable. See mpsse_cmd enum.
 */

#define MC_DATA_TMS  (0x40) /* When set use TMS mode */
#define MC_DATA_IN   (0x20) /* When set read data (Data IN) */
#define MC_DATA_OUT  (0x10) /* When set write data (Data OUT) */
#define MC_DATA_LSB  (0x08) /* When set input/output data LSB first. */
#define MC_DATA_ICN  (0x04) /* When set receive data on negative clock edge */
#define MC_DATA_BITS (0x02) /* When set count bits not bytes */
#define MC_DATA_OCN  (0x01) /* When set update data on negative clock edge */

/* Flash command definitions */
/* This command list is based on the Winbond W25Q128JV Datasheet */
enum flash_cmd {
	FC_WE = 0x06, /* Write Enable */
	FC_SRWE = 0x50, /* Volatile SR Write Enable */
	FC_WD = 0x04, /* Write Disable */
	FC_RPD = 0xAB, /* Release Power-Down, returns Device ID */
	FC_MFGID = 0x90, /*  Read Manufacturer/Device ID */
	FC_JEDECID = 0x9F, /* Read JEDEC ID */
	FC_UID = 0x4B, /* Read Unique ID */
	FC_RD = 0x03, /* Read Data */
	FC_FR = 0x0B, /* Fast Read */
	FC_PP = 0x02, /* Page Program */
	FC_SE = 0x20, /* Sector Erase 4kb */
	FC_BE32 = 0x52, /* Block Erase 32kb */
	FC_BE64 = 0xD8, /* Block Erase 64kb */
	FC_CE = 0xC7, /* Chip Erase */
	FC_RSR1 = 0x05, /* Read Status Register 1 */
	FC_WSR1 = 0x01, /* Write Status Register 1 */
	FC_RSR2 = 0x35, /* Read Status Register 2 */
	FC_WSR2 = 0x31, /* Write Status Register 2 */
	FC_RSR3 = 0x15, /* Read Status Register 3 */
	FC_WSR3 = 0x11, /* Write Status Register 3 */
	FC_RSFDP = 0x5A, /* Read SFDP Register */
	FC_ESR = 0x44, /* Erase Security Register */
	FC_PSR = 0x42, /* Program Security Register */
	FC_RSR = 0x48, /* Read Security Register */
	FC_GBL = 0x7E, /* Global Block Lock */
	FC_GBU = 0x98, /* Global Block Unlock */
	FC_RBL = 0x3D, /* Read Block Lock */
	FC_RPR = 0x3C, /* Read Sector Protection Registers (adesto) */
	FC_IBL = 0x36, /* Individual Block Lock */
	FC_IBU = 0x39, /* Individual Block Unlock */
	FC_EPS = 0x75, /* Erase / Program Suspend */
	FC_EPR = 0x7A, /* Erase / Program Resume */
	FC_PD = 0xB9, /* Power-down */
	FC_QPI = 0x38, /* Enter QPI mode */
	FC_ERESET = 0x66, /* Enable Reset */
	FC_RESET = 0x99, /* Reset Device */
};

Spi::Spi() {
	ftHandle = 0;
	active = false;
	verbose = false;
}

FT_STATUS Spi::connect(unsigned int devNumber) {
	return FT_Open(devNumber, &ftHandle);
}

FT_STATUS Spi::disconnect() {
	active = false;
	return FT_Close(ftHandle);
}

bool Spi::initialize() {
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
	ftStatus |= FT_SetLatencyTimer(ftHandle, 1); //Set the latency timer 1ms
	ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x00);	        //Reset controller
	ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x02);	        //Enable MPSSE mode

	if (ftStatus != FT_OK) {
		cerr << "Failed to set initial configuration!" << endl;
		return false;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for all the USB stuff to complete and work

	if (!sync_mpsse()){
		cerr << "Failed to sync with MPSSE!" << endl;
		return false;
	}

	if (!config_spi()){
		cerr << "Failed to set SPI configuration!" << endl;
		return false;
	}

	active = true;

	return true;
}

bool Spi::sync_mpsse() {
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
	// Send off the BAD commands
	dwNumBytesToSend = 0; // Reset output buffer pointer
	do {
		ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
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

bool Spi::config_spi() {
	FT_STATUS ftStatus;
	BYTE byOutputBuffer[64]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	DWORD dwNumBytesToSend = 0; // Index to the output buffer
	DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	DWORD dwClockDivisor = 0x0; // Value of clock divisor, SCL Frequency = 60/((1+0x0)*2) (MHz) = 30MHz
	// -----------------------------------------------------------
	// Configure the MPSSE settings for SPI
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
	// ADBUS0 SCK output 1 low 0
	// ADBUS1 MOSI output 1 low 0
	// ADBUS2 MISO input 0 low 0
	// ADBUS3 NC output 1 low 0
	// ADBUS4 SS output 1 low 0
	// ADBUS5 NC output 1 low 0
	// ADBUS6 CDONE input 0 low 0
	// ADBUS7 CRESET output 1 low 0
	byOutputBuffer[dwNumBytesToSend++] = 0x80;
	// Set data bits low-byte of MPSSE port
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	// Initial state config above
	byOutputBuffer[dwNumBytesToSend++] = 0xBB;
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

void Spi::check_rx() {
	FT_STATUS ftStatus;
	BYTE byInputBuffer[1];
	DWORD dwNumBytesRead = 0;

	while (1) {
		ftStatus = FT_Read(ftHandle, &byInputBuffer, 1, &dwNumBytesRead);
		if (ftStatus != FT_OK)
			break;
		cerr << "Unexpected rx byte: " << byInputBuffer[0] << endl;
	}
}

void Spi::error(int status) {
	check_rx();
	cerr << "ABORT." << endl;
	if (active)
		disconnect();
	exit(status);
}

BYTE Spi::recv_byte() {
	FT_STATUS ftStatus;
	BYTE byInputBuffer[1];
	DWORD dwNumBytesRead = 0;
	while (1) {
		ftStatus = FT_Read(ftHandle, &byInputBuffer, 1, &dwNumBytesRead);
		if (ftStatus != FT_OK) {
			cerr << "Read error." << endl;
			error(2);
		}
		if (dwNumBytesRead == 1)
			break;
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
	return byInputBuffer[0];
}

void Spi::send_byte(uint8_t data) {
	FT_STATUS ftStatus;
	BYTE byOutputBuffer[1];
	DWORD dwNumBytesSent = 0;

	byOutputBuffer[0] = data;

	ftStatus = FT_Write(ftHandle, byOutputBuffer, 1, &dwNumBytesSent);
	if (ftStatus != FT_OK) {
		cerr << "Write error!" << endl;
		error(2);
	}
	if (dwNumBytesSent != 1) {
		cerr << "Write error (single byte, received " << dwNumBytesSent
				<< ", expected 1)." << endl;
		error(2);
	}
}

void Spi::send_spi(uint8_t *data, int n) {
	if (n < 1)
		return;

	/* Output only, update data on negative clock edge. */
	send_byte(MC_DATA_OUT | MC_DATA_OCN);
	send_byte(n - 1);
	send_byte((n - 1) >> 8);

	FT_STATUS ftStatus;
	DWORD dwNumBytesSent = 0;

	ftStatus = FT_Write(ftHandle, data, n, &dwNumBytesSent);
	if (ftStatus != FT_OK) {
		cerr << "Write error!" << endl;
		error(2);
	}

	if (dwNumBytesSent != (unsigned int) n) {
		fprintf(stderr, "Write error (chunk, rc=%u, expected %d).\n",
				dwNumBytesSent, n);
		error(2);
	}
}

void Spi::xfer_spi(uint8_t *data, int n) {
	if (n < 1)
		return;

	/* Input and output, update data on negative edge read on positive. */
	send_byte(MC_DATA_IN | MC_DATA_OUT | MC_DATA_OCN);
	send_byte(n - 1);
	send_byte((n - 1) >> 8);

	FT_STATUS ftStatus;
	DWORD dwNumBytesSent = 0;

	ftStatus = FT_Write(ftHandle, data, n, &dwNumBytesSent);
	if (ftStatus != FT_OK) {
		cerr << "Write error!" << endl;
		error(2);
	}

	if (dwNumBytesSent != (unsigned int) n) {
		fprintf(stderr, "Write error (chunk, rc=%u, expected %d).\n",
				dwNumBytesSent, n);
		error(2);
	}

	for (int i = 0; i < n; i++)
		data[i] = recv_byte();
}

uint8_t Spi::xfer_spi_bits(uint8_t data, int n) {
	if (n < 1)
		return 0;

	/* Input and output, update data on negative edge read on positive, bits. */
	send_byte(MC_DATA_IN | MC_DATA_OUT | MC_DATA_OCN | MC_DATA_BITS);
	send_byte(n - 1);
	send_byte(data);

	return recv_byte();
}

void Spi::set_gpio(int slavesel_b, int creset_b) {
	uint8_t gpio = 0;

	if (slavesel_b) {
		// ADBUS4 (GPIOL0)
		gpio |= 0x10;
	}

	if (creset_b) {
		// ADBUS7 (GPIOL3)
		gpio |= 0x80;
	}

	send_byte(MC_SETB_LOW);
	send_byte(gpio); /* Value */
	send_byte(0x93); /* Direction */
}

int Spi::get_cdone() {
	uint8_t data;
	send_byte(MC_READB_LOW);
	data = recv_byte();
	// ADBUS6 (GPIOL2)
	return (data & 0x40) != 0;
}

// ---------------------------------------------------------
// FLASH function implementations
// ---------------------------------------------------------

// the FPGA reset is released so also FLASH chip select should be deasserted
void Spi::flash_release_reset() {
	set_gpio(1, 1);
}

// FLASH chip select assert
// should only happen while FPGA reset is asserted
void Spi::flash_chip_select() {
	set_gpio(0, 0);
}

// FLASH chip select deassert
void Spi::flash_chip_deselect() {
	set_gpio(1, 0);
}

// SRAM reset is the same as flash_chip_select()
// For ease of code reading we use this function instead
void Spi::sram_reset() {
	// Asserting chip select and reset lines
	set_gpio(0, 0);
}

// SRAM chip select assert
// When accessing FPGA SRAM the reset should be released
void Spi::sram_chip_select() {
	set_gpio(0, 1);
}

void Spi::flash_read_id() {
	/* JEDEC ID structure:
	 * Byte No. | Data Type
	 * ---------+----------
	 *        0 | FC_JEDECID Request Command
	 *        1 | MFG ID
	 *        2 | Dev ID 1
	 *        3 | Dev ID 2
	 *        4 | Ext Dev Str Len
	 */

	uint8_t data[260] = { FC_JEDECID };
	int len = 5; // command + 4 response bytes

	if (verbose)
		fprintf(stdout, "read flash ID..\n");

	flash_chip_select();

	// Write command and read first 4 bytes
	xfer_spi(data, len);

	if (data[4] == 0xFF)
		fprintf(stderr, "Extended Device String Length is 0xFF, "
				"this is likely a read error. Ignorig...\n");
	else {
		// Read extended JEDEC ID bytes
		if (data[4] != 0) {
			len += data[4];
			xfer_spi(data + 5, len - 5);
		}
	}

	flash_chip_deselect();

	fprintf(stdout, "flash ID:");
	for (int i = 1; i < len; i++)
		fprintf(stdout, " 0x%02X", data[i]);
	fprintf(stdout, "\n");
}

void Spi::flash_reset() {
	flash_chip_select();
	xfer_spi_bits(0xFF, 8);
	flash_chip_deselect();

	flash_chip_select();
	xfer_spi_bits(0xFF, 2);
	flash_chip_deselect();
}

void Spi::flash_power_up() {
	uint8_t data_rpd[1] = { FC_RPD };
	flash_chip_select();
	xfer_spi(data_rpd, 1);
	flash_chip_deselect();
}

void Spi::flash_power_down() {
	uint8_t data[1] = { FC_PD };
	flash_chip_select();
	xfer_spi(data, 1);
	flash_chip_deselect();
}

uint8_t Spi::flash_read_status() {
	uint8_t data[2] = { FC_RSR1 };

	flash_chip_select();
	xfer_spi(data, 2);
	flash_chip_deselect();

	if (verbose) {
		fprintf(stdout, "SR1: 0x%02X\n", data[1]);
		fprintf(stdout, " - SPRL: %s\n",
				((data[1] & (1 << 7)) == 0) ? "unlocked" : "locked");
		fprintf(stdout, " -  SPM: %s\n",
				((data[1] & (1 << 6)) == 0) ?
						"Byte/Page Prog Mode" : "Sequential Prog Mode");
		fprintf(stdout, " -  EPE: %s\n",
				((data[1] & (1 << 5)) == 0) ?
						"Erase/Prog success" : "Erase/Prog error");
		fprintf(stdout, "-  SPM: %s\n",
				((data[1] & (1 << 4)) == 0) ?
						"~WP asserted" : "~WP deasserted");
		fprintf(stdout, " -  SWP: ");
		switch ((data[1] >> 2) & 0x3) {
		case 0:
			fprintf(stdout, "All sectors unprotected\n");
			break;
		case 1:
			fprintf(stdout, "Some sectors protected\n");
			break;
		case 2:
			fprintf(stdout, "Reserved (xxxx 10xx)\n");
			break;
		case 3:
			fprintf(stdout, "All sectors protected\n");
			break;
		}
		fprintf(stdout, " -  WEL: %s\n",
				((data[1] & (1 << 1)) == 0) ?
						"Not write enabled" : "Write enabled");
		fprintf(stdout, " - ~RDY: %s\n",
				((data[1] & (1 << 0)) == 0) ? "Ready" : "Busy");
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	return data[1];
}

void Spi::flash_write_enable() {
	if (verbose) {
		fprintf(stdout, "status before enable:\n");
		flash_read_status();
	}

	if (verbose)
		fprintf(stdout, "write enable..\n");

	uint8_t data[1] = { FC_WE };
	flash_chip_select();
	xfer_spi(data, 1);
	flash_chip_deselect();

	if (verbose) {
		fprintf(stdout, "status after enable:\n");
		flash_read_status();
	}
}

void Spi::flash_bulk_erase() {
	if (verbose)
		fprintf(stdout, "bulk erase..\n");

	uint8_t data[1] = { FC_CE };
	flash_chip_select();
	xfer_spi(data, 1);
	flash_chip_deselect();
}

void Spi::flash_64kB_sector_erase(int addr) {
	if (verbose)
		fprintf(stdout, "erase 64kB sector at 0x%06X..\n", addr);

	uint8_t command[4] = { FC_BE64, (uint8_t) (addr >> 16),
			(uint8_t) (addr >> 8), (uint8_t) addr };

	flash_chip_select();
	send_spi(command, 4);
	flash_chip_deselect();
}

void Spi::flash_prog(int addr, uint8_t *data, int n) {
	if (verbose)
		fprintf(stdout, "prog 0x%06X +0x%03X..\n", addr, n);

	uint8_t command[4] = { FC_PP, (uint8_t) (addr >> 16), (uint8_t) (addr >> 8),
			(uint8_t) addr };

	flash_chip_select();
	send_spi(command, 4);
	send_spi(data, n);
	flash_chip_deselect();

	if (verbose)
		for (int i = 0; i < n; i++)
			fprintf(stderr, "%02x%c", data[i],
					i == n - 1 || i % 32 == 31 ? '\n' : ' ');
}

void Spi::flash_read(int addr, uint8_t *data, int n) {
	if (verbose)
		fprintf(stdout, "read 0x%06X +0x%03X..\n", addr, n);

	uint8_t command[4] = { FC_RD, (uint8_t) (addr >> 16), (uint8_t) (addr >> 8),
			(uint8_t) addr };

	flash_chip_select();
	send_spi(command, 4);
	memset(data, 0, n);
	xfer_spi(data, n);
	flash_chip_deselect();

	if (verbose)
		for (int i = 0; i < n; i++)
			fprintf(stderr, "%02x%c", data[i],
					i == n - 1 || i % 32 == 31 ? '\n' : ' ');
}

void Spi::flash_wait() {
	if (verbose)
		fprintf(stderr, "waiting..");

	int count = 0;
	while (1) {
		uint8_t data[2] = { FC_RSR1 };

		flash_chip_select();
		xfer_spi(data, 2);
		flash_chip_deselect();

		if ((data[1] & 0x01) == 0) {
			if (count < 2) {
				count++;
				if (verbose) {
					fprintf(stderr, "r");
					fflush(stderr);
				}
			} else {
				if (verbose) {
					fprintf(stderr, "R");
					fflush(stderr);
				}
				break;
			}
		} else {
			if (verbose) {
				fprintf(stderr, ".");
				fflush(stderr);
			}
			count = 0;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (verbose)
		fprintf(stderr, "\n");

}

void Spi::flash_disable_protection() {
	fprintf(stderr, "disable flash protection...\n");

	// Write Status Register 1 <- 0x00
	uint8_t data[2] = { FC_WSR1, 0x00 };
	flash_chip_select();
	xfer_spi(data, 2);
	flash_chip_deselect();

	flash_wait();

	// Read Status Register 1
	data[0] = FC_RSR1;

	flash_chip_select();
	xfer_spi(data, 2);
	flash_chip_deselect();

	if (data[1] != 0x00)
		fprintf(stderr,
				"failed to disable protection, SR now equal to 0x%02x (expected 0x00)\n",
				data[1]);

}

bool Spi::eraseFlash() {
	fprintf(stdout, "reset..\n");

	flash_chip_deselect();
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	fprintf(stdout, "cdone: %s\n", get_cdone() ? "high" : "low");

	flash_reset();
	flash_power_up();

	flash_read_id();

	flash_write_enable();
	flash_bulk_erase();
	flash_wait();

	// ---------------------------------------------------------
	// Reset
	// ---------------------------------------------------------

	flash_power_down();

	set_gpio(1, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	fprintf(stdout, "cdone: %s\n", get_cdone() ? "high" : "low");

	return true;
}
bool Spi::writeBin(string filename) {
	int rw_offset = 0;

	FILE *f = NULL;
	long file_size = -1;

	f = fopen(filename.c_str(), "rb");

	if (f == NULL) {
		fprintf(stderr, "Can't open '%s' for reading: ", filename.c_str());
		return false;
	}

	if (fseek(f, 0L, SEEK_END) != -1) {
		file_size = ftell(f);
		if (file_size == -1) {
			fprintf(stderr, "%s: ftell: ", filename.c_str());
			return false;
		}
		if (fseek(f, 0L, SEEK_SET) == -1) {
			fprintf(stderr, "%s: fseek: ", filename.c_str());
			return false;
		}
	} else {
		FILE *pipe = f;

		f = tmpfile();
		if (f == NULL) {
			fprintf(stderr, "can't open temporary file\n");
			return false;
		}
		file_size = 0;

		while (true) {
			static unsigned char buffer[4096];
			size_t rc = fread(buffer, 1, 4096, pipe);
			if (rc <= 0)
				break;
			size_t wc = fwrite(buffer, 1, rc, f);
			if (wc != rc) {
				fprintf(stderr, "can't write to temporary file\n");
				return false;
			}
			file_size += rc;
		}
		fclose(pipe);

		/* now seek to the beginning so we can
		 start reading again */
		fseek(f, 0, SEEK_SET);
	}

	cout << "Resetting..." << endl;

	flash_chip_deselect();
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	cout << "cdone: " << (get_cdone() ? "high" : "low") << endl;

	flash_reset();
	flash_power_up();

	flash_read_id();

	int begin_addr = rw_offset & ~0xffff;
	int end_addr = (rw_offset + file_size + 0xffff) & ~0xffff;

	for (int addr = begin_addr; addr < end_addr; addr += 0x10000) {
		flash_write_enable();
		flash_64kB_sector_erase(addr);
		if (verbose) {
			fprintf(stderr, "Status after block erase:\n");
			flash_read_status();
		}
		flash_wait();
	}

	cout << "Programming... ";

	for (int rc, addr = 0; true; addr += rc) {
		uint8_t buffer[256];
		int page_size = 256 - (rw_offset + addr) % 256;
		rc = fread(buffer, 1, page_size, f);
		if (rc <= 0)
			break;
		flash_write_enable();
		flash_prog(rw_offset + addr, buffer, rc);
		flash_wait();
	}

	cout << "Done." << endl;

	/* seek to the beginning for second pass */
	fseek(f, 0, SEEK_SET);

	// ---------------------------------------------------------
	// Reset
	// ---------------------------------------------------------

	flash_power_down();

	set_gpio(1, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	cout << "cdone: " << (get_cdone() ? "high" : "low") << endl;
	cout << "Done." << endl;

	if (f != NULL && f != stdin && f != stdout)
		fclose(f);
	return true;
}
