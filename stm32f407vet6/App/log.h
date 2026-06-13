#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include <stdarg.h>
#include "usart.h"

void log_info(const char* tag, const char *fmt, ...);

#endif