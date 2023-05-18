cmake_minimum_required(VERSION 3.12)

set(PROJECT_NAME modem)
project(${PROJECT_NAME} C CXX ASM)

# Set to ON for the UART RS232 build and OFF for the PIO build
set(USING_UART ON)

# Add some basic defenitions
add_definitions(-DWOLFSSL_USER_SETTINGS)
if(USING_UART)
        add_definitions(-DUSE_UART)
        remove_definitions(-DUSE_PIO)
else()
        add_definitions(-DUSE_PIO)
        remove_definitions(-DUSE_UART)
endif()

# Add this up here or the library isn't found
add_link_options(-L${WOLFSSL_ARM_BUILD_PATH} -L${WOLFSSH_ARM_BUILD_PATH})

# Application, including in the FreeRTOS-Kernel
add_executable(${PROJECT_NAME}
        Client.h
        CoreBUS.cpp
        CoreBUS.h
        CoreUART.cpp        
        CoreUART.h
        hw_config.c
        incbin.s
        IPAddress.cpp
        IPAddress.h
        main.cpp
        MemBuffer.cpp
        MemBuffer.h
        Modem.cpp
        NTPClient.cpp
        NTPClient.h
        Print.cpp
        Print.h
        Printable.h
        RingBuf.h
        SDFile.cpp
        SDFile.h
        Serial.cpp
        Serial.h
        Stream.cpp
        Stream.h
        wcList.h
        WiFi.cpp
        WiFi.h
        WiFiClient.cpp
        WiFiClient.h
        WString.cpp
        WString.h
)

# Force the build of libwolfss?.a through an external Makefile
add_dependencies(${PROJECT_NAME} libwolfssl libwolfssh)

# Build the app
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}
        ${FREERTOS_KERNEL_PATH}/include
        ${WOLFSSL_PATH}
        ${WOLFSSH_PATH}
        ${LIBSMB2_PATH}/include
        ${PICOW_CONFIG_PATH}
)

# Select where debug prints go
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_semihosting(${PROJECT_NAME} 0)

if(NOT USING_UART)
        # Generate bus.pio.h if this is a PIO buile
        pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/bus.pio)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
)

target_link_libraries(${PROJECT_NAME}
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel
        pico_stdlib
        pico_multicore
        wolfssh                 # Order matters ssh before ssl
        wolfssl
        libsmb2
        FatFs_SPI
        FreeRTOS-Kernel-Heap4
)

pico_add_extra_outputs(${PROJECT_NAME})
