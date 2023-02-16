#ifndef __SYS_LOG_H
#define __SYS_LOG_H


#include <stdio.h>

#define sys_log_info(fmt, ...) printf("info: %s[%d]:"fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define sys_log_debug(fmt, ...) printf("info: %s[%d]:"fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define sys_log_erro(fmt, ...) printf("info: %s[%d]:"fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif
