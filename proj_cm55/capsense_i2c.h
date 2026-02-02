#ifndef CAPSENSE_I2C_H
#define CAPSENSE_I2C_H

#include <stdbool.h>
#include <stdint.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "display_i2c_config.h"

typedef enum
{
    CAPSENSE_EVENT_NONE = 0,
    CAPSENSE_EVENT_BTN0,
    CAPSENSE_EVENT_BTN1,
    CAPSENSE_EVENT_SLIDER,
} capsense_event_type_t;

typedef struct
{
    capsense_event_type_t type;
    uint8_t button_status;
    uint8_t slider;
} capsense_event_t;

void capsense_i2c_init(cy_stc_scb_i2c_context_t *context);
bool capsense_i2c_poll(capsense_event_t *event);

#endif /* CAPSENSE_I2C_H */
