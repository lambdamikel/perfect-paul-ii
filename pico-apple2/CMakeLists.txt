# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.13)

# Reuse the import helpers already shipped with DECtalkMini's Pico target.
include(${CMAKE_CURRENT_LIST_DIR}/../pico/pico_sdk_import.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../pico/pico_extras_import.cmake)

project(dectalk_apple2 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

option(DECTALK_AUDIO_I2S "Use an I2S DAC/amplifier (recommended)" ON)
option(DECTALK_SPEAK_STARTUP_BANNER "Speak a ready message after boot" ON)
set(DECTALK_PWM_GPIO 28 CACHE STRING "GPIO for PWM audio when I2S is disabled")

file(GLOB DECTALK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../src/*.c"
)
list(REMOVE_ITEM DECTALK_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../../src/main.c"
)

add_executable(dectalk_apple2
    ${DECTALK_SOURCES}
    main.c
)

pico_generate_pio_header(dectalk_apple2
    ${CMAKE_CURRENT_LIST_DIR}/apple2_slot_rx.pio
)

target_include_directories(dectalk_apple2 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../include
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(dectalk_apple2 PRIVATE
    _REENTRANT=1
    NOMME=1
    LTSSIM=1
    TTSSIM=1
    ANSI=1
    BLD_DECTALK_DLL=1
    ENGLISH=1
    ENGLISH_US=1
    ACCESS32=1
    TYPING_MODE=1
    ACNA=1
    DISABLE_AUDIO=1
    SINGLE_THREADED=1
    NO_FILESYSTEM=1
    __unix__=1
    AUDIO_HALF_FREQ=1
    HANG_FIXES=1
    DECTALK_SPEAK_STARTUP_BANNER=$<BOOL:${DECTALK_SPEAK_STARTUP_BANNER}>
)

target_link_libraries(dectalk_apple2 PRIVATE
    pico_stdlib
    pico_multicore
    pico_util
    hardware_pio
)

# Every target that produces audio goes through this, so the self test can never
# again be built against a different backend than the firmware it is meant to
# vouch for.
function(dectalk_configure_audio target)
    if(DECTALK_AUDIO_I2S)
        target_compile_definitions(${target} PRIVATE
            DECTALK_AUDIO_I2S=1
            PICO_AUDIO_I2S_MONO_OUTPUT=1
            PICO_AUDIO_I2S_MONO_INPUT=1
            PICO_AUDIO_I2S_CLOCK_PIN_BASE=20
            PICO_AUDIO_I2S_DATA_PIN=22
        )
        target_link_libraries(${target} PRIVATE pico_audio_i2s)
    else()
        target_compile_definitions(${target} PRIVATE
            DECTALK_AUDIO_I2S=0
            DECTALK_PWM_GPIO=${DECTALK_PWM_GPIO}
        )
        target_link_libraries(${target} PRIVATE
            pico_audio_pwm
            hardware_pwm
        )
    endif()
endfunction()

dectalk_configure_audio(dectalk_apple2)

pico_enable_stdio_usb(dectalk_apple2 1)
pico_enable_stdio_uart(dectalk_apple2 0)
pico_add_extra_outputs(dectalk_apple2)

# Standalone bring-up test: speaks in a loop, needs no Apple II. It follows
# DECTALK_AUDIO_I2S like the firmware does, so the i2s preset yields an I2S self
# test and the pwm preset a PWM one.
option(DECTALK_BUILD_SELFTEST "Build the standalone speech self test" ON)

if(DECTALK_BUILD_SELFTEST)
    add_executable(dectalk_selftest
        ${DECTALK_SOURCES}
        speech_selftest.c
    )

    target_include_directories(dectalk_selftest PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../include
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_compile_definitions(dectalk_selftest PRIVATE
        _REENTRANT=1
        NOMME=1
        LTSSIM=1
        TTSSIM=1
        ANSI=1
        BLD_DECTALK_DLL=1
        ENGLISH=1
        ENGLISH_US=1
        ACCESS32=1
        TYPING_MODE=1
        ACNA=1
        DISABLE_AUDIO=1
        SINGLE_THREADED=1
        NO_FILESYSTEM=1
        __unix__=1
        AUDIO_HALF_FREQ=1
        HANG_FIXES=1
    )

    target_link_libraries(dectalk_selftest PRIVATE
        pico_stdlib
    )

    dectalk_configure_audio(dectalk_selftest)

    pico_enable_stdio_usb(dectalk_selftest 1)
    pico_enable_stdio_uart(dectalk_selftest 0)
    pico_add_extra_outputs(dectalk_selftest)
endif()
