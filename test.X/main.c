/* 
 * 
 *	ALERT - MUDEI A FREQ DO TIMER 1 PARA FICAR MAIS RAPIDO
 * 		NECESSARIO ALTERAR 
 *
 * 	NOTE - KEYWORD PARA ALTERACOES 
*/

#include <xc.h>
#include "mcc_generated_files/mcc.h"
#include "eeprom_rw.h"

#define NREG 30
#define PMON 5
#define TALA 3
#define ALAT 25
#define ALAL 2
#define ALAF 0
#define CLKH 0
#define CLKM 0

#define ON 1
#define OFF 0
#define SET 2

#define FIELD 0
#define SUBFIELD 1

#define NONE -1
#define MIN_TIME -1

#define TIMER_MS_RESET 200
#define DEBNC_TIME 10     //200ms

volatile int value = 0;
void handler_clock_hms(void);
void handler_clock_ms(void);
void copyto_EEPROM(void);
void LED_bin(unsigned int value);
void all_LED(void);
unsigned int ADC_read(void);
void mod1_LED(void);
void mod2_LED(void);
void mod3_LED(void);
void mod4_LED(void);

unsigned char checkDebounceSW1();
unsigned char checkDebounceSW2();
void representLed(unsigned char _value);

void clock_field(void);
void config_routine(void);
void clock_subfields(void);
void increment_subfield(void);
void mode_select_LED();
        
volatile unsigned char clkh = CLKH;
volatile unsigned char clkm = CLKM;
volatile unsigned char seg;
volatile unsigned char clkms = 0;

unsigned int hours_tens =0; // 0 -23
unsigned int hours_units =0;
unsigned int min_tens =0; //0 -59
unsigned int min_units =0;

unsigned char last_ms = 0;
unsigned char last_ms2 = 0;

unsigned int mode_field_subfield[2]= {NONE,NONE};
volatile unsigned char set_mode = 0;
volatile unsigned char config_mode = OFF;
volatile unsigned char alarm = 0;

unsigned int convertedValue = 0;
unsigned int duty_cycle = 0;

unsigned int level_bin = 0;
unsigned int lum_threshold = 0;

unsigned char nreg = NREG;
unsigned char nreg_pt;
unsigned char pmon = PMON;
unsigned char tala = TALA;


/*******************************************
 *  Desc: External Interrupt
 *******************************************/
void sw1_EXT(void){
    

    if(checkDebounceSW1()){
        if (alarm == ON){               	// Turn off the alarm 
            alarm = OFF;
            RA6_SetLow();
            PWM6_LoadDutyValue(OFF); 
        }
        else{
            if(!IO_RB4_GetValue()){
               
                if(config_mode == OFF){
                    config_mode = ON; 			// NOTE after changing to Configure mode disable the EXT interrupt and only check if is pressed at main loop		
                   
                    EXT_INT_InterruptDisable();

                    }					// for not overloading the interrupt vector ISR            
               }
            } 
        }
  }
    
    


/*******************************************
 *  Desc: Timer 0 interrupt
 *******************************************/
void ISR_3s(void){

    if (lum_threshold){     //check if we still have a issue
        PWM6_LoadDutyValue(1023);
        alarm = ON;
        
    }
    
    TMR0_StopTimer();
    PIE0bits.TMR0IE = 0;
    
}

void main(void){
    SYSTEM_Initialize();
   
    TMR0_SetInterruptHandler(ISR_3s);
    TMR1_SetInterruptHandler(handler_clock_hms);
    INT_SetInterruptHandler(sw1_EXT);
    TMR2_SetInterruptHandler(handler_clock_ms);
   
    unsigned int task_schedule = 0;
    
    recoverData();
  
    PWM6_LoadDutyValue(OFF);
    alarm = OFF ;
    IO_RB4_SetPullup();                                 //Help debouncing 

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();
    
  
    while (1)
    {   
        
        //SLEEP();
        //NOP();


              do{
                    if(!config_mode){
  
                        convertedValue = ADC_read();
                        level_bin = (convertedValue >> 8);
                        LED_bin(level_bin);                
                        lum_threshold = (level_bin > ALAL);

                        if(lum_threshold){
                            if(alarm == SET){           //if alarm is set ON you need to press SW1 ON 
                                duty_cycle +=1 ;   
                                PWM6_LoadDutyValue(duty_cycle);
                            }
                            else if(alarm == OFF){
                                PIE0bits.TMR0IE = 1;    // We exceed the limit - forget about of Low Power mode and check the 3 sec 
                                TMR0_StartTimer();
                                alarm = SET ;  
                            }
                        }
                        else{
                            if(alarm == SET){
                                PWM6_LoadDutyValue(OFF);
                                alarm = OFF ;
                            }
                        }

                     }
                    else if(config_mode == ON){

                
                      EXT_INT_InterruptDisable(); 
                      config_routine();
                      EXT_INT_InterruptEnable(); 
                }
                    
                __delay_ms(1);

                    
                }while(1);    // NOTE maintain this state - ONly debug
   
    
    }

}





void config_routine(void){
    
    unsigned int select_mode =0;
      last_ms = clkms;
      last_ms2 = clkms;
    
    
            do{
                if(mode_field_subfield[FIELD] == NONE && select_mode == 0){  			
                        all_LED();} 

                if(!IO_RB4_GetValue()){		  
                    if(checkDebounceSW1()){

                            select_mode +=1;
 
                        switch(select_mode){			
                            case 1: mod1_LED();break;
                            case 2: mod2_LED();break;
                            case 3: mod3_LED();break;
                            case 4: mod4_LED();break;
                            default: select_mode = 0; config_mode = OFF; alarm = OFF;	// NOTE Enable EXT interrupt or at that moment when the pic is moving to normal operation
                            break;

                            }   
                        }
                        
                        
                        last_ms = clkms;
                    }
                       

                if(!IO_RC5_GetValue()){
                    if(checkDebounceSW2()){                
                        mode_field_subfield[FIELD] = select_mode;
                        mode_select_LED();      // notice the select was done
                        
                        if(select_mode== 1){    //send to subfield functions here
                            clock_subfields();
                        }
                        
                        

                        }
                    }
              
                   __delay_ms(2);
               
            }while(config_mode == ON);  
    
    mode_field_subfield[FIELD] = NONE;
    mode_field_subfield[SUBFIELD] = NONE;
    
}
void increment_subfield(void){  //funcao universal para todos os subfields 
    
    int plus = 10;
    int exit = 0;
    
    PWM6_LoadDutyValue(0);
          
           while(exit == 0) {     
             
               if(!IO_RC5_GetValue()){
                    if(checkDebounceSW2()){
                         plus += 100;   //teste do incremento de um subfield
                         PWM6_LoadDutyValue(plus);
                         
                        }
              }
               if(!IO_RB4_GetValue()){		  
                    if(checkDebounceSW1()){
                        exit = 1;
                    }
             
             
          }
             
             
        }
}

void clock_subfields(void){ // o clock tem 4 subfields
    
    unsigned int  subfield = 1; 
    
         do{
      
                if(!IO_RB4_GetValue()){		  
                    if(checkDebounceSW1()){
                            subfield +=1;
                        }
                        last_ms = clkms;
                    }
                       
                       switch(subfield){			// Apenas faz display do LED
                            case 1: mod1_LED();break;
                            case 2: mod2_LED();break;
                            case 3: mod3_LED();break;
                            case 4: mod4_LED();break;
                            default: 
                            break;

                            }   
               

                if(!IO_RC5_GetValue()){             // Select operation
                    if(checkDebounceSW2()){
                           
                        mode_field_subfield[SUBFIELD] = subfield;
                        increment_subfield();
                        }
                    }
              
                   __delay_ms(2);
               
            }while(subfield <= 4 );  
    
    
    
}


/*******************************************
 *  Func: all_LED
 *  Desc: Blink all LEDs
 *  Obs: 
 *******************************************/



void all_LED(void){
    
       IO_RA7_SetHigh();
        __delay_ms(100);      
       IO_RA7_SetLow();
       __delay_ms(100); 
       //RA6_SetHigh();
       PWM6_LoadDutyValue(1023);
        __delay_ms(100);        
       //RA6_SetLow();
        PWM6_LoadDutyValue(OFF);
        __delay_ms(100);        
       IO_RA5_SetHigh();       
         __delay_ms(100);       
       IO_RA5_SetLow();
       __delay_ms(100); 
       IO_RA4_SetHigh();
        __delay_ms(100); 
       IO_RA4_SetLow();       
                                
    
    
}

/*******************************************
 *  Func: LED_bin
 *  Desc: Convert voltage into 2 bit levels
 *  Obs: Assign the bit code to RA5 and RA4
 *******************************************/

void LED_bin(unsigned int value){
    
    IO_RA4_LAT =  (value & 1);
    IO_RA5_LAT =  (value >>1);
  
}

/*******************************************
 *  Func: ADC_read
 *  Desc: Read the RB4 port
 *  Obs:  ADC()
 *******************************************/

unsigned int ADC_read(void){
    
    ADCC_StartConversion(channel_ANA0);
    while(!ADCC_IsConversionDone()){
        __delay_ms(1);
    }
                        
    return ADCC_GetConversionResult();
}

void handler_clock_hms(void){
   
    if(!config_mode){
        IO_RA7_Toggle();
    }
    
    seg++;
    if(seg >= 60) {
        clkm++;
        seg = 0;
        if(clkm >= 60) {
            clkh++;
            clkm = 0;
        }
    }
     
}
void handler_clock_ms(void){   
    clkms++;
    
    if(clkms > TIMER_MS_RESET){
        clkms = 0;
    }
}

void copyto_EEPROM(void) {
    
}


void mod1_LED(void){
    LATA = 0;
    PWM6_LoadDutyValue(OFF);
    IO_RA7_SetHigh();
     
}

void mod2_LED(void){
    LATA = 0;
    PWM6_LoadDutyValue(1023);    
}

void mod3_LED(void){
    LATA = 0;
    PWM6_LoadDutyValue(OFF);
    IO_RA5_SetHigh();
     
}

void mod4_LED(void){
    LATA = 0;
    PWM6_LoadDutyValue(OFF);
    IO_RA4_SetHigh();
}

unsigned char checkDebounceSW1(){
    //Fazer disable interrupt clkms
    
    if(clkms - last_ms < 0){       // clkms deu reset
        
        if ((TIMER_MS_RESET - last_ms)+ clkms > DEBNC_TIME ){
            last_ms = clkms;
            return 1;
        }
    }
    
    if(clkms - last_ms < DEBNC_TIME){
        return 0;
    }else{
        last_ms = clkms;
        return 1;
    }
}


unsigned char checkDebounceSW2(){
    //Fazer disable interrupt clkms
    
    if(clkms - last_ms2 < 0){       // clkms deu reset
        
        if ((TIMER_MS_RESET - last_ms2)+ clkms > DEBNC_TIME ){
            last_ms2 = clkms;
            return 1;
        }
    }
    
    if(clkms - last_ms2 < DEBNC_TIME){
        return 0;
    }else{
        last_ms2 = clkms;
        return 1;
    }
}


void mode_select_LED(){
   
    
                        PWM6_LoadDutyValue(1023);
                        IO_RA4_SetHigh();
                        __delay_ms(500);
                        IO_RA5_SetHigh();
                        __delay_ms(500);

                        IO_RA4_SetLow();
                        IO_RA5_SetLow();
    
    
}
//Representa um valor nos LEDs, so consegue representar valores entre 0 a 2^4 -1 = 15 (decimal)
/*void representLed(unsigned char _value){
    LATA = 0;

    if(_value >> 4){         //Se o valor for acima de 16
        return
    }

    LATAbits.LATA7 = _value & 0b1000 ;   //MSB, primeiro led
    LATAbits.LATA6 = _value & 0b100;
    LATAbits.LATA5 = _value & 0b10;
    LATAbits.LATA4 = _value & 1;
}
*/