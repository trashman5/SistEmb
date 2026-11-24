#include "soc_AM335x.h"
#include "gpio_v2.h"

void delay(int val){ for(int i=0; i<val; i++);}

int main(void){
    GPIO1ModuleClkConfig();
    GPIOModuleEnable(SOC_GPIO_1_REGS);
    GPIOModuleReset(SOC_GPIO_1_REGS);

    for (int j=21; j<25; j++){
        GPIODirModeSet(SOC_GPIO_1_REGS, j, GPIO_DIR_OUTPUT);
    }

    while(1){

        for (int j=21; j<25; j++){
            GPIOPinWrite(SOC_GPIO_1_REGS, j, GPIO_PIN_HIGH);
            delay(0x5000);
            GPIOPinWrite(SOC_GPIO_1_REGS, j, GPIO_PIN_LOW);
            delay(0x15000);
        }

    }

    return 0;
}
