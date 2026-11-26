// Lab 1.4 StarterWare APIs: Timers - Traffic light with the user LEDs
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "interrupt.h"
#include "dmtimer.h"
#include "gpio_v2.h"
#include "uartStdio.h"

static void DMTimerAintcConfigure(void);
static void DMTimerSetUp(unsigned int);
static void DMTimerIsr(void);
static void LedsInit(void);
static void SetLight(unsigned int state);

static volatile unsigned int flagIsr = 0;

/* DMTimer2 runs at 24 MHz: overflow after 24,000,000 ticks = 1 second
 * 0xFFFFFFFF - 24000000 + 1 = 0xFE91CA00 */
#define INITIAL_COUNT (0xFE91CA00u)

/* User LEDs (same as Lab 1.3): GPIO1_21..GPIO1_24 */
#define LED_GREEN  21   // LED1
#define LED_AMBER  22   // LED2
#define LED_RED    23   // LED3
#define LED_4      24   // LED4 (not used, kept off)

/* Traffic light states and their durations in seconds */
enum { GREEN = 0, AMBER = 1, RED = 2 };
static const unsigned int duration[3] = {5, 1, 5};
static const char *name[3] = {"GREEN", "AMBER", "RED"};

int main(void)
{
    unsigned int state = GREEN;
    unsigned int seconds = 0;

    UARTStdioInit();             //Initialize the UART console
    LedsInit();                  //Configure user LEDs as outputs
    DMTimer2ModuleClkConfig();   //Enable clocks for the DMTimer2 (24Mhz)
    DMTimerAintcConfigure();     //Register DMTimer2 interrupts on to AINTC
    DMTimerSetUp(INITIAL_COUNT); //1 second period, auto-reload

    DMTimerIntEnable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG); //Enable the DMTimer interrupts
    IntMasterIRQEnable();        //Enable IRQ in CPSR

    UARTprintf("\f\nTraffic light PBM-Tecnun:\n");
    SetLight(state);
    UARTprintf("\r%s  ", name[state]);

    DMTimerEnable(SOC_DMTIMER_2_REGS); //Start the DMTimer

    while(1)
    {
        if(flagIsr == 1) {       // one second elapsed
            flagIsr = 0;
            seconds++;
            if(seconds >= duration[state]) {
                seconds = 0;
                state = (state + 1) % 3;   // GREEN -> AMBER -> RED -> GREEN
                SetLight(state);
                UARTprintf("\r%s  ", name[state]);
            }
        }
    }

    DMTimerDisable(SOC_DMTIMER_2_REGS); //Stop the DMTimer
    return 0; //never reach this line
}

static void LedsInit(void)
{
    GPIO1ModuleClkConfig();              //Enabling functional clocks for GPIO1
    GPIOModuleEnable(SOC_GPIO_1_REGS);   //Enabling the GPIO
    GPIOModuleReset(SOC_GPIO_1_REGS);    //Resetting the GPIO1 module

    for (int j = LED_GREEN; j <= LED_4; j++) {
        GPIODirModeSet(SOC_GPIO_1_REGS, j, GPIO_DIR_OUTPUT); //User LEDs as outputs
        GPIOPinWrite(SOC_GPIO_1_REGS, j, GPIO_PIN_LOW);      //All off
    }
}

/* Turn on only the LED that matches the current state */
static void SetLight(unsigned int state)
{
    GPIOPinWrite(SOC_GPIO_1_REGS, LED_GREEN, state == GREEN ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
    GPIOPinWrite(SOC_GPIO_1_REGS, LED_AMBER, state == AMBER ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
    GPIOPinWrite(SOC_GPIO_1_REGS, LED_RED,   state == RED   ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

static void DMTimerAintcConfigure(void)
{
    IntAINTCInit(); //Initialize the ARM interrupt control
    IntRegister(SYS_INT_TINT2, DMTimerIsr); //Registering DMTimerIsr
    IntPrioritySet(SYS_INT_TINT2, 0, AINTC_HOSTINT_ROUTE_IRQ); //Set the priority
    IntSystemEnable(SYS_INT_TINT2); // Enable the system interrupt
}

static void DMTimerSetUp(unsigned int Value)
{
    DMTimerCounterSet(SOC_DMTIMER_2_REGS, Value); //Load the Counter with Value
    DMTimerReloadSet(SOC_DMTIMER_2_REGS, Value);  //ReLoad the Counter with Value
    // Configure the DMTimer for Auto-reload and compare mode
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);
}

/*** DMTimer interrupt service routine ***/
static void DMTimerIsr(void)
{
    DMTimerIntDisable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG); //Disable Interrupts
    DMTimerIntStatusClear(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_IT_FLAG);//Clear the status flag
    flagIsr = 1; // one second tick
    DMTimerIntEnable(SOC_DMTIMER_2_REGS, DMTIMER_INT_OVF_EN_FLAG); //Enable Interrupts
}
