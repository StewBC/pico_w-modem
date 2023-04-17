/*
  membuffer.h - data blob
  Stefan Wessels, 2022
*/
#ifndef _membuffer_h
#define _membuffer_h

#include "Stream.h"

class MemBuffer : public Stream
{
private:
    uint8_t *data = nullptr;
    uint index = 0, written = 0, buffer_size = 0;
    bool ownsData = false;

public:
    MemBuffer() { ; }

    int begin(uint max_size);
    void begin(uint8_t *pData, uint size);
    int available();                                  // 0 if a Read will block, 1 if char available
    int peek();                                       // -1 if buffer head == tail, else next char to be Read
    size_t Write(const uint8_t *buffer, size_t size); // copy as many bytes from buffer to txbuffer as will fit
    size_t Write(uint8_t c);                          // single character into txbuffer, block if buffer full
    int Read(uint8_t *buffer, size_t length);         //
    int Read();                                       // get single char from rxbuf. block if buffer empty
    int GetWrittenLength() { return written; }
    const uint8_t *GetData() { return data; }
};

#endif //_membuffer_h