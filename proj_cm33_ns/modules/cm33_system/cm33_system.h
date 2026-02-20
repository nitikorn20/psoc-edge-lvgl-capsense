/*******************************************************************************
 * File Name        : cm33_system.h
 *
 * Description      : Function prototypes and definitions for the CM33 system
 *                    initialization (BSP, RTC, retarget-io, LPTimer, CM55).
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 * Version          : 1.0
 * Target           : PSoC Edge E84, CM33 (non-secure)
 *
 *******************************************************************************/

#ifndef CM33_SYSTEM_H_
#define CM33_SYSTEM_H_

#include <stdbool.h>

/*******************************************************************************
 * Public API
 *******************************************************************************/

/** Initializes BSP, RTC, retarget-io, LPTimer (tickless idle). Returns true on success. */
bool cm33_system_init(void);

/** Enables the CM55 core. Call after IPC pipe is started if boot order matters. */
void cm33_system_enable_cm55(void);

#endif /* CM33_SYSTEM_H_ */
