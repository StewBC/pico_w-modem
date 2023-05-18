#include "Stream.h"

/*
 * Circular buffer class for uart rx/tx
 * The class has no over/under-flow protection
 */
class RingBuffer : public Stream
{
private:
    static const int size = 512+11; // 1 VSDrive block plus header bytes

public:
    volatile size_t head = 0;
    volatile size_t tail = 0;
    unsigned char buffer[size];

    inline bool is_empty() { return head == tail;}
    inline bool is_full() { size_t next_head = head + 1; if(next_head == size) {next_head = 0;} return next_head == tail;}
    inline void put(unsigned char byte) { buffer[head] = byte; if(++head == size) head = 0;}
    inline int  get() { return buffer[tail];}
    inline void advance() { if(++tail == size) tail = 0;}

    inline virtual int available() { return head != tail;}
    inline virtual int Read() { int c = -1; if(available()) {c= get(); advance();} return c;}
    inline virtual int peek() { return is_empty() ? -1 : buffer[tail];}
    inline virtual size_t Write(uint8_t c) { while(is_full()){;} put(c); return 1;}
    inline virtual size_t Write(const uint8_t *buffer, size_t size) { size_t s = size; while(s--) {while(is_full()){;} put(*buffer++);} return size;}
};
