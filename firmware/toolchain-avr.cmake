# AVR toolchain for the host (Linux) build of the firmware.
#
# The CLion setup uses its own toolchain file, which is not in the repository; this one is the
# Linux equivalent described in AGENTS.md §3:
#
#   cmake -S firmware -B firmware/cmake-build-avr -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/toolchain-avr.cmake"
#   cmake --build firmware/cmake-build-avr --config Release
#
# The toolchain path must be absolute: CMake looks it up relative to the build directory and
# fails with "Could not find toolchain file" otherwise.
#
# The last line is mandatory: -mmcu is added only inside firmware/CMakeLists.txt, so CMake's
# compiler check fails without CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY.
#
# Result on Ubuntu 22.04 with avr-gcc 5.4.0, Release: 3050 bytes of flash (37.2 % of the
# ATmega8), 232 bytes of RAM (22.7 %).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)