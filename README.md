# Alchitry Loader (based on D2XX)
This project was originally used as a tool by Alchitry Labs to program the Alchitry boards using the
official D2XX library from FTDI. You can find D2XX here https://ftdichip.com/drivers/d2xx-drivers/

It has been updated to include the Alchitry Au+ and can be used as a stand-alone command line tool for
loading the Alchitry boards without using Alchitry Labs.

Alchitry Labs no longer relies on this loader and instead has a fully Java loader built in.

## Building
Clone the repository.

`git clone https://github.com/alchitry/alchitry-loader.git`

Enter the project files.

`cd alchitry-loader`

Create the Makefile.

`cmake CMakeLists.txt`

Build the project.

`make`

Run it.

`./alchitry_loader`