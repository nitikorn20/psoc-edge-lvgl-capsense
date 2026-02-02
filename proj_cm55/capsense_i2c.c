/*******************************************************************************
* File Name        : capsense_i2c.c
*
* Description      : CapSense read helper over I2C for CM55 (shared I2C bus).
*
*******************************************************************************/
#include "capsense_i2c.h"

#define INTERGER_ASCII_DIFFERENCE    (30U)
#define CAPSENSE_READ_BUFFER_SIZE    (3U)
#define I2C_SLAVE_ADDRESS            (0x8U)
#define I2C_SEND_RECEIVE_TIMEOUT_MS  (0U)
#define BUTTON0_INDEX                (0U)
#define BUTTON1_INDEX                (1U)
#define SLIDER_INDEX                 (2U)
#define I2C_READ_SIZE_MIN            (0U)
#define I2C_READ_SIZE_MAX            (1U)
#define CAPSENSE_BTN0_NOT_PRESSED    (0U)
#define CAPSENSE_BTN1_NOT_PRESSED    (1U)
#define CAPSENSE_BTN0_PRESSED        (1U)
#define CAPSENSE_BTN1_PRESSED        (2U)
#define SLIDER_POS_NOT_CHANGED       (0U)

static cy_stc_scb_i2c_context_t *capsense_i2c_context = NULL;
static uint32_t button0_status_prev = CAPSENSE_BTN0_NOT_PRESSED;
static uint32_t button1_status_prev = CAPSENSE_BTN1_NOT_PRESSED;
static uint16_t slider_pos_prev = SLIDER_POS_NOT_CHANGED;

void capsense_i2c_init(cy_stc_scb_i2c_context_t *context)
{
    capsense_i2c_context = context;
    button0_status_prev = CAPSENSE_BTN0_NOT_PRESSED;
    button1_status_prev = CAPSENSE_BTN1_NOT_PRESSED;
    slider_pos_prev = SLIDER_POS_NOT_CHANGED;
}

bool capsense_i2c_poll(capsense_event_t *event)
{
    uint8_t buffer[CAPSENSE_READ_BUFFER_SIZE];
    uint8_t size = CAPSENSE_READ_BUFFER_SIZE;
    uint8_t *data = &buffer[BUTTON0_INDEX];
    cy_en_scb_i2c_command_t ack = CY_SCB_I2C_ACK;
    cy_en_scb_i2c_status_t status;
    uint32_t button0_status;
    uint32_t button1_status;
    uint16_t slider_pos;

    if ((capsense_i2c_context == NULL) || (event == NULL))
    {
        return false;
    }

    event->type = CAPSENSE_EVENT_NONE;
    event->button_status = 0U;
    event->slider = 0U;

    status = (capsense_i2c_context->state == CY_SCB_I2C_IDLE) ?
        Cy_SCB_I2C_MasterSendStart(DISPLAY_I2C_CONTROLLER_HW,
                                   I2C_SLAVE_ADDRESS,
                                   CY_SCB_I2C_READ_XFER,
                                   I2C_SEND_RECEIVE_TIMEOUT_MS,
                                   capsense_i2c_context) :
        Cy_SCB_I2C_MasterSendReStart(DISPLAY_I2C_CONTROLLER_HW,
                                     I2C_SLAVE_ADDRESS,
                                     CY_SCB_I2C_READ_XFER,
                                     I2C_SEND_RECEIVE_TIMEOUT_MS,
                                     capsense_i2c_context);

    if (CY_SCB_I2C_SUCCESS == status)
    {
        while (size > I2C_READ_SIZE_MIN)
        {
            if (size == I2C_READ_SIZE_MAX)
            {
                ack = CY_SCB_I2C_NAK;
            }
            status = Cy_SCB_I2C_MasterReadByte(DISPLAY_I2C_CONTROLLER_HW,
                                               ack,
                                               (uint8_t *)data,
                                               I2C_SEND_RECEIVE_TIMEOUT_MS,
                                               capsense_i2c_context);
            if (status != CY_SCB_I2C_SUCCESS)
            {
                break;
            }
            ++data;
            --size;
        }
    }

    Cy_SCB_I2C_MasterSendStop(DISPLAY_I2C_CONTROLLER_HW,
                              I2C_SEND_RECEIVE_TIMEOUT_MS,
                              capsense_i2c_context);

    if (status != CY_SCB_I2C_SUCCESS)
    {
        return false;
    }

    buffer[BUTTON0_INDEX] -= INTERGER_ASCII_DIFFERENCE;
    buffer[BUTTON1_INDEX] -= INTERGER_ASCII_DIFFERENCE;

    button0_status = buffer[BUTTON0_INDEX];
    button1_status = buffer[BUTTON1_INDEX];
    slider_pos = buffer[SLIDER_INDEX];

    if ((CAPSENSE_BTN0_NOT_PRESSED != button0_status) &&
        (CAPSENSE_BTN0_NOT_PRESSED == button0_status_prev))
    {
        event->type = CAPSENSE_EVENT_BTN0;
        event->button_status = CAPSENSE_BTN0_PRESSED;
    }
    else if ((CAPSENSE_BTN1_NOT_PRESSED != button1_status) &&
             (CAPSENSE_BTN1_NOT_PRESSED == button1_status_prev))
    {
        event->type = CAPSENSE_EVENT_BTN1;
        event->button_status = CAPSENSE_BTN1_PRESSED;
    }
    else if ((SLIDER_POS_NOT_CHANGED != slider_pos) &&
             (slider_pos != slider_pos_prev))
    {
        event->type = CAPSENSE_EVENT_SLIDER;
        event->slider = (uint8_t)slider_pos;
    }

    button0_status_prev = button0_status;
    button1_status_prev = button1_status;
    slider_pos_prev = slider_pos;

    return (event->type != CAPSENSE_EVENT_NONE);
}

