/*
  membuffer.cpp - data blob
  Stefan Wessels, 2022
*/
#include "MemBuffer.h"

int MemBuffer::begin(uint max_size)
{
    index = written = 0;
    if (data && ownsData)
        free(data);
    ownsData = true;
    buffer_size = max_size;
    return (nullptr != (data = (uint8_t*)malloc(buffer_size)));
}

void MemBuffer::begin(uint8_t* pData, uint size)
{
    index = 0;
    written = size;
    if (data && ownsData)
        free(data);
    ownsData = false;
    data = pData;
}

int MemBuffer::available()
{
    return written - index;
}

int MemBuffer::peek()
{
    return data ? data[index] : -1;
}

size_t MemBuffer::Write(const uint8_t* buffer, size_t size)
{
    if (data)
    {
        if (buffer_size - size >= written)
        {
            memcpy(&data[written], buffer, size);
            written += size;
            return size;
        }
    }
    return -1;
}

size_t MemBuffer::Write(uint8_t c)
{
    if (data && written < buffer_size)
    {
        data[written++] = c;
        return 1;
    }
    return -1;
}

int MemBuffer::Read(uint8_t* buffer, size_t length)
{
    if (index + length <= written)
    {
        memcpy(buffer, &data[index], length);
        index += length;
        return length;
    }
    return -1;
}

int MemBuffer::Read()
{
    if (index < written)
        return data[index++];
    return -1;
}
