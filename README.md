# Alchitry Loader (based on D2XX)
This project was originally used as a tool by Alchitry Labs to program the Alchitry boards using the
official D2XX library from FTDI. You can find D2XX here https://ftdichip.com/drivers/d2xx-drivers/

It has been updated to include the Alchitry Au+ and can be used as a stand-alone command line tool for
loading the Alchitry boards without using Alchitry Labs.

Alchitry Labs no longer relies on this loader and instead has a fully Java loader built in.

## Building
Clone the repository

`git clone https://github.com/alchitry/alchitry-loader.git`

Enter the project files

`cd alchitry-loader`

Create the Makefile

`cmake CMakeLists.txt`

Build the project

`make`

Test it out

`./alchitry_loader`

## Usage

```
Usage: "loader arguments"

Arguments:
-e : erase FPGA flash
-l : list detected boards
-h : print this help message
-f config.bin : write FPGA flash
-r config.bin : write FPGA RAM
-u config.data : write FTDI eeprom
-b n : select board "n" (defaults to 0)
-p loader.bin : Au bridge bin
-t TYPE : TYPE can be au, au+, or cu (defaults to au)
```

### Examples
Load a .bin onto an Au's RAM (lost on power cycle)

`./alchitry_loader -t au -r au_config.bin`

Load a .bin onto an Au+'s flash (persistent config)

`./alchitry_loader -t "au+" -f au_config.bin -p ./bridge/au_plus_loader.bin`

Note that to load to the Au or Au+ flash memory you need to specify a bridge bin file. These can be found
in the bridge folder of this repo. This file is loaded onto the Au and allows this loader to program the
flash memory. It acts as a bridge from the JTAG port to the SPI of the flash memory.

The source for the bridge files can be found here https://github.com/alchitry/au-bridge

This isn't needed for the Cu which has direct access to the flash over the SPI protocol.