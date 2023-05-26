/*

MIT License

Copyright (c) 2022 Oliver Schmidt (https://a2retro.de/)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#ifdef USE_PIO

#include "RingBuf.h"
#include "CoreBUS.h"

namespace Modem
{
extern RingBuffer c0rx;  // The queue that core 0 receives bytes on
extern RingBuffer c0tx;
};

extern const __attribute__((aligned(4))) uint8_t firmware[];

namespace CoreBUS
{
#include "bus.pio.h"

#define SSC_DATA        0x8
#define SSC_STATUS      0x9
#define SSC_COMMAND     0xA
#define SSC_CONTROL     0xB

volatile bool active;

void bus_init()
{
    for (uint gpio = gpio_addr; gpio < gpio_addr + size_addr; gpio++) {
        gpio_init(gpio);
        gpio_set_pulls(gpio, false, false);  // floating
    }

    for (uint gpio = gpio_data; gpio < gpio_data + size_data; gpio++) {
        pio_gpio_init(pio0, gpio);
        gpio_set_pulls(gpio, false, false);  // floating
    }

    gpio_init(gpio_enbl);
    gpio_set_pulls(gpio_enbl, false, false);  // floating

    uint offset;

    offset = pio_add_program(pio0, &enbl_program);
    enbl_program_init(offset);

    offset = pio_add_program(pio0, &write_program);
    write_program_init(offset);

    offset = pio_add_program(pio0, &read_program);
    read_program_init(offset);
}

typedef void (*port_function)();
uint8_t values[0x10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
void __time_critical_func(empty)(void) {;}
void __time_critical_func(ssc_data)(void) 
{
    int buffered = Modem::c0tx.available() ? 0b00011000 : 0b00010000;
    if(buffered == 0b00011000) // was there something?
    {
        Modem::c0tx.advance(); // it was read so move on and see if there's something new
        buffered = Modem::c0tx.available() ? 0b00011000 : 0b00010000;
        values[SSC_DATA] = Modem::c0tx.get(); // load new char (or garbage)
    }
    values[SSC_STATUS] = buffered; // set status as likely read next
}

void __time_critical_func(ssc_status)(void) 
{
    // see if a character showed up
    int buffered = Modem::c0tx.available() ? 0b00011000 : 0b00010000;
    values[SSC_STATUS] = buffered; // set status to indicate availability
    // If there is a new char, update values for data since that's probably read next
    values[SSC_DATA] = Modem::c0tx.get(); 
}

void __time_critical_func(bus_interface)(void) 
{
    active = false;
    port_function portfunc[0x10] = {
        &empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
        &ssc_data, &ssc_status,
        &empty, &empty, &empty, &empty, &empty, &empty
    };

    while (true) {
        uint32_t enbl = pio_sm_get_blocking(pio0, sm_enbl);
        uint32_t addr = enbl & 0x0FFF;
        uint32_t io   = enbl & 0x0F00;  // IOSTRB or IOSEL
        uint32_t strb = enbl & 0x0800;  // IOSTRB
        uint32_t read = enbl & 0x1000;  // R/W

        if (read) {
            if (!io) {  // DEVSEL
                int port = addr & 0x0f;
                pio_sm_put(pio0, sm_read, values[port]);
                portfunc[port]();
            } else {
                if (!strb || active) {
                    pio_sm_put(pio0, sm_read, firmware[addr]);
                }
            }
        } else {
            uint32_t data = pio_sm_get_blocking(pio0, sm_write);
            if (!io)   // DEVSEL
            {
                switch (addr & 0xF)
                {
                    case SSC_DATA:
                        Modem::c0rx.Write(data);
                        break;
                }
            }
        }

        if (io && !strb) {
            active = true;
        } else if (addr == 0x0FFF) {
            active = false;
        }
    }
}

}; // CoreBUS

#endif // USE_PIO
