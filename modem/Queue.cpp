/*
  Serial.cpp - Pico W serial port "driver"
  Stefan Wessels, 2022
*/
#include <pico/stdlib.h>
#include <stdio.h>
#include "Queue.h"

bool Queue::setup(size_t buffer_size)
{
    queue_init(&queue, 1, buffer_size);
    return is_init = true;
}

int Queue::begin()
{
    return 1;
}

int Queue::available()
{
    return queue_get_level(&queue);
}

//
bool Queue::is_empty()
{
    return queue_is_empty(&queue);
}

//
bool Queue::is_full()
{
    return queue_is_full(&queue);
}

int Queue::peek()
{
    uint8_t data;
    if(queue_try_peek(&queue, &data))
        return -1;
    return data;
}

// Copy as much from buffer to txBuffer as can fit
// Sets interrupt to start sending
size_t Queue::Write(const uint8_t *buffer, size_t size)
{
    int i = 0;
    while(i < size)
    {
        queue_add_blocking(&queue, &buffer[i++]);
    }
    return i;
}

// Write a single character to head in txBuffer and set interrupt to start sending
// Will block of txBuffer is full (head + 1 == tail)
size_t Queue::Write(uint8_t c)
{
    queue_add_blocking(&queue, &c);
    return c;
}

// Read a single character from rxBuffer at tail
// Will block if rxBuffer is empty (head == tail)
int Queue::Read()
{
    uint8_t data;
    queue_remove_blocking(&queue, &data);
    return data;
}
