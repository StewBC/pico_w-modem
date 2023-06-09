/*
  modem.h - main entry point for Pico W modem "driver"
  Stefan Wessels, 2022
*/
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SDFile.h"
#include "WiFi.h"

#ifdef USE_UART
#include "CoreUART.h"
#else // USE_PIO
#include "CoreBUS.h"
#endif

#include <wolfssl/ssl.h>
#include <wolfssh/ssh.h>

#include "RingBuf.h"

namespace Modem
{
extern void pico_modem_main();
extern void defaultSettings();
extern void loadSettings();

#ifdef USE_UART
    extern int  bauds[];
    extern byte serialspeed;
    // This is a command queue - this "commands" the SSC - switch baud, etc.
    extern RingBuffer c0cmd;
#endif

// These are the queues that core0 uses - core 0 receives on c0rx and sends on c0tx
extern RingBuffer c0rx;  // The queue that core 0 receives bytes on
extern RingBuffer c0tx;  // The queue that core 0 transmits bytes on

// True when the SD card has been initialised
extern bool sd_init_driver;

};

void defaultSettings()
{
    Modem::defaultSettings();
}

void loadSettings()
{
    Modem::loadSettings();
}

static void main_task(__unused void *params)
{
    if (cyw43_arch_init())
    {
        printf("Failed to initialise Pico W\n");
    }
    else
    {
        // wolfSSH_Debugging_ON();
        WiFi.init();
        if(wolfSSH_Init() == WS_SUCCESS)
        {
            Modem::pico_modem_main();
            wolfSSH_Cleanup();
        }
        else
        {
            printf("Failed to initialise WolfSSH\n");
        }
    }
    vTaskDelete(NULL);
}

void core1_main()
{
#ifdef USE_UART
    CoreUART::init();
    CoreUART::uart_interface();
#else	
    CoreBUS::bus_init();
    CoreBUS::bus_interface();
#endif
}

#ifdef USE_PIO
// static void fifo_task(__unused void *params)
// {
//     while(true)
//     {
//         if (multicore_fifo_wready() && Modem::c0tx.available()) 
//         {
//             multicore_fifo_push_blocking(Modem::c0tx.Read());
//         }
//     }
// }
#endif

int main(void)
{
    TaskHandle_t task;

    stdio_init_all();

    defaultSettings();
    if(sd_init_driver())
    {
        Modem::sd_init_driver = true;
        loadSettings();
    }

    multicore_reset_core1();

    multicore_launch_core1(core1_main);
// #ifdef USE_PIO
// Add this and fifo_task back in to use the inter-core fifo
//     xTaskCreate(fifo_task, "FifoThread", configMINIMAL_STACK_SIZE, NULL, 1, &task);
// #endif
    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, 1, &task);
    vTaskStartScheduler();
}
