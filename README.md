# alchitry-loader

Command line loader program for the Au and Cu


## Compilation

To build and install, invoke the build toolchain with

```sh
make
sudo make install
```

## Usage

`alchitry-loader` has a pretty simple interface:

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
```

### Usage Examples

Print the available devices:
```
alchitry-loader -l
```

Program an Alchitry Au:
```
alchitry-loader -p au_loader.bin -f YOUR_BIN_FILE.bin
```

Program an Alchitry Cu:
```
alchitry-loader -f YOUR_BIN_FILE.bin
```

Note that for the Au you need to specify the "au_loader.bin" file. This file is used to bridge the JTAG interface and the FLASH memory. The file is included in `/tools`.
