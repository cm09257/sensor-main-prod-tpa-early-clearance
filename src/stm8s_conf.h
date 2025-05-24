#ifndef __STM8S_CONF_H
#define __STM8S_CONF_H




#define TEST_CONF_INCLUDED 1


#ifndef FLASH_DATA_START_PHYSICAL_ADDRESS
#define FLASH_DATA_START_PHYSICAL_ADDRESS   ((uint32_t)0x004000)  // EEPROM-Beginn
#endif

#ifndef FLASH_DATA_END_PHYSICAL_ADDRESS
#define FLASH_DATA_END_PHYSICAL_ADDRESS     ((uint32_t)0x00427F)  // EEPROM-Ende
#endif

#ifndef FLASH_BLOCK_SIZE
#define FLASH_BLOCK_SIZE                    ((uint16_t)64)        // EEPROM Blockgröße
#endif



/* Enable peripheral headers */
#define USE_ADC1
//#define USE_ADC2
#define USE_AWU
#define USE_BEEP
//#define USE_CAN
#define USE_CLK
#define USE_EXTI
#define USE_GPIO
#define USE_ITC
#define USE_UART1
#define USE_TIM1
#define USE_TIM2
#define USE_TIM4
#define USE_SPI
#define USE_I2C


/* assert_param needs to be defined */
#include <stdint.h>
#include <stdio.h>

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif

#endif /* __STM8S_CONF_H */
