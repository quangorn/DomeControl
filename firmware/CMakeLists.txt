cmake_minimum_required(VERSION 3.2)
project(DomeControl C)

SET(MCU "atmega8")
SET(F_CPU "12000000")

#-fpack-struct option is mandatory for compiler (pragma pack analog)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmcu=${MCU} -DF_CPU=${F_CPU} -Os -fno-move-loop-invariants -fno-tree-scev-cprop -fno-inline-small-functions -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -Wall -Wno-unknown-pragmas -Werror -std=gnu11")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -mmcu=${MCU} -Wl,--relax,--gc-sections -Wl,-u,vfprintf -lprintf_min")
SET(CMAKE_C_FLAGS_RELEASE "-DNDEBUG") #needed to remove default -O3 option
# The Debug configuration no longer adds -DDEBUG. The trace under `#ifdef DEBUG` goes to the same
# USART as the protocol, so a -DDEBUG build answers `IM#` with `Encoder value: 42` and the ASCOM
# driver throws on it: the trace and the protocol cannot share the port. The code is kept so it can
# be re-enabled by hand (a logic analyzer, or a second UART on a bench board) with
# -DCMAKE_C_FLAGS_DEBUG="-DDEBUG".

include_directories(.)

file(GLOB_RECURSE SOURCE_FILES "source/*.c" "source/*.h")
add_executable(DomeControl ${SOURCE_FILES})