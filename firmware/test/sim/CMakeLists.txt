cmake_minimum_required(VERSION 3.10)
project(domecontrol_sim C)

# Host build: the harness links libsimavr/libsimavrparts (apt: simavr, libsimavr-dev) and takes the
# firmware ELF built by ../CMakeLists.txt as an argument. The cross build stays in firmware/.
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