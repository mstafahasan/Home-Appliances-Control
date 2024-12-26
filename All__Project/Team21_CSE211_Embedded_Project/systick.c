#include "systick.h"
#include "TM4C123GH6PM.h"


void SysTick_Init(uint32_t reloadValue){
NVIC_ST_CTRL_R=0;
NVIC_ST_RELOAD_R= reloadValue;
NVIC_ST_CURRENT_R=0;
NVIC_ST_CTRL_R=0X05;
}

void SysTick_Enable(void){
NVIC_ST_CTRL_R!=0X05;
}

void SysTick_Disable(void){
NVIC_ST_CTRL_R &= ~0X01;
}
void SysTick_PeriodSet(uint32_t ui32Period){
NVIC_ST_RELOAD_R= ui32Period -1;
}
uint32_t SysTick_PeriodGet(void){
return NVIC_ST_RELOAD_R  + 1;
}
uint32_t SysTick_ValueGet(void){
return NVIC_ST_CURRENT_R;
}
bool SysTick_Is_Time_Out(void){
return (NVIC_ST_CTRL_R & 0X10000)!=0;
}