#include "log.h"

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

void log_info(const char *tag, const char *fmt, ...)
{
    printf("(%lu) %s: ", HAL_GetTick()/10, tag);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\r\n");
}