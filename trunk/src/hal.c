/****************************************************************
 * file:	hal.c
 * date:	2009-03-12
 * description:	hardware abstraction layer for CC2430--implement
 ***************************************************************/

#include "hal.h"
#include "types.h"

/******************************************************
    ****************** Definition Statistics **************
 ******************************************************/
#ifdef STATISTIC
UINT16 statistic_mac_tx	=	0;	// mac sent success
UINT16 statistic_mac_rx	=	0;	// received mac packets, excluding ack
UINT16 statistic_mac_drop	=	0;	// drop due to mac buffer full
UINT16 statistic_mac_err	=	0;	// mac sent fail

UINT16 statistic_data_rx	=	0;	// received data for self node
UINT16 statistic_data_tx	=	0;	// sent data from self
UINT16 statistic_data_drop	=	0;	// data dropped due to send buffer full or no route
#endif

static void hal_init_random();

LRWPAN_STATUS_ENUM hal_init()
{
	LRWPAN_STATUS_ENUM hal_state;
	INIT_LED();
	LED_RED_ON();			// indictate initializing

	SET_MAIN_CLOCK_SOURCE(CRYSTAL);
	uart_init();
	mac_timer_init();
	hal_state = radio_init();

   	ENABLE_GLOBAL_INTERRUPT();	//enable interrupts

	hal_init_random();	

	LED_RED_OFF();
	LED_GREEN_ON();			// indicate init finished

	return hal_state;
}

static void hal_init_random()
{
	UINT8 i;

	ISRXON;
	hal_wait(1);
	ADCCON1 &= ~0x0C;		//enable random generator
	for(i = 0 ; i < 32 ; i++)
	{
		RNDH = ADCTSTH;
		ADCCON1 |= 0x04;	//clock random generator
	}
	ADCCON1 |= 0x04;
}

//assumes that Timer2 has been initialized and is running
//also assumes radio is turned on
UINT8 hal_get_random_byte(void)
{
	UINT8 i;
	ADCCON1 |= 0x04;		//advance random number
	i = RNDH;

	return(i);
}

//software delay, waits is in milliseconds
void hal_wait(UINT8 wait)
{
	UINT32 largeWait;

	if(wait == 0)
	{
		return;
	}
	largeWait = ((UINT16) (wait << 7));
	largeWait += 114*wait;

	largeWait = (largeWait >> CLKSPD);
	while(largeWait--)
		;

	return;
}

/*
 * The RF output power settings
 * in register TXCTRLL, PA_LEVEL is [4:0], PA_CURRENT is [7:5]
 * so we use 32 levels
 */
UINT8 hal_get_RF_power(void)
{
	UINT8 temp;

	temp = TXCTRLL;
	return temp;
}

void hal_set_RF_power(UINT8 val)
{

	TXCTRLL = val;
}


#define GRADIENT_C 0.01496
#define OFFSET_C -300                    //偏移量，此参数需要标定
#define ADC14_TO_CELSIUS(ADC_VALUE) \
( (float)ADC_VALUE *(float)GRADIENT_C + OFFSET_C)

#define GRADIENT_F (9.0/5.0 * GRADIENT_C)
#define OFFSET_F (9.0/5.0 * OFFSET_C) + 32
#define ADC14_TO_FARENHEIT(ADC_VALUE) \
( (float)ADC_VALUE *(float)GRADIENT_F + OFFSET_F)

/*Ref: 1,25 V, Resolution: 14bit, Temperature Sensor */
#define SAMPLE_TEMP_SENSOR(v) \
do{ \
ADCCON2 = 0x3E; \
ADCCON1 = 0x73; \
while(!(ADCCON1 & 0x80)); \
v = ADCL; \
v |= (((unsigned int)ADCH) << 8); \
}while(0)
/*
 * get temperature from ADC
 */
//获取温度值函数
UINT8 hal_get_temperature()
{
	float tmp;
	unsigned int value;
	SAMPLE_TEMP_SENSOR(value);       //采样温度
	tmp =  ADC14_TO_CELSIUS(value);  //返回AD采样值
	return (UINT8) tmp;
}
