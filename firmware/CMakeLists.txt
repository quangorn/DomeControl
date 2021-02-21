cmake_minimum_required(VERSION 3.2)
project(DomeControl C)

SET(MCU "atmega8")
SET(F_CPU "12000000")

#-fpack-struct option is mandatory for compiler (pragma pack analog)
SET(CMAKE_C_FLAGS "-mmcu=${MCU} -DF_CPU=${F_CPU} -DNDEBUG -Os -fno-move-loop-invariants -fno-tree-scev-cprop -fno-inline-small-functions -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -Wall -Wno-unknown-pragmas -std=gnu99")
SET(CMAKE_C_LINK_FLAGS "-mmcu=${MCU} -Wl,--relax,--gc-sections")

include_directories(.)

file(GLOB_RECURSE SOURCE_FILES "source/*.c" "source/*.h")
add_executable(DomeControl ${SOURCE_FILES})