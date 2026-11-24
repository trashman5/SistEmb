#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "uartStdio.h"

void delay(int val){ for(int i=0; i<val; i++);}

int main(void){
    UARTStdioInit(); //Uart0 enable
    GPIO1ModuleClkConfig(); //Enabling functional clocks for GPIO1
    GPIOModuleEnable(SOC_GPIO_1_REGS); //Enabling the GPIO
    GPIOModuleReset(SOC_GPIO_1_REGS); //Resetting the GPIO1 module

    for (int j=21; j<25; j++){
            GPIODirModeSet(SOC_GPIO_1_REGS, j, GPIO_DIR_OUTPUT); //Set all user LED to outputs
    }

    UARTprintf("\nStarterWare Program");
    UARTprintf("\nType command: ");

    int flags[4]={0,0,0,0};
    char input[64];

    while(1){

        for (int j=21; j<25; j++){
            if (flags[j-21]==1){
                GPIOPinWrite(SOC_GPIO_1_REGS, j, GPIO_PIN_HIGH ); //High
                delay(0x5000);
                GPIOPinWrite(SOC_GPIO_1_REGS, j, GPIO_PIN_LOW); //Low
                delay(0x15000);
            }
        }


        if (UARTCharsAvail(SOC_UART_0_REGS)){

           for (int i=5; i<64; i++){
               input[i]=UARTGetc(); //Blocking function
               UARTprintf(input[i]);

               if (input[i]==13){
                   if(input[i-1]=='f' && input[i-5]=='1'){
                       flags[0]=0;
                   }

                   if(input[i-1]=='f' && input[i-5]=='2'){
                       flags[1]=0;
                  }

                   if(input[i-1]=='f' && input[i-5]=='3'){
                       flags[2]=0;
                   }

                   if(input[i-1]=='f' && input[i-5]=='4'){
                       flags[3]=0;
                   }

                   if(input[i-1]=='n' && input[i-4]=='1'){
                       flags[0]=1;
                   }

                   if(input[i-1]=='n' && input[i-4]=='2'){
                       flags[1]=1;
                   }

                   if(input[i-1]=='n' && input[i-4]=='3'){
                       flags[2]=1;
                   }

                   if(input[i-1]=='n' && input[i-4]=='4'){
                       flags[3]=1;
                   }

                   i=5;
               }
           }


        }

    }

    return 0;
}
