/*
  Serial.h - Pico W serial port "driver"
  Stefan Wessels, 2022
*/
#ifndef serial_h
#define serial_h

#include "Stream.h"
#include <pico/util/queue.h>
#include <hardware/uart.h>

#include <stdio.h>

/*
 * One of these classes to manage a UART
 */
class Serial_ : public Stream
{
private:
    int uartIRQ = 0;
    uart_inst_t *uartInstance = NULL;
    queue_t   *rx = nullptr, *tx = nullptr;

public:
    Serial_() { ; }

    bool setup(uart_inst_t *instance,                 // Call once to make RX and TX buffers and set basic parameters
               uint baudrate,
               uint data_bits,
               uint stop_bits,
               uart_parity_t parity);

    int baud(uint baudrate);
    int available();                                  // 0 if a Read will block, 1 if char available
    int peek();                                       // -1 if buffer head == tail, else next char to be Read
    size_t Write(const uint8_t *buffer, size_t size); // copy as many bytes from buffer to txbuffer as will fit
    size_t Write(uint8_t c) {return Write(&c,1);}     // single character into txbuffer, block if buffer full
    int Read();                                       // get single char from rxbuf. block if buffer empty
};

#endif // serial_h