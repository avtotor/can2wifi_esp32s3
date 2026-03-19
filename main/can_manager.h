#ifndef CAN_MANAGER_IDF_H_
#define CAN_MANAGER_IDF_H_

#include "config_idf.h"
#include "esp32_can_idf.h"

void can_manager_setup(void);
void can_manager_loop(void);
void can_manager_send_frame(int bus_idx, CAN_FRAME *frame);
void can_manager_display_frame(CAN_FRAME *frame, int whichBus);

#endif
