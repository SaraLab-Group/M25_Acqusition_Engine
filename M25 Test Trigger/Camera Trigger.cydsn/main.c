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
#define NEW_CNT 0x4
#define ACK_CMD 0x8
#define START_COUNT 0x10
#define COUNTING 0x20
#define STOP_COUNT 0x40
#define CAMERAS_ACQUIRED 0x100
#define RELEASE_CAMERAS 0x200
#define START_CAPTURE 0x800
#define CAPTURING 0x1000
#define STAGE_TRIGG_ENABLE 0x40000
#define STAGE_TRIGG_DISABLE 0x80000
#define SEND_TRIGG 0x100000
#define TRIGG_SENT 0x200000
#define START_LIVE 0x100000
#define LIVE_RUNNING 0x200000
#define STOP_LIVE 0x400000
#define START_Z_STACK 0x800000
#define Z_STACK_RUNNING 0x1000000
#define STOP_Z_STACK 0x2000000
#define TOGGLE_LED 0x4000000
#define TOGGLE_DIG_MOD 0x8000000

#define DEFAULT_FPS (50.0f)
#define CAM_PERIOD (10u)
//#define STAGE_PERIOD (240000u) //for PI Stage

#define STAGE_PERIOD (240000u) //for ASI Stage

struct usb_data{
    float fps;
    uint32 flags;
    uint32 time_waiting; // Currently Time between not ready and ready
    uint64 count;
};

uint32 set_fps();

volatile struct usb_data incoming;
volatile struct usb_data outgoing;
volatile uint8 to_send = 0;
volatile uint8 send_count = 0;
volatile uint8 recieved = 0;
volatile uint8 lcd_draw = 0;
volatile uint32 time_btwn_trig = 0;
volatile double time_sec = 0;
volatile uint16 counter = 0;
volatile float current_fps = DEFAULT_FPS;

// This just grabs the counter time until ready value in clock ticks
CY_ISR(PERIOD_ISR){
    time_btwn_trig = /*MAX_32BIT -*/ Frame_Period_Timer_ReadCounter();
    time_sec = time_btwn_trig * TICK_PERIOD;
    Frame_Period_Timer_ReadStatusRegister();
    RESET_RDY_TIMER_Write(REG_ON);
    RESET_RDY_TIMER_Write(REG_OFF);
    
    //if(send_count){
        //to_send = 1;
        outgoing.count++;
        outgoing.flags |= NEW_CNT;
    //}
//    if(!(counter % 100)){
//        lcd_draw = 1;
//    }
}
volatile uint16 stage_cnt = 0;
CY_ISR(STAGE_ISR){
    // Switch between Trigger Camera and Trigger Stage
    if(DEMUX_SWITCH_Read() & REG_ON){
        DEMUX_SWITCH_Write(REG_OFF);
        STAGE_COACH_WritePeriod(STAGE_PERIOD);
    } else {
        DEMUX_SWITCH_Write(REG_ON);
        STAGE_COACH_WritePeriod(CAM_PERIOD);
    }
    //STAGE_COACH_ReadStatusRegister();
    STAGE_PERIOD_Write(REG_ON);
    STAGE_PERIOD_Write(REG_OFF);
    //STAGE_ISR_ClearPending();
    //LCD_Char_ClearDisplay();
    /*LCD_Char_Position(3u,0u);
    LCD_Char_PrintString("ISR CNT");
    LCD_Char_Position(3u,9u);
    LCD_Char_PrintDecUint16(stage_cnt++);*/
}

/*CY_ISR(TRIG_ISR_BOD){
    Trigger_Count_ReadStatusRegister();  
}*/

int main(void)
{
    //uint16 length;
    incoming.flags = outgoing.flags = 0;
    incoming.fps = outgoing.fps = DEFAULT_FPS;
    outgoing.count = 0;
    //outgoing.flags |= TIMED_TRIGG_MODE;
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */
    
    Trigger_Count_Start(); // Counts how many times a capture event has occured.
    PWM_1_Start();
    //Frame_Period_Timer_Start(); // Main Capture Rate
    TRIG_PERIOD_Start(); //Counting Time Camera isn't Ready

    PERIOD_ISR_StartEx(PERIOD_ISR);
    STAGE_COACH_Start();
    READY_Start();
    STAGE_ISR_StartEx(STAGE_ISR);
    //TRIG_ISR_StartEx(TRIG_ISR_BOD);
    LCD_Char_Start();
    
    
    
    //uint32 count = 0;
    char msg[20];
    char msg2[20];
    char msg3[20];
    char msg4[20];
    
    sprintf(msg2, "CNT DSBLD STOP     ");
    sprintf(msg, "fps: %.2f         ", incoming.fps);
    sprintf(msg3, "Enjoy Delicious    ");
    sprintf(msg4, "    Sandwhich      ");
    
    //char bfr[] = "before";
    //char aftr[] = "after";
    
    // Currently just sleeps for 1sec and counts up
    //LCD_Char_ClearDisplay();
    LCD_Char_Position(1u,0u);
    LCD_Char_PrintString(msg);
    
    LCD_Char_Position(0u, 0u);
    LCD_Char_PrintString(msg2);
    
    LCD_Char_Position(2u, 0u);
    LCD_Char_PrintString(msg3);
   
    LCD_Char_Position(3u, 0u);
    LCD_Char_PrintString(msg4);
    
    
    /* Start USBFS operation with 5V operation. */
    USBFS_Start(USBFS_DEVICE, USBFS_5V_OPERATION);
    //LCD_Char_PrintString(sand);
    /* Wait until device is enumerated by host. */
    while (0u == USBFS_GetConfiguration())
    {
    }
    //set_fps();
    //Just added this since not sure why just ran set fps
    Frame_Period_Timer_WritePeriod(set_fps());
    /* Enable OUT endpoint to receive data from host. */
    USBFS_EnableOutEP(OUT_EP_NUM);
    
    //LCD_Char_ClearDisplay();
    LCD_Char_Position(1u,0u);
    LCD_Char_PrintString(msg);
    LCD_Char_Position(0u,0u);
    LCD_Char_PrintString(msg2);
    LCD_Char_Position(2u,0u);
    LCD_Char_PrintString(msg3);
    LCD_Char_Position(3u,0u);
    LCD_Char_PrintString(msg4);
  
    
    CyDelay(1000);
    STAGE_PERIOD_Write(REG_ON);
    STAGE_PERIOD_Write(REG_OFF);
    
    
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
            //length = USBFS_GetEPCount(OUT_EP_NUM);

            /* Trigger DMA to copy data from OUT endpoint buffer. */

            USBFS_ReadOutEP(OUT_EP_NUM, (uint8*)&incoming, sizeof(struct usb_data));
            /*if(incoming.flags & SEND_TRIGG){
                    SOFT_TRIG_Reg_Write(REG_ON);
                    CyDelayUs(100);
                    SOFT_TRIG_Reg_Write(REG_OFF);
                    incoming.flags &= ~(SEND_TRIGG);
                    outgoing.flags |= TRIGG_SENT;
            }*/
            
            
            // These two statements make sure the trigger isn't running
            // Until all of the CAMERAS have been acquired, and stops the trigger timer
            // If the Cameras are released
            
            if(incoming.flags & START_CAPTURE){
                Z_TRIG_Reg_Write(REG_OFF);
                FRAME_TRIG_Reg_Write(REG_ON);
                //Frame_Period_Timer_Start();
                incoming.flags &= ~(START_CAPTURE);
                if(incoming.flags & START_LIVE){
                    incoming.flags &= ~START_LIVE;
                    sprintf(msg2, "LIVE MODE          ");
                } else {
                    outgoing.count = 0;
                    send_count = 1;
                    outgoing.flags |= START_COUNT;
                    incoming.flags &= ~(START_COUNT);
                    sprintf(msg2, "CNT ENBLD START    ");
                }
                //LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg2);
            }
            
            if(incoming.flags & RELEASE_CAMERAS){
                Frame_Period_Timer_Stop();
                FRAME_TRIG_Reg_Write(REG_OFF);
                Z_TRIG_Reg_Write(REG_OFF);
                incoming.flags &= !(RELEASE_CAMERAS);
                sprintf(msg2, "CNT DSBLD STOP     ");
                //LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg2);
            }
            
            if(incoming.flags & CHANGE_FPS){
                uint32 new_period = set_fps();
                Frame_Period_Timer_WritePeriod(new_period);
                outgoing.fps = incoming.fps;
                incoming.flags &= ~(CHANGE_FPS);
                
                sprintf(msg, "fps: %.2f          ", outgoing.fps);
                //LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg2);
             
            } /*else if(incoming.flags & START_COUNT){
                outgoing.count = 0;
                send_count = 1;
                outgoing.flags |= START_COUNT;
                incoming.flags &= ~(START_COUNT);
                sprintf(msg2, "CNT ENBLD START");
                LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg2);
                
            } */else if(incoming.flags & STOP_COUNT){
                send_count = 0;
                //Frame_Period_Timer_Stop();
                FRAME_TRIG_Reg_Write(REG_OFF);
                Z_TRIG_Reg_Write(REG_OFF);
                incoming.flags &= ~(STOP_COUNT);
                outgoing.flags &= ~(START_COUNT);
                sprintf(msg2, "CNT DSBLD STOP      ");
                //LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg2);
            }   else if(incoming.flags & START_Z_STACK) {
                //Frame_Period_Timer_Stop();
                stage_cnt = 0;
                FRAME_TRIG_Reg_Write(REG_ON);
                Z_TRIG_Reg_Write(REG_ON);
                incoming.flags &= ~(START_Z_STACK);
                //outgoing.flags |= ~(SOFT_TRIGG_MODE);
                //outgoing.flags &= ~(TIMED_TRIGG_MODE);
                sprintf(msg3, "Z STACK             ");
                //LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg3);
            } else if(incoming.flags & CAMERAS_ACQUIRED) {
                FRAME_TRIG_Reg_Write(REG_OFF);
                Z_TRIG_Reg_Write(REG_OFF);                
                Frame_Period_Timer_Start();
                incoming.flags &= ~(CAMERAS_ACQUIRED);
                outgoing.flags |= ~(CAMERAS_ACQUIRED);
                //outgoing.flags &= ~(SOFT_TRIGG_MODE);
                sprintf(msg2, "TRIGGER ARMED       ");
                //LCD_Char_ClearDisplay();    
                LCD_Char_Position(0u,0u);
                LCD_Char_PrintString(msg);
                LCD_Char_Position(1u,0u);
                LCD_Char_PrintString(msg2);
            }
            
            /* Toggle Emmision */
            
            if(incoming.flags & TOGGLE_LED){
                if(DIODE_PWR_Read()){
                    DIODE_PWR_Write(REG_OFF);
                    sprintf(msg3, "LED: OFF ");
                    LCD_Char_Position(2u,0u);
                    LCD_Char_PrintString(msg3);
                } else {
                    DIODE_PWR_Write(REG_ON);
                    sprintf(msg3, "LED: ON  ");
                    LCD_Char_Position(2u,0u);
                    LCD_Char_PrintString(msg3);
                }
                incoming.flags &= ~TOGGLE_LED;
            }
            
            /* Toggle Digital Modulation) */
            if(incoming.flags & TOGGLE_DIG_MOD){
                if(DIG_MOD_Read()){
                    DIG_MOD_Write(REG_OFF);
                    sprintf(msg3, "MOD: OFF  ");
                    LCD_Char_Position(2u,9u);
                    LCD_Char_PrintString(msg3);
                } else {
                    DIG_MOD_Write(REG_ON);
                    sprintf(msg3, "MOD: ON   ");
                    LCD_Char_Position(2u,9u);
                    LCD_Char_PrintString(msg3);
                }
                incoming.flags &= ~TOGGLE_DIG_MOD;
            }
            
            /* (USBFS_GEN_16BITS_EP_ACCESS) */

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
        }
        /* Trigger DMA to copy data into IN endpoint buffer.
        * After data has been copied, IN endpoint is ready to be read by the
        * host.
        */
        //if(to_send){
          
            outgoing.time_waiting = time_btwn_trig;
            outgoing.fps = current_fps;
            USBFS_LoadInEP(IN_EP_NUM, (uint8*)&outgoing, sizeof(struct usb_data));
            /* (USBFS_GEN_16BITS_EP_ACCESS) */
            outgoing.flags &= ~(NEW_CNT);
            to_send = 0;
            CyDelayUs(1000); 
        //}
        
//        if(lcd_draw){
//            LCD_Char_PrintString(msg);
//            LCD_Char_Position(0u, 0u);
//            LCD_Char_PrintString(msg2);
//            count++;
//            sprintf(msg2, "%lu", time_til_ready);
//            sprintf(msg, "%lu", count);
//            //CyDelay(1000u);
//            LCD_Char_ClearDisplay();
//            LCD_Char_Position(1u,0u);
//            lcd_draw = 0;
//        }
    }
    
}

uint32 set_fps(){
    incoming.flags &= ~(CHANGE_FPS);
    outgoing.fps = incoming.fps;
    current_fps = incoming.fps;
    return (CLOCK_FREQ / incoming.fps);
}

/* [] END OF FILE */
