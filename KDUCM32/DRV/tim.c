#include "tim.h"

void LCDK_Init(void)
{
	TIM_TimeBaseInitType htim3;
	OCInitType sConfigOC;
	GPIO_InitType GPIO_InitStructure;

	/* TIM3 clock enable */
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);
	
	GPIO_InitStruct(&GPIO_InitStructure);
	GPIO_InitStructure.Pin				= GPIO_PIN_1;
	GPIO_InitStructure.GPIO_Mode 		= GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Current 	= GPIO_DC_4mA;
	GPIO_InitStructure.GPIO_Alternate 	= GPIO_AF2_TIM3;
	GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);
	
	uint16_t PrescalerValue = 0;
	uint32_t tim3Freq		= 5000000;
	RCC_ClocksType RCC_ClockFreq;
	RCC_GetClocksFreqValue(&RCC_ClockFreq);
	
	 /* Compute the prescaler value */	
	//Divide by 4--> timer frequency 12M
	//You need to judge the size of 'HclkFreq'. If it exceeds 27M, you need to divide the frequency. At the same time, 
	//the frequency doubling of the timer requires x2 when the frequency division coefficient is not 1.
	
	//SystemCoreClock
	PrescalerValue = (uint16_t)( (RCC_ClockFreq.HclkFreq>27000000?RCC_ClockFreq.Pclk1Freq*2:RCC_ClockFreq.Pclk1Freq) / tim3Freq) - 1;
	
	
	
	htim3.Prescaler = PrescalerValue;
	htim3.Period 	= 100;						//AutoReload
	htim3.ClkDiv    = 0;
    	htim3.CntMode   = TIM_CNT_MODE_UP;
	TIM_InitTimeBase(TIM3, &htim3);
	
	/* PWM1 Mode configuration: Channel1 */
    sConfigOC.OcMode      = TIM_OCMODE_PWM1;
    sConfigOC.OutputState = TIM_OUTPUT_STATE_ENABLE;
    sConfigOC.Pulse       = 60;						//CCDAT4, Preload
    sConfigOC.OcPolarity  = TIM_OC_POLARITY_HIGH;
    TIM_InitOc4(TIM3, &sConfigOC);
	TIM_ConfigOc4Preload(TIM3, TIM_OC_PRE_LOAD_ENABLE);
 
    TIM_ConfigArPreload(TIM3, ENABLE);
    /* TIM3 enable counter */
    TIM_Enable(TIM3, ENABLE);
}

void BackLight_SetVal(char val)
{
	val = val*6/10;
	TIM3->CCDAT4=val;
	if(val)
		TIM4->CCDAT4=30;
	else
		TIM4->CCDAT4= 0;
}
extern volatile u8  BL;
extern int LAMP_TIME;
void LightBacklight()
{
	if(LAMP_TIME)
	{
		BackLight_SetVal(BL);
		bsp_StartAutoTimer(TMR_FLOW, LAMP_TIME);	//TMR_PERIOD_500 ms    TMR_PERIOD_1S bsp_StartAutoTimer
	}

}

////////////////////////////////////////////////////////////////////////
static volatile uint32_t s_uiDelayCount = 0;
static volatile uint8_t s_ucTimeOutFlag = 0;
static SOFT_TMR s_tTmr[TMR_COUNT];
__IO int32_t g_iRunTime = 0;

static void bsp_SoftTimerDec(SOFT_TMR *_tmr);

//Periodic crossover switch	32 000 000
void TIM2_Init(void)
{
	TIM_TimeBaseInitType htim2;
	NVIC_InitType NVIC_InitStructure;
	
	//RCC_ConfigPclk1(RCC_HCLK_DIV4);
	RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM2, ENABLE);
	
	/* Enable the TIM2 global Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
	extern int LAMP_TIME;
	
	uint16_t PrescalerValue = 0;
	uint16_t tim2Freq		= 50000;
	
	RCC_ClocksType RCC_ClockFreq;
	RCC_GetClocksFreqValue(&RCC_ClockFreq);
	
	//You need to judge the size of 'HclkFreq'. If it exceeds 27M, you need to divide the frequency. 
	//At the same time, the frequency doubling of the timer requires x2 when the frequency division coefficient is not 1.
	
	//SystemCoreClock
	PrescalerValue = (uint16_t)( (RCC_ClockFreq.HclkFreq>27000000?RCC_ClockFreq.Pclk1Freq*2:RCC_ClockFreq.Pclk1Freq) / tim2Freq) - 1;
	//10 000 Hz 		 0.1 ms/Hz
	htim2.Prescaler	= PrescalerValue;			
	//printf("PrescalerValue:%d\n", PrescalerValue);
	//10 Hz*0.1 ms/Hz = 1 ms
	htim2.Period	= tim2Freq/1000;			
	//printf("Period:%d\n", htim2.Period);
	htim2.ClkDiv   	= 0;
	htim2.CntMode	= TIM_CNT_MODE_UP;			//TIM_COUNTERMODE_DOWN;//
	
	TIM_InitTimeBase(TIM2, &htim2);
	//TIM_ConfigPrescaler(TIM2, PrescalerValue, TIM_PSC_RELOAD_MODE_IMMEDIATE);
	/* TIM2 enable update irq */
    TIM_ConfigInt(TIM2, TIM_INT_UPDATE, ENABLE);
	TIM2->CNT=0;
	/* TIM2 enable counter */
    TIM_Enable(TIM2, ENABLE);
	
	bsp_InitTimer();
	bsp_StartAutoTimer(TMR_DELAY_FLASH, TMR_PERIOD_10MS);
	bsp_StartAutoTimer(TMR_FLOW, 		LAMP_TIME);
	bsp_StartAutoTimer(TMR_ASK,         TMR_PERIOD_1S);       //1 sec ask once to make sure the connection is normal
	//s_tTmr[TMR_FLOW].Flag=1;
}
void disposeAllTimeData(void)
{
    if (s_uiDelayCount > 0)
    {
        if (--s_uiDelayCount == 0)
        {
            s_ucTimeOutFlag = 1;
        }
    }

    for (uint8_t i = 0; i < TMR_COUNT; i++)
    {
        bsp_SoftTimerDec(&s_tTmr[i]);
    }

    g_iRunTime++;
    if (g_iRunTime == 0x7FFFFFFF)	
    {
        g_iRunTime = 0;
    }

}



void TIM2_IRQHandler()
{
	//TIM2->SR &= ~(1 << 0);
	//TIM_ClrIntPendingBit(TIM2, TIM_INT_UPDATE);
	
	TIM2->STS &= (uint32_t)~TIM_INT_UPDATE;
	disposeAllTimeData();
}
//
//Update the value of the structure array in the timer interrupt
static void bsp_SoftTimerDec(SOFT_TMR *_tmr)
{
	if (_tmr->Count > 0)
	{
		
		if (--_tmr->Count == 0)
		{
			_tmr->Flag = 1;

			if(_tmr->Mode == TMR_AUTO_MODE)
			{
				_tmr->Count = _tmr->PreLoad;
			}
		}
	}
}
//

void bsp_InitTimer(void)
{
	uint8_t i;

	/* Clear all software timers */
	for (i = 0; i < TMR_COUNT; i++)
	{
		s_tTmr[i].Count = 0;
		s_tTmr[i].PreLoad = 0;
		s_tTmr[i].Flag = 0;
		s_tTmr[i].Mode = TMR_ONCE_MODE;		/* The default is one-time working mode*/
	}
}



//Get the number of runs
int32_t bsp_GetRunTime(void)
{
	int32_t runtime;

	DISABLE_INT();  	/*  */

	runtime = g_iRunTime;	/*  */

	ENABLE_INT();  		/*  */

	return runtime;
}




//

//Start the "timer"
void bsp_StartTimer(uint8_t _id, uint32_t _period)
{
	if (_id >= TMR_COUNT)
	{
		while(1); 
	}

	DISABLE_INT();  			

	s_tTmr[_id].Count = _period;		
	s_tTmr[_id].PreLoad = _period;		
	s_tTmr[_id].Flag = 0;				
	s_tTmr[_id].Mode = TMR_ONCE_MODE;	

	ENABLE_INT();  				
}

//Start the "timer" that automatically fills the overloaded value
void bsp_StartAutoTimer(uint8_t _id, uint32_t _period)
{
	if (_id >= TMR_COUNT)
	{
		while(1); 
	}

	DISABLE_INT();  		

	s_tTmr[_id].Count = _period;			
	s_tTmr[_id].PreLoad = _period;		/*  */
	s_tTmr[_id].Flag = 0;				/*  */
	s_tTmr[_id].Mode = TMR_AUTO_MODE;	/*  */

	ENABLE_INT();  			/*  */
}
//Stop the "timer"
void bsp_StopTimer(uint8_t _id)
{
	if (_id >= TMR_COUNT)
	{
		/*  */
		while(1); /*  */
	}

	DISABLE_INT();  	/*  */

	s_tTmr[_id].Count = 0;				/*  */
	s_tTmr[_id].Flag = 0;				/*  */
	s_tTmr[_id].Mode = TMR_ONCE_MODE;	/*  */

	ENABLE_INT();  		/*  */
}
//
//Check in the program whether the "timer" flag has reached the flag
uint8_t bsp_CheckTimer(uint8_t _id)
{
	if (_id >= TMR_COUNT)
	{
		return 0;
	}

	if (s_tTmr[_id].Flag == 1)
	{
		s_tTmr[_id].Flag = 0;
		return 1;
	}
	else
	{
		return 0;
	}
}

//


