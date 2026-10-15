# Perfect Paul ][ -- SP0256-AL2 firmware.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A SEPARATE project from the DECtalk firmware in the parent directory, and
# deliberately so. The SP0256 core is GPL-3 and the DECtalk core carries
# proprietary notices from DEC, Fonix and Force; one binary holding both could
# not be distributed by anyone, because GPL-3 requires complete corresponding
# source for the whole work. Two executables, each distributable on its own
# terms, is the only arrangement that works -- and this one is the first
# Perfect Paul ][ firmware whose UF2 can actually be shipped.
#
# It needs no DECtalkMini checkout at all. Everything it compiles is here, plus
# the MIT PIO program it shares with the DECtalk firmware:
#
#   export PICO_SDK_PATH=/path/to/pico-sdk
#   export PICO_EXTRAS_PATH=/path/to/pico-extras
#   cmake --preset pico-i2s-release
#   cmake --build --preset pico-i2s-release

cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico)
endif()

# These must be set BEFORE pico_sdk_init(), or they never reach the SDK's own
# sources: add_compile_definitions() is a directory property and only applies to
# targets created after it, and pico_sdk_init() creates them all.
#
# 4 KB of stack per core instead of the SDK's 2 KB. The letter-to-sound engine
# keeps its working buffers in BSS precisely so it does NOT need this -- but a
# 2 KB stack is tight enough that one printf inside one more call level is a
# silent overflow into the other core's stack, and this board has 264 KB of RAM
# to spare. Belt as well as braces; see the comment in nrl.c.
#
# printf must never stall the audio. With a USB host attached but nothing
# reading the CDC, stdio_usb blocks for up to PICO_STDIO_USB_STDOUT_TIMEOUT_US
# -- half a second, by default, PER CALL -- which on a card whose job is to keep
# an audio DMA fed is a fault, not a diagnostic. Zero drops the text instead.
add_compile_definitions(
    PICO_STACK_SIZE=4096
    PICO_CORE1_STACK_SIZE=4096
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=0
)

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/pico_extras_import.cmake)

project(perfectpaul2_sp0256 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()



option(SP0256_AUDIO_I2S "Use an I2S DAC/amplifier (recommended)" ON)
option(SP0256_SPEAK_STARTUP_BANNER "Speak a ready message after boot" ON)
option(SP0256_DEFAULT_TEXT_MODE "Start in English text mode rather than allophone mode" ON)
set(SP0256_PWM_GPIO 28 CACHE STRING "GPIO for PWM audio when I2S is disabled")

add_executable(perfectpaul2_sp0256
    main_sp0256.c
    pp2_sp0256.c
    pp2_audio.c
    nrl.c
    core/sp0256_speak.c
    core/sp0256_rom.cpp
    vendor/sp0256.c
    vendor/sp0256_al2.cpp
)

# Shared verbatim with the DECtalk firmware, which is where it is maintained.
# MIT, and MIT combines into a GPL-3 work without friction.
pico_generate_pio_header(perfectpaul2_sp0256
    ${CMAKE_CURRENT_LIST_DIR}/../apple2_slot_rx.pio
)

target_include_directories(perfectpaul2_sp0256 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/core
    ${CMAKE_CURRENT_LIST_DIR}/vendor
)

target_compile_definitions(perfectpaul2_sp0256 PRIVATE
    # The vendor core's interactive single-step debugger holds a 1 KB buffer,
    # which costs sp0256_micro() a 1112-byte stack frame even though the code
    # is unreachable here -- more than half of a Pico core's 2 KB stack.
    # Verified not to change a single rendered sample.
    SP0256_NO_STDIN_DEBUG=1
    SP0256_SPEAK_STARTUP_BANNER=$<BOOL:${SP0256_SPEAK_STARTUP_BANNER}>
    PP2_SP_DEFAULT_MODE=$<IF:$<BOOL:${SP0256_DEFAULT_TEXT_MODE}>,PP2_SP_TEXT,PP2_SP_ALLOPHONE>
)

target_link_libraries(perfectpaul2_sp0256 PRIVATE
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_clocks
)

# Every target that produces audio goes through this, so the self test can
# never be built against a different backend than the firmware it vouches for.
# They also compile the same pp2_audio.c, so it is the same code and not merely
# the same library.
function(pp2_configure_audio target)
    if(SP0256_AUDIO_I2S)
        target_compile_definitions(${target} PRIVATE
            SP0256_AUDIO_I2S=1
            PICO_AUDIO_I2S_MONO_OUTPUT=1
            PICO_AUDIO_I2S_MONO_INPUT=1
            PICO_AUDIO_I2S_CLOCK_PIN_BASE=20
            PICO_AUDIO_I2S_DATA_PIN=22
        )
        target_link_libraries(${target} PRIVATE pico_audio_i2s)
    else()
        target_compile_definitions(${target} PRIVATE
            SP0256_AUDIO_I2S=0
            SP0256_PWM_GPIO=${SP0256_PWM_GPIO}
        )
        target_link_libraries(${target} PRIVATE
            pico_audio_pwm
            hardware_pwm
        )
    endif()
endfunction()

pp2_configure_audio(perfectpaul2_sp0256)

pico_enable_stdio_usb(perfectpaul2_sp0256 1)
pico_enable_stdio_uart(perfectpaul2_sp0256 0)
pico_add_extra_outputs(perfectpaul2_sp0256)

# Standalone bring-up test and demo: speaks a script in a loop, needs no Apple
# II and no slot traffic. Single core, because there is no slot to service.
option(SP0256_BUILD_SELFTEST "Build the standalone self test" ON)
option(SP0256_SELFTEST_VERBOSE "Self test: trace the rule engine and the chip state" OFF)

if(SP0256_BUILD_SELFTEST)
    add_executable(perfectpaul2_sp0256_selftest
        sp0256_selftest.c
        pp2_sp0256.c
        pp2_audio.c
        nrl.c
        core/sp0256_speak.c
        core/sp0256_rom.cpp
        vendor/sp0256.c
        vendor/sp0256_al2.cpp
    )

    target_include_directories(perfectpaul2_sp0256_selftest PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/core
        ${CMAKE_CURRENT_LIST_DIR}/vendor
    )

    target_compile_definitions(perfectpaul2_sp0256_selftest PRIVATE
        SP0256_NO_STDIN_DEBUG=1
        SP0256_SELFTEST_VERBOSE=$<BOOL:${SP0256_SELFTEST_VERBOSE}>
    )

    target_link_libraries(perfectpaul2_sp0256_selftest PRIVATE
        pico_stdlib
        hardware_clocks
    )

    pp2_configure_audio(perfectpaul2_sp0256_selftest)

    pico_enable_stdio_usb(perfectpaul2_sp0256_selftest 1)
    pico_enable_stdio_uart(perfectpaul2_sp0256_selftest 0)
    pico_add_extra_outputs(perfectpaul2_sp0256_selftest)
endif()
