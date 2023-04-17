/*
  Serial.cpp - Pico W serial port "driver"
  Stefan Wessels, 2022
*/
#ifdef USE_UART

#include "Serial.h"
#include <pico/stdlib.h>
#include <stdio.h>

// RP2040 has 2 uarts, 0 and 1.  These are the rx/tx buffers for the uarts
queue_t uart0Rx, uart0Tx, uart1Rx, uart1Tx;

/*
 * All Read and writes happen in a uart IRQ, and is handeled here for uart0
 */
void uart0_irq()
{
    static uint8_t data;
    if(uart_is_readable(uart0) && !queue_is_full(&uart0Rx))
    {
        data = uart_getc(uart0);
        queue_try_add(&uart0Rx, &data);
    }

    if(!queue_is_empty(&uart0Tx) && uart_is_writable(uart0))
    {
        queue_try_remove(&uart0Tx, &data);
        uart_putc_raw(uart0, data);
    }

    if(queue_is_empty(&uart0Tx))
    {
        uart_set_irq_enables(uart0, true, false);
    }
}

/*
 * All Read and writes happen in a uart IRQ, and is handeled here for uart1
 */
void uart1_irq()
{
    static uint8_t data;
    while(uart_is_readable(uart1) && !queue_is_full(&uart1Rx))
    {
        data = uart_getc(uart1);
        queue_try_add(&uart1Rx, &data);
    }

    while(!queue_is_empty(&uart1Tx) && uart_is_writable(uart1))
    {
        queue_try_remove(&uart1Tx, &data);
        uart_putc_raw(uart1, data);
    }

    if(queue_is_empty(&uart1Tx))
    {
        uart_set_irq_enables(uart1, true, false);
    }
}

/*
 * One time init of a uart (0|1).
 */
bool Serial_::setup(uart_inst_t *instance, uint baudrate, uint data_bits, uint stop_bits, uart_parity_t parity)
{
    uint8_t tx_pin = 0, rx_pin = 1;
    if (uartInstance != NULL)
        return false;

    uartInstance = instance;

    if (uartInstance == uart0)
    {
        rx = &uart0Rx;
        tx = &uart0Tx;
        uartIRQ = UART0_IRQ;
    }
    else
    {
        tx_pin = 4;
        rx_pin = 5;
        rx = &uart1Rx;
        tx = &uart1Tx;
        uartIRQ = UART1_IRQ;
    }
    queue_init(rx, 1, 256);
    queue_init(tx, 1, 256);

    uart_init(uartInstance, baudrate);
    uart_set_hw_flow(uartInstance, true, true);
    uart_set_format(uartInstance, data_bits, stop_bits, parity);
    uart_set_fifo_enabled(uartInstance, true);
    uart_set_translate_crlf(uartInstance, false);
    irq_set_exclusive_handler(uartIRQ, (uartInstance == uart0 ? uart0_irq : uart1_irq));
    irq_set_enabled(uartIRQ, true);
    uart_set_irq_enables(uartInstance, true, false);

    gpio_set_function(tx_pin, GPIO_FUNC_UART); // TX pin
    gpio_set_function(rx_pin, GPIO_FUNC_UART); // RX Pin

    return true;
}

/*
 * 
 */
int Serial_::baud(uint baudrate)
{
    return (int)uart_set_baudrate(uartInstance, baudrate);
}

/*
 * Return TRUE if the rx queue is NOT empty, so a read will not block
 */
int Serial_::available()
{
    return !queue_is_empty(rx);
}

/*
 * Returns the character that will be read next by a read
 */
int Serial_::peek()
{
    uint8_t data;
    queue_peek_blocking(rx, &data);
    return data;
}

/*
 * Will block/send till entire buffer is sent
 * Sets interrupt to start sending
 */
size_t Serial_::Write(const uint8_t *buffer, size_t size)
{
    size_t count = 0;
    bool irq_set = false;

    // Loop till all bytes sent
    while (count < size)
    {
        queue_add_blocking(tx, &buffer[count]);
        count++;
        if(!irq_set)
        {
            // Sstart uart send pump
            uart_set_irq_enables(uartInstance, true, true);
            irq_set_pending(uartIRQ);
            irq_set = true;
        }
    }
    return count;
}

/*
 * Read a single character from rx
 * Will block if rxBuffer is empty
 */
int Serial_::Read()
{
    uint8_t data;
    queue_remove_blocking(rx, &data);
    return data;
}

#endif
