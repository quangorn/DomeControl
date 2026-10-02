cmake_minimum_required(VERSION 3.10)
project(domecontrol_sim C)

# Host build: the harness links libsimavr/libsimavrparts (apt: simavr, libsimavr-dev) and takes the
# firmware ELF built by ../CMakeLists.txt as an argument. The cross build stays in firmware/.
enable_testing()

find_path(SIMAVR_INCLUDE_DIR sim_avr.h PATHS /usr/include/simavr)
find_library(SIMAVR_LIBRARY simavr REQUIRED)
find_library(SIMAVR_PARTS_LIBRARY simavrparts REQUIRED)
find_package(Threads REQUIRED)

if(NOT SIMAVR_INCLUDE_DIR)
	message(FATAL_ERROR "sim_avr.h not found; install libsimavr-dev")
endif()

add_executable(harness harness.c)
target_include_directories(harness PRIVATE ${SIMAVR_INCLUDE_DIR})
target_link_libraries(harness PRIVATE
	${SIMAVR_LIBRARY}
	${SIMAVR_PARTS_LIBRARY}
	Threads::Threads)

# The L3 checks (firmware/test/sim/l3_check.py) run the real firmware ELF under this harness, so
# they are registered here rather than in firmware/test/CMakeLists.txt: they need libsimavr and an
# AVR cross build, and firmware/test/ must stay buildable on a host that has neither.
#
#   cmake -S firmware -B firmware/cmake-build-avr \
#         -DCMAKE_TOOLCHAIN_FILE=$PWD/firmware/toolchain-avr.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build firmware/cmake-build-avr --config Release
#   cmake -S firmware/test/sim -B firmware/cmake-build-sim && cmake --build firmware/cmake-build-sim
#   ctest --test-dir firmware/cmake-build-sim --output-on-failure
#
# CTest does not build anything, so the harness and the ELF must exist first; tools/check.sh does
# both in order. pyserial (python3-serial) is needed for the pty transport and is skipped over,
# not failed, when it is absent.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	set(FIRMWARE_ELF "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake-build-avr/DomeControl"
		CACHE FILEPATH "the firmware ELF the L3 checks run against")
	add_test(NAME l3
		COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/l3_check.py"
			"${FIRMWARE_ELF}" --harness $<TARGET_FILE:harness>)
	set_tests_properties(l3 PROPERTIES TIMEOUT 600)
else()
	message(WARNING "python3 not found: the l3 test is not registered")
endif()