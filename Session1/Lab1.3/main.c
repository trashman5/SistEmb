#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "uartStdio.h"
#include "uart_irda_cir.h"      // UARTCharGetNonBlocking()
#include <string.h>             // strcmp()

void delay(int val){ for(int i=0; i<val; i++);}

static int flags[4] = {0,0,0,0};

/* Parses "ledN=on" / "ledN=off" (line is already lowercase) */
static void parseCommand(char *s){
    while (*s == ' ') s++;                       // skip leading spaces
    char *end = s;
    while (*end) end++;
    while (end > s && end[-1] == ' ') *--end = '\0';  // trim trailing spaces

    if (s[0]=='l' && s[1]=='e' && s[2]=='d' &&
        s[3]>='1' && s[3]<='4' && s[4]=='='){
        int led = s[3] - '1';
        if (strcmp(s+5, "on") == 0){
            flags[led] = 1;
            UARTprintf("\nLED%d ON", led+1);
            return;
        }
        if (strcmp(s+5, "off") == 0){
            flags[led] = 0;
            UARTprintf("\nLED%d OFF", led+1);
            return;
        }
    }
    UARTprintf("\nUnknown command: %s  (use led1=on ... led4=off)", s);
}

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

    char line[32];
    int len = 0;        // chars currently in line[]
    int tick = 0;       // ticks spent in current blink phase
    int ledOn = 0;      // current blink phase (1 = on)

    while(1){
        /* 1. Read every char waiting in the UART FIFO, never block */
        int c;
        while ((c = UARTCharGetNonBlocking(SOC_UART_0_REGS)) != -1){
            if (c == '\r' || c == '\n'){             // Enter -> process line
                if (len > 0){
                    line[len] = '\0';
                    parseCommand(line);
                    len = 0;
                    UARTprintf("\nType command: ");
                }
            } else if (c == 8 || c == 127){          // Backspace / Delete
                if (len > 0){
                    len--;
                    UARTPutc('\b'); UARTPutc(' '); UARTPutc('\b');
                }
            } else if (len < 31){            // normal char
                if (c >= 'A' && c <= 'Z') c += 32;   // to lowercase
                line[len++] = (char)c;
                UARTPutc((unsigned char)c);          // echo
            }
        }

        /* 2. Advance the blink timer */
        tick++;
        if (ledOn && tick >= 0x50){ ledOn = 0; tick = 0; }
        else if (!ledOn && tick >= 0x150){ ledOn = 1; tick = 0; }

        /* 3. Drive the LEDs: enabled ones follow the blink, others stay off */
        for (int k=0; k<4; k++){
            GPIOPinWrite(SOC_GPIO_1_REGS, 21+k,
                         (flags[k] && ledOn) ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
        }

        delay(0x100);
    }

    return 0;
}

