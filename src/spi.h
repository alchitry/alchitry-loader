/*
 * spi.h
 *
 *  Created on: Feb 5, 2019
 *      Author: justin
 */

#ifndef SPI_H_
#define SPI_H_

#include "ftd2xx.h"
#include <unistd.h>
#include <string>
#include <stdint.h>

using namespace std;

class Spi {
	FT_HANDLE ftHandle;
	unsigned int uiDevIndex = 0xF; // The device in the list that is used
	bool active;
	bool verbose;

public:
	Spi();
	FT_STATUS connect(unsigned int);
	FT_STATUS disconnect();
	bool initialize();
	bool eraseFlash();
	bool writeBin(string);

private:
	bool sync_mpsse();
	bool config_spi();
	static void hexToByte(string, BYTE*);
	bool flush();
	bool compareHexString(string, string, string);
	void check_rx();
	void error(int);
	BYTE recv_byte();
	void send_byte(BYTE data);
	void send_spi(uint8_t *data, int n);
	void xfer_spi(uint8_t *data, int n);
	uint8_t xfer_spi_bits(uint8_t data, int n);
	void set_gpio(int slavesel_b, int creset_b);
	int get_cdone();
	void flash_release_reset();
	void flash_chip_select();
	void flash_chip_deselect();
	void sram_reset();
	void sram_chip_select();
	void flash_read_id();
	void flash_reset();
	void flash_power_up();
	void flash_power_down();
	uint8_t flash_read_status();
	void flash_write_enable();
	void flash_bulk_erase();
	void flash_64kB_sector_erase(int addr);
	void flash_prog(int addr, uint8_t *data, int n);
	void flash_read(int addr, uint8_t *data, int n);
	void flash_wait();
	void flash_disable_protection();

};

#endif /* SPI_H_ */
