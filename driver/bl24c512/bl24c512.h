#ifndef __BL24C512_H__
#define __BL24C512_H__

#include <stdbool.h>
#include <stdint.h>

void bl24c512_init(void);
bool bl24c512_write(uint16_t address, uint8_t data[], uint32_t length);
bool bl24c512_read(uint16_t address, uint8_t data[], uint32_t length);

#endif /* __BL24C512_H__ */
