#include "log.h"
#include "bootCount.h"

void app_main()
{
    log_info("sys", "OV7670 Demo");
    log_info("sys", "Boot Count: %d", BOOTCOUNT_Get());
    
    while (1){}
}