#ifdef USE_UART

#include "Queue.h"
#include "Serial.h"
#include "CoreUART.h"
#include <stdio.h>

namespace Modem
{
extern int  bauds[];
extern byte serialspeed;
// This is a command queue - this "commands" the SSC - switch baud, etc.
extern Queue c0cmd;
// These are the queues that core0 uses - core 0 sends on c0rx, core1 reads there
extern Queue c0rx;
extern Queue c0tx;
};

namespace CoreUART
{
Serial_ Serial;       // Connection over UART

void init()
{
    // Wat for the queueus to be set up on Core 0
    while(!(Modem::c0cmd.isInit() && Modem::c0rx.isInit() && Modem::c0tx.isInit())) {;}
    // Set up the serial
    Serial.setup(uart0, Modem::bauds[Modem::serialspeed], 8, 1, UART_PARITY_NONE);
}

void uart_interface(void)
{
    while(true)
    {
        if(Modem::c0cmd.available())
        {
            uint8_t chr = Modem::c0cmd.Read();
            switch(chr)
            {
                case 'B':
                {
                    chr = Modem::c0cmd.Read();
                    Serial.baud(Modem::bauds[chr]);
                }
                break;
                
                case 'S':
                    goto exit_loop;
                break;

                default:
                    while(Modem::c0cmd.available())
                        chr = Modem::c0cmd.Read();
            }
        }

        // Look at a multi-byte read here
        while(Serial.available())
        {
            uint8_t chr = Serial.Read();
            Modem::c0rx.Write(chr);
        }

        while(Modem::c0tx.available())
        {
            uint8_t chr = Modem::c0tx.Read();
            Serial.Write(chr);
        }
    }
exit_loop:;
}

}; // namespace CoreUART

#endif // USE_UART
