/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"
#include <stdio.h> //for sprintf

#define REG_ON (1u)
#define REG_OFF (0u)
#define MAX_32BIT (0xFFFFFFFE)
#define TICK_PERIOD (4.166666e-8)

volatile uint32 time_til_ready = 0;
volatile double time_sec = 0;

// This just grabs the counter time until ready value in clock ticks
CY_ISR(CMRA_RDY_ISR){
    time_til_ready = /*MAX_32BIT -*/ Frame_Period_Timer_ReadCounter();
    time_sec = time_til_ready * TICK_PERIOD;
    Frame_Period_Timer_ReadStatusRegister();
    RESET_RDY_TIMER_Write(REG_ON);
    RESET_RDY_TIMER_Write(REG_OFF);
}

/*CY_ISR(TRIG_ISR_BOD){
    Trigger_Count_ReadStatusRegister();  
}*/

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */
    Trigger_Count_Start(); // Counts how many times a capture event has occured.
    Frame_Period_Timer_Start(); // Main Capture Rate
    CMRA_RDY_TIMER_Start(); //Counting Time Camera isn't Ready
    READY_ISR_StartEx(CMRA_RDY_ISR);
    //TRIG_ISR_StartEx(TRIG_ISR_BOD);
    LCD_Char_Start();
    
    
    uint32 count = 0;
    char msg[16];
    char msg2[16];
    sprintf(msg2, "%lu", time_til_ready);
    sprintf(msg, "%lu", count);
    
    // Currently just sleeps for 1sec and counts up
    LCD_Char_ClearDisplay();
    LCD_Char_Position(1u,0u);
    for(;;)
    {
        /* Place your application code here. */
        LCD_Char_PrintString(msg);
        LCD_Char_Position(0u, 0u);
        LCD_Char_PrintString(msg2);
        count++;
        sprintf(msg2, "%lu", time_til_ready);
        sprintf(msg, "%lu", count);
        CyDelay(1000u);
        LCD_Char_ClearDisplay();
        LCD_Char_Position(1u,0u);
    }
    
}

/* [] END OF FILE */
