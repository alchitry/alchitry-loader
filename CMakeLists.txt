cmake_minimum_required(VERSION 3.5)
project(alchitry_loader)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_OSX_ARCHITECTURES "x86_64")

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

target_link_libraries(alchitry_loader pthread)

if (APPLE)
        find_library(IOKIT IOKit REQUIRED)
        find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
        target_link_libraries(alchitry_loader
                objc
                ${IOKIT}
                ${CORE_FOUNDATION}
                ${CMAKE_SOURCE_DIR}/lib/osx/libftd2xx.a)
elseif(LINUX)
        target_link_libraries(alchitry_loader ${CMAKE_SOURCE_DIR}/lib/linux/libftd2xx.a)
else()
        target_link_libraries(alchitry_loader ${CMAKE_SOURCE_DIR}/lib/windows/ftd2xx.lib)
endif()

