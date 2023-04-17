/*
  Serial.h - Pico W serial port "driver"
  Stefan Wessels, 2022
*/
#ifndef queue_h
#define queue_h

#include "Stream.h"
#include "pico/util/queue.h"

// One of these classes to manage a Queue
class Queue : public Stream
{
private:
    bool    is_init = false;
    queue_t queue;

public:
    Queue() { ; }

    bool setup(size_t buffer_size);
    int begin();
    bool isInit() { return is_init;}
    int available();                                  // 0 if a Read will block, 1 if char available
    bool is_empty();
    bool is_full();
    int peek();                                       // -1 if buffer head == tail, else next char to be Read
    size_t Write(const uint8_t *buffer, size_t size); // copy as many bytes from buffer to txbuffer as will fit
    size_t Write(uint8_t c);                          // single character into txbuffer, block if buffer full
    int Read();                                       // get single char from rxbuf. block if buffer empty
};

#endif // queue_h