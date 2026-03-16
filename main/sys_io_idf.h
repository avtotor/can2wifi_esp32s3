#ifndef SYS_IO_IDF_H_
#define SYS_IO_IDF_H_

#include <stdint.h>
#include <stdbool.h>

void setLED(uint8_t which, bool hi);
void toggleTXLED(void);
void toggleRXLED(void);
void toggleSD_LED(void);

#endif
