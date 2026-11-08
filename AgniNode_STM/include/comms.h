#ifndef COMMS_H
#define COMMS_H

#include <stdint.h>

void comms_init(void);
void comms_poll(uint32_t now_ms);
void comms_periodic(uint32_t now_ms);

#endif /* COMMS_H */
