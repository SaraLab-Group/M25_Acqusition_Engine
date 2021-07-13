/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF UCSC M25 Research Team.
 *
 * ========================================
*/

/* Currently Verifying USB Bulk Transfer Method for Creating basic compact data relay */

#include "project.h"
#include <stdio.h> //for sprintf

#define REG_ON (1u)
#define REG_OFF (0u)
#define MAX_32BIT (0xFFFFFFFE)
#define TICK_PERIOD (4.166666e-8) // 24MHZ
#define CLOCK_FREQ (24000000u)

/* USB device number. */
#define USBFS_DEVICE  (0u)

/* Active endpoints of USB device. */
#define IN_EP_NUM     (1u)
#define OUT_EP_NUM    (2u)

/* Size of SRAM buffer to store endpoint data. */
#define BUFFER_SIZE   (64u)

#if (USBFS_16BITS_EP_ACCESS_ENABLE)
    /* To use the 16-bit APIs, the buffer has to be:
    *  1. The buffer size must be multiple of 2 (when endpoint size is odd).
    *     For example: the endpoint size is 63, the buffer size must be 64.
    *  2. The buffer has to be aligned to 2 bytes boundary to not cause exception
    *     while 16-bit access.
    */
    #ifdef CY_ALIGN
        /* Compiler supports alignment attribute: __ARMCC_VERSION and __GNUC__ */
        CY_ALIGN(2) uint8 buffer[BUFFER_SIZE];
    #else
        /* Complier uses pragma for alignment: __ICCARM__ */
        #pragma data_alignment = 2
        uint8 buffer[BUFFER_SIZE];
    #endif /* (CY_ALIGN) */
#else
    /* There are no specific requirements to the buffer size and alignment for 
    * the 8-bit APIs usage.
    */
    uint8 buffer[BUFFER_SIZE];
#endif /* (USBFS_GEN_16BITS_EP_ACCESS) */

//Stuff for USB communication
#define CHANGE_FPS 0x1
#define DROPPED_FRAME 0x2
#define SET_RTC 0x4
#define ACK_CMD 0x8
#define START_COUNT 0x10
#define COUNTING 0x20
#define DEFAULT_FPS (100u)

struct usb_data{
    uint16 flags;
    uint16 fps;
    uint32 time_waiting; // Currently Time between not ready and ready
};

uint32 set_fps();

struct usb_data incoming;
struct usb_data outgoing;

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
    uint16 length;
    incoming.flags = outgoing.flags = 0;
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
    
    char bfr[] = "before";
    char aftr[] = "after";
    
    // Currently just sleeps for 1sec and counts up
    LCD_Char_ClearDisplay();
    LCD_Char_Position(1u,0u);
    
    LCD_Char_PrintString(bfr);
    LCD_Char_Position(0u, 0u);
    
    /* Start USBFS operation with 5V operation. */
    USBFS_Start(USBFS_DEVICE, USBFS_5V_OPERATION);
    char sand[] = "sandwhich";
    LCD_Char_PrintString(sand);
    /* Wait until device is enumerated by host. */
    while (0u == USBFS_GetConfiguration())
    {
    }

    /* Enable OUT endpoint to receive data from host. */
    USBFS_EnableOutEP(OUT_EP_NUM);
    
    LCD_Char_ClearDisplay();
    LCD_Char_Position(1u,0u);
    LCD_Char_PrintString(aftr);
    
    CyDelay(1000);
    
    
    for(;;)
    {
        /* Place your application code here. */
        
        /*USB BULK TRANSFER SAMPLE CODE*/
        
        
        /* Check if configuration is changed. */
        if (0u != USBFS_IsConfigurationChanged())
        {
            /* Re-enable endpoint when device is configured. */
            if (0u != USBFS_GetConfiguration())
            {
                /* Enable OUT endpoint to receive data from host. */
                USBFS_EnableOutEP(OUT_EP_NUM);
            }
        }

        /* Check if data was received. */
        if (USBFS_OUT_BUFFER_FULL == USBFS_GetEPState(OUT_EP_NUM))
        {
            /* Read number of received data bytes. */
            length = USBFS_GetEPCount(OUT_EP_NUM);

            /* Trigger DMA to copy data from OUT endpoint buffer. */
        #if (USBFS_16BITS_EP_ACCESS_ENABLE)
            USBFS_ReadOutEP16(OUT_EP_NUM, buffer, length);
        #else
            USBFS_ReadOutEP(OUT_EP_NUM, buffer, length);
        #endif /* (USBFS_GEN_16BITS_EP_ACCESS) */

            /* Wait until DMA completes copying data from OUT endpoint buffer. */
            while (USBFS_OUT_BUFFER_FULL == USBFS_GetEPState(OUT_EP_NUM))
            {
            }
            
            /* Enable OUT endpoint to receive data from host. */
            USBFS_EnableOutEP(OUT_EP_NUM);

            /* Wait until IN buffer becomes empty (host has read data). */
            while (USBFS_IN_BUFFER_EMPTY != USBFS_GetEPState(IN_EP_NUM))
            {
            }

        /* Trigger DMA to copy data into IN endpoint buffer.
        * After data has been copied, IN endpoint is ready to be read by the
        * host.
        */
        #if (USBFS_16BITS_EP_ACCESS_ENABLE)
            USBFS_LoadInEP16(IN_EP_NUM, buffer, length);
        #else
            USBFS_LoadInEP(IN_EP_NUM, buffer, length);
        #endif /* (USBFS_GEN_16BITS_EP_ACCESS) */
        }
        
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

uint32 set_fps(){
    incoming.flags &= ~(CHANGE_FPS);
    outgoing.fps = incoming.fps;
    return (CLOCK_FREQ / incoming.fps);
}

/* [] END OF FILE */