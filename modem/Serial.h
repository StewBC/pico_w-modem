/*
  Serial.h - Pico W serial port "driver"
  Stefan Wessels, 2022
*/
#ifndef serial_h
#define serial_h

#include "Stream.h"
// #include "Queue.h"
#include <pico/util/queue.h>
#include <hardware/uart.h>

#include <stdio.h>

/*
 * Circular buffer class for uart rx/tx
 * The class has no over/under-flow protection
 */
// class RingBuffer
// {
// private:
//     static const int size = 256;

// public:
//     volatile int head = 0;
//     volatile int tail = 0;
//     unsigned char buffer[size];

//     bool is_empty() { return head == tail; }
//     bool is_full() { return ((head+1)%size) == tail; }
//     void put(unsigned char byte) {buffer[head] = byte; head = ++head % size; }
//     unsigned char get() { unsigned char c = buffer[tail]; tail = ++tail % size; return c; }
// };

/*
 * One of these classes to manage a UART
 */
class Serial_ : public Stream
{
private:
    int uartIRQ = 0;
    uart_inst_t *uartInstance = NULL;
    // RingBuffer *rx = nullptr, *tx = nullptr;
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