#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "interrupt.h"

void delay(int val){ for(int i=0; i<val; i++);}

static void GPIOINTCConfigure(void);
volatile int flagIsr=0;

int main(void){
    GPIO1ModuleClkConfig();
    GPIO0ModuleClkConfig();

    GPIOINTCConfigure();

    GPIOModuleEnable(SOC_GPIO_1_REGS); //Enabling the GPIO1 module
    GPIOModuleReset(SOC_GPIO_1_REGS); //Resetting the GPIO1 module
    GPIODirModeSet(SOC_GPIO_1_REGS, 21, GPIO_DIR_OUTPUT); //Set pin 21 as Output
    GPIODirModeSet(SOC_GPIO_1_REGS, 23, GPIO_DIR_OUTPUT);

    GPIOModuleEnable(SOC_GPIO_0_REGS); //Enabling the GPIO1 module
    GPIOModuleReset(SOC_GPIO_0_REGS); //Resetting the GPIO1 module
    GPIODirModeSet(SOC_GPIO_0_REGS, 30, GPIO_DIR_INPUT); //Set pin 30 as Input

    GPIOIntTypeSet(SOC_GPIO_0_REGS, 30, GPIO_INT_TYPE_FALL_EDGE);
    GPIOPinIntEnable(SOC_GPIO_0_REGS, GPIO_INT_LINE_1, 30);

    IntMasterIRQEnable();

    while(1){
        GPIOPinWrite(SOC_GPIO_1_REGS, 21, GPIO_PIN_HIGH ); //High
        delay(0x5000);
        GPIOPinWrite(SOC_GPIO_1_REGS, 21, GPIO_PIN_LOW); //Low
        delay(0x15000);
        //int pin30=GPIOPinRead(SOC_GPIO_0_REGS, 30); //Polling
        if (flagIsr == 1){
            GPIOPinWrite(SOC_GPIO_1_REGS, 23, GPIO_PIN_HIGH ); //High
            flagIsr = 0;
        } else {
            GPIOPinWrite(SOC_GPIO_1_REGS, 23, GPIO_PIN_LOW ); //Low
        }
    }

    return 0;
}

static void GPIOIsr()
{
    GPIOPinIntClear(SOC_GPIO_0_REGS,GPIO_INT_LINE_1, 30);
    flagIsr=1; //User code
}
static void GPIOINTCConfigure(void)
{
    IntAINTCInit(); //Initialize the ARM interrupt control
    IntRegister(SYS_INT_GPIOINT0A, GPIOIsr); //ISR Function
    IntPrioritySet(SYS_INT_GPIOINT0A, 0, AINTC_HOSTINT_ROUTE_IRQ);
    IntSystemEnable(SYS_INT_GPIOINT0A); // Enable the system interrupt
}
