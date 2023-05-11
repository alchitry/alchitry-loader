cmake_minimum_required(VERSION 3.0)
project(alchitry_loader)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(alchitry_loader
        src/Alchitry_Loader.cpp
        src/config_type.cpp
        src/config_type.h
        src/ftd2xx.h
        src/jtag.cpp
        src/jtag.h
        src/jtag_fsm.cpp
        src/jtag_fsm.h
        src/loader.cpp
        src/loader.h
        src/mingw.thread.h
        src/spi.cpp
        src/spi.h
        src/WinTypes.h)


target_link_libraries(alchitry_loader
        ${CMAKE_SOURCE_DIR}/lib/linux/libftd2xx.a
        ${CMAKE_SOURCE_DIR}/lib/windows/ftd2xx.lib
        pthread)