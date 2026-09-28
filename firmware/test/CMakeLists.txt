cmake_minimum_required(VERSION 3.10)
project(DomeControlTests C)

# Host build of the L1 firmware tests (test_plan.md §5). The production sources are
# compiled unchanged; firmware/test/fake is prepended to the include path so that
# <avr/io.h>, <avr/interrupt.h> and <util/*.h> resolve to the fake layer. The host
# toolchain is not a substitute for avr-gcc: L1 checks intent (which register bit was
# written), not the effect on the real peripheral.
#
#   cmake -S firmware/test -B firmware/cmake-build-test
#   cmake --build firmware/cmake-build-test
#   ctest --test-dir firmware/cmake-build-test --output-on-failure

enable_testing()

get_filename_component(FW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Flags mirror the AVR build where they matter for warnings and layout; -Wextra is
# added on the host. avr-gcc and gcc do not warn identically, so this is an extra
# net, not a replacement (test_plan.md §5.1).
add_compile_options(
	-std=gnu11
	-Wall
	-Wextra
	-Werror
	-DF_CPU=12000000
	-funsigned-char
	-fpack-struct
	-fshort-enums
	-Wno-unknown-pragmas
	-g
)

set(FAKE_SOURCES
	"${TEST_DIR}/fake/fake_avr.c"
	"${TEST_DIR}/fake/fake_stdlib.c"
)

# add_fw_test(<suite> [production unit relative to firmware/source ...])
function(add_fw_test suite)
	set(sources "${TEST_DIR}/test_${suite}.c" ${FAKE_SOURCES})
	foreach(unit IN LISTS ARGN)
		list(APPEND sources "${FW_ROOT}/source/${unit}")
	endforeach()
	add_executable(test_${suite} ${sources})
	target_include_directories(test_${suite} PRIVATE
		"${TEST_DIR}/fake"
		"${TEST_DIR}"
		"${FW_ROOT}"
	)
	add_test(NAME ${suite} COMMAND test_${suite})
endfunction()

add_fw_test(utils common/utils.c)
add_fw_test(encoder encoder/encoder.c settings/settings.c)
add_fw_test(limits limits/limits.c encoder/encoder.c settings/settings.c)
add_fw_test(buttons buttons/buttons.c)
add_fw_test(motor motor/motor.c encoder/encoder.c limits/limits.c settings/settings.c)
# test_usart.c includes usart.c itself to reach the module's static send state.
add_fw_test(usart)