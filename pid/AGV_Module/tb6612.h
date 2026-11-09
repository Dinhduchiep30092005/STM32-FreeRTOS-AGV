#ifndef TB6612_H
#define TB6612_H

#include <stdint.h>

void tb6612_init(void);
void tb6612_set_motor(int16_t pwmL, int16_t pwmR);
void tb6612_stop(void);

#endif // TB6612_H