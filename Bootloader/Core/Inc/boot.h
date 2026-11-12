#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

#define APP_START_ADDR    0x08008000U
#define APP_END_ADDR      0x08080000U

uint8_t Boot_IsApplicationValid(void);
void Boot_JumpToApplication(void);

#endif
