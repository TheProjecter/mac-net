/****************************************************************
 * file:	hal.h
 * date:	2009-03-12
 * description: hardware abstraction layer for CC2430
 ***************************************************************/
#ifndef	HAL_H
#define HAL_H

#include "types.h"
#include "ioCC2430.h"
#include "frame_defines.h"

#include "hal_uart.h"
#include "hal_led.h"
#include "hal_timer.h"
#include "hal_radio.h"

#define CC2430_FLASH_SIZE	128

#if (CC2430_FLASH_SIZE == 32)
#define IEEE_ADDRESS_ARRAY 0x7FF8
#elif (CC2430_FLASH_SIZE == 64) || (CC2430_FLASH_SIZE == 128)
#define IEEE_ADDRESS_ARRAY 0xFFF8
#endif

// Macros for configuring IO direction:
// Example usage:
//   IO_DIR_PORT_PIN(0, 3, IO_IN);    // Set P0_3 to input
//   IO_DIR_PORT_PIN(2, 1, IO_OUT);   // Set P2_1 to output

#define IO_DIR_PORT_PIN(port, pin, dir)  \
   do {                                  \
      if (dir == IO_OUT)                 \
         P##port##DIR |= (0x01<<(pin));  \
      else                               \
         P##port##DIR &= ~(0x01<<(pin)); \
   }while(0)

// Where port={0,1,2}, pin={0,..,7} and dir is one of:
#define IO_IN   0
#define IO_OUT  1

/********************************************************************
 * interrupt related defines and marco
 * *****************************************************************/
#define INT_ON   1
#define INT_OFF  0
#define INT_SET  1
#define INT_CLR  0

// Global interrupt enables
#define INT_GLOBAL_ENABLE(on) EA=(!!on)
#define SAVE_AND_DISABLE_GLOBAL_INTERRUPT(x) {x=EA;EA=0;}
#define RESTORE_GLOBAL_INTERRUPT(x) EA=x
#define ENABLE_GLOBAL_INTERRUPT() INT_GLOBAL_ENABLE(INT_ON)
#define DISABLE_GLOBAL_INTERRUPT() INT_GLOBAL_ENABLE(INT_OFF)
#define DISABLE_ALL_INTERRUPTS() (IEN0 = IEN1 = IEN2 = 0x00)

/********************************************************************
 *   Power and clock management
 * *****************************************************************/
// These macros are used to set power-mode, clock source and clock speed.
// Macro for getting the clock division factor
#define CLKSPD  (CLKCON & 0x07)

// Macro for getting the timer tick division factor.
#define TICKSPD ((CLKCON & 0x38) >> 3)

// Macro for checking status of the crystal oscillator
#define XOSC_STABLE (SLEEP & 0x40)

// Macro for checking status of the high frequency RC oscillator.
#define HIGH_FREQUENCY_RC_OSC_STABLE    (SLEEP & 0x20)

// Macro for setting power mode
#define SET_POWER_MODE(mode)                   \
   do {                                        \
      if(mode == 0)        { SLEEP &= ~0x03; } \
      else if (mode == 3)  { SLEEP |= 0x03;  } \
      else { SLEEP &= ~0x03; SLEEP |= mode;  } \
      PCON |= 0x01;                            \
      asm("NOP");                              \
   }while (0)

// Where _mode_ is one of
#define POWER_MODE_0  0x00  // Clock oscillators on, voltage regulator on
#define POWER_MODE_1  0x01  // 32.768 KHz oscillator on, voltage regulator on
#define POWER_MODE_2  0x02  // 32.768 KHz oscillator on, voltage regulator off
#define POWER_MODE_3  0x03  // All clock oscillators off, voltage regulator off

// Macro for setting the 32 KHz clock source
#define SET_32KHZ_CLOCK_SOURCE(source) \
   do {                                \
      if( source ) {                   \
         CLKCON |= 0x80;               \
      } else {                         \
         CLKCON &= ~0x80;              \
      }                                \
   } while (0)

// Where _source_ is one of
#define CRYSTAL 0x00
#define RC      0x01

// Macro for setting the main clock oscillator source,
//turns off the clock source not used
//changing to XOSC will take approx 150 us
#define SET_MAIN_CLOCK_SOURCE(source) \
   do {                               \
      if(source) {                    \
        CLKCON |= 0x40;               \
        while(!HIGH_FREQUENCY_RC_OSC_STABLE); \
        if(TICKSPD == 0){             \
          CLKCON |= 0x08;             \
        }                             \
        SLEEP |= 0x04;                \
      }                               \
      else {                          \
        SLEEP &= ~0x04;               \
        while(!XOSC_STABLE);          \
        asm("NOP");                   \
        CLKCON &= ~0x47;              \
        SLEEP |= 0x04;                \
      }                               \
   }while (0)

/********************************************************************
 * hal functions
 * *****************************************************************/
LRWPAN_STATUS_ENUM hal_init();
void hal_wait(UINT8 wait);
UINT8 hal_get_random_byte(void);

// RF power settings
#define RF_POWER_RANKGING 16	// Ranking in 16 level
UINT8 hal_get_RF_power(void);
void hal_set_RF_power(UINT8 index);

#define hal_idle()      // do nothing in idle state
// temperature
UINT8 hal_get_temperature();

/******************************************************
    ********************** Statistics **************
 ******************************************************/
#define STATISTIC
#ifdef STATISTIC
extern UINT16 statistic_mac_tx;		// mac sent success
extern UINT16 statistic_mac_rx;		// received mac packets
extern UINT16 statistic_mac_drop;		// drop due to mac buffer full
extern UINT16 statistic_mac_err	;	// mac sent fail

extern UINT16 statistic_data_rx;		// received data for self node
extern UINT16 statistic_data_tx;		// sent data from self
extern UINT16 statistic_data_drop;		// data dropped due to send buffer full
#endif

#endif
