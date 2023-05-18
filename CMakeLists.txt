# As of V3.17 the FetchContent_Declare supports GIT_SUBMODULES_RECURSE which makes a massive 
# difference on the pico-sdk (87M vs >4G to download)
cmake_minimum_required(VERSION 3.17)

# Set up a path to shared config files
set(PICOW_CONFIG_PATH ${CMAKE_CURRENT_LIST_DIR}/config)

# Locate the git executable and prepare to bring in "libraries" from GitHub
find_package(Git)
find_program(PATCH NAMES patch HINTS ${GIT_DIR} PATH_SUFFIXES usr/bin)
if(NOT PATCH)
  message(FATAL_ERROR "Did not find patch command")
endif()
include(FetchContent)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/CMakeModules")
find_package("ProjectUtilities")

# pico-sdk - get and init submodules
GetGithubCode(pico_sdk PICO_SDK_PATH https://github.com/raspberrypi/pico-sdk.git 1.5.0)
InitSubmodules(${PICO_SDK_PATH})

# FreeRTOS-Kernel - get only
GetGithubCode(FreeRTOS-Kernel FREERTOS_KERNEL_PATH https://github.com/FreeRTOS/FreeRTOS-Kernel.git V10.5.1)

# WolfSSL - get from git and build library
GetGithubCode(wolfssl WOLFSSL_PATH https://github.com/wolfSSL/wolfssl.git v5.5.4-stable)
set(WOLFSSL_ARM_PATH ${WOLFSSL_PATH}/IDE/GCC-ARM)
set(WOLFSSL_ARM_BUILD_PATH ${WOLFSSL_ARM_PATH}/Build)
set(WOLFSSL_ARM_LIB_PATH ${WOLFSSL_ARM_BUILD_PATH}/libwolfssl.a)
# Save the original Makefile.common as a sentinal and install a new Makefile.common
CopyConditional(
    ${WOLFSSL_ARM_PATH}/Makefile.common.original
    ${CMAKE_CURRENT_LIST_DIR}/patches/wolfssl-Makefile.common
    ${WOLFSSL_ARM_PATH}/Makefile.common
)
# Add the make for libwolfssl.a which is an external Makefile
# Set options in ${WOLFSSL_ARM_PATH}/Makefile.common (such as debug, etc.)
add_custom_command(OUTPUT ${WOLFSSL_ARM_LIB_PATH}
                COMMAND make -j WolfSSLStaticLib
                WORKING_DIRECTORY ${WOLFSSL_ARM_PATH}
)
add_custom_target(libwolfssl DEPENDS "${WOLFSSL_ARM_LIB_PATH}")

# WolfSSH  - get from git and build library
GetGithubCode(wolfssh WOLFSSH_PATH https://github.com/wolfSSL/wolfssh.git v1.4.12-stable)
set(WOLFSSH_SRC_PATH ${WOLFSSH_PATH}/src)
set(WOLFSSH_ARM_PATH ${WOLFSSH_PATH}/ide/GCC-ARM)
set(WOLFSSH_ARM_BUILD_PATH ${WOLFSSH_ARM_PATH}/Build)
set(WOLFSSH_ARM_LIB_PATH ${WOLFSSH_ARM_BUILD_PATH}/libwolfssh.a)
# Add a directory for the GCC-ARM make
file(MAKE_DIRECTORY ${WOLFSSH_ARM_PATH})
# Install the custom GCC-ARM Makefile to make wolfssh for ARM-GCC
CopyConditional(
    ${WOLFSSH_ARM_PATH}/Makefile.sentinal
    ${CMAKE_CURRENT_LIST_DIR}/patches/wolfssh-Makefile
    ${WOLFSSH_ARM_PATH}/Makefile
)
CopyConditional(
    ${WOLFSSH_SRC_PATH}/internal.c.sentinal
    ${CMAKE_CURRENT_LIST_DIR}/patches/wolfssh-internal.c.v1.4.12-stable
    ${WOLFSSH_SRC_PATH}/internal.c
)
CopyConditional(
    ${WOLFSSH_SRC_PATH}/ssh.c.sentinal
    ${CMAKE_CURRENT_LIST_DIR}/patches/wolfssh-ssh.c.v1.4.12-stable
    ${WOLFSSH_SRC_PATH}/ssh.c
)
# Build the library
add_custom_command(OUTPUT ${WOLFSSH_ARM_LIB_PATH}
                COMMAND make -j
                WORKING_DIRECTORY ${WOLFSSH_ARM_PATH}
)
add_custom_target(libwolfssh DEPENDS "${WOLFSSH_ARM_LIB_PATH}")

# libsmb2
GetGithubCode(libsmb2 LIBSMB2_PATH https://github.com/sahlberg/libsmb2.git master)
# Save the original libsmb2 CMakeLists.txt as a sentinal file and install a new file
CopyConditional(
    ${LIBSMB2_PATH}/CMakeLists.txt.original
    ${CMAKE_CURRENT_LIST_DIR}/patches/libsmb2-CMakeLists.txt
    ${LIBSMB2_PATH}/CMakeLists.txt
)

GetGithubCode(no-os-fatfs-sd-spi-rpi-pico NO_OS_FATFS_SD_SPI https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico.git v1.0.8)
set(FATFS_SD_PATH ${NO_OS_FATFS_SD_SPI}/FatFs_SPI)

# Name the project and prepare it to be a pico_w project, using FreeRTOS and lwip
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(PICO_BOARD pico_w)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
project(${PROJECT_NAME} C CXX ASM)
pico_sdk_init()

# Add a couple of defenitions that all projects/libraries can/should use
add_definitions(-DPICO_PLATFORM=${PICO_PLATFORM})
add_definitions(-DHAVE_CONFIG_H)

# Build libsmb2 as a library
add_subdirectory(${LIBSMB2_PATH})

# # Build no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI
add_subdirectory(${FATFS_SD_PATH})

# Add the application to build
add_subdirectory(modem)

