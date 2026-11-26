// Lab 2.4 StarterWare APIs: Timers - Tecnun2025
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "interrupt.h"
#include "dmtimer.h"
#include "uartStdio.h"

static void DMTimerAintcConfigure(void);
static void DMTimerSetUp(int);
static void DMTimerIsr(void);

static volatile unsigned int cntValue = 20;
static volatile unsigned int flagIsr = 0;
#define INITIAL_COUNT (0xFF000000u) //clock a 24Mhz ~0.7seg

int main(void)
{
    DMTimer2ModuleClkConfig(); //Enable clocks for the DMTimer2 (24Mhz)
    UARTStdioInit(); //Initialize the UART console
    DMTimerAintcConfigure(); //Register DMTimer2 interrupts on to AINTC
    DMTimerSetUp(INITIAL_COUNT); //Configurations for DMTimer

    DMTimerIntEnable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG); //Enable the DMTimer interrupts
    DMTimerEnable(SOC_DMTIMER_2_REGS); //Start the DMTimer

    IntMasterIRQEnable(); //Enable IRQ in CPSR
    UARTprintf("\f\nContador20 PBM-Tecnun:\n ");
    while(cntValue)
    {
        if(flagIsr == 1) {
            UARTprintf("\rCont=%d ",(cntValue - 1));
            cntValue--;
            if ( cntValue<=0 ) cntValue=20;
            flagIsr = 0;
        }
        if (UARTCharsAvail(SOC_UART_0_REGS)){
            char ch=UARTGetc(); //Blocking function
            if (ch==13) UARTprintf("\n\rcmd>",ch);
        }
    }
    DMTimerDisable(SOC_DMTIMER_2_REGS); //Stop the DMTimer
    return 0; //never reach this line
    }
static void DMTimerAintcConfigure(void)
{
    IntAINTCInit(); //Initialize the ARM interrupt control
    IntRegister(SYS_INT_TINT2, DMTimerIsr); //Registering DMTimerIsr
    IntPrioritySet(SYS_INT_TINT2, 0, AINTC_HOSTINT_ROUTE_IRQ); //Set the priority
    IntSystemEnable(SYS_INT_TINT2); // Enable the system interrupt
}
static void DMTimerSetUp(int Value)
{
    DMTimerCounterSet(SOC_DMTIMER_2_REGS, Value); //Load the Counter with Value
    DMTimerReloadSet(SOC_DMTIMER_2_REGS, Value); //ReLoad the Counter with Value;
    // Configure the DMTimer for Auto-reload and compare mode
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);
}
/*** DMTimer interrupt service routine ***/
static void DMTimerIsr(void)
{
    DMTimerIntDisable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG); //Disable Interrupts
    DMTimerIntStatusClear(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_IT_FLAG);//Clear the status flag
    flagIsr = 1; // User Code
    DMTimerIntEnable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG); //Enable Interrupts
}
