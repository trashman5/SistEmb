#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "uartStdio.h"
#include "uart_irda_cir.h"
#include "tsc_adc.h"

#define STEP_BAT        0          //StepConfig index 0 -> STEPCONFIG1 (enable bit 1)
#define STEP_POT        1          //StepConfig index 1 -> STEPCONFIG2 (enable bit 2)
#define BAT_OPEN_DELAY  0x3FFFF    //Max open delay: ~0.52 s @ 500 kHz, lets AIN7 settle
#define ADC_TIMEOUT     50000000u  //Safety exit for the FIFO wait

int main(void)
{
    unsigned int timeout;
    unsigned int raw_bat, raw_pot;
    int mv;

    UARTStdioInit();   //Uart0 enable

    TSCADCModuleClkConfig(); // Enable the clock for ADC
    TSCADCPinMuxSetUp();     //Mux Pins

    // Configures ADC to 500khz
    TSCADCConfigureAFEClock(SOC_ADC_TSC_0_REGS, 24000000, 500000);
    TSCADCTSModeConfig(SOC_ADC_TSC_0_REGS, TSCADC_GENERAL_PURPOSE_MODE);
    TSCADCStepConfigProtectionDisable(SOC_ADC_TSC_0_REGS); //Disable Proteccion

    //Step BAT: CHANNEL8 = AIN7 (VCC through onboard divider). Measured FIRST.
    TSCADCTSStepConfig(SOC_ADC_TSC_0_REGS, STEP_BAT, TSCADC_NEGATIVE_REF_VSSA,
                       TSCADC_POSITIVE_INP_CHANNEL8, TSCADC_NEGATIVE_INP_CHANNEL1,
                       TSCADC_POSITIVE_REF_VDDA);
    TSCADCTSStepOperationModeControl(SOC_ADC_TSC_0_REGS, 0, STEP_BAT); //0:single Ended
    TSCADCTSStepFIFOSelConfig(SOC_ADC_TSC_0_REGS, STEP_BAT, TSCADC_FIFO_0);
    TSCADCTSStepModeConfig(SOC_ADC_TSC_0_REGS, STEP_BAT, TSCADC_ONE_SHOT_SOFTWARE_ENABLED);
    TSCADCTSStepOpenDelayConfig(SOC_ADC_TSC_0_REGS, STEP_BAT, BAT_OPEN_DELAY);
    TSCADCTSStepSampleDelayConfig(SOC_ADC_TSC_0_REGS, STEP_BAT, 0xFF);

    //Step POT: CHANNEL1 = AIN0 (potentiometer). Measured SECOND.
    TSCADCTSStepConfig(SOC_ADC_TSC_0_REGS, STEP_POT, TSCADC_NEGATIVE_REF_VSSA,
                       TSCADC_POSITIVE_INP_CHANNEL1, TSCADC_NEGATIVE_INP_CHANNEL1,
                       TSCADC_POSITIVE_REF_VDDA);
    TSCADCTSStepOperationModeControl(SOC_ADC_TSC_0_REGS, 0, STEP_POT); //0:single Ended
    TSCADCTSStepFIFOSelConfig(SOC_ADC_TSC_0_REGS, STEP_POT, TSCADC_FIFO_0);
    TSCADCTSStepModeConfig(SOC_ADC_TSC_0_REGS, STEP_POT, TSCADC_ONE_SHOT_SOFTWARE_ENABLED);

    UARTprintf("\n\rStarterWare ADC\n\r");

    while(1){
        if (UARTCharsAvail(SOC_UART_0_REGS)){
            char ch=UARTGetc();   //Blocking function

            //Trigger on Enter (CR) or printable keys; ignore LF, ESC and other control codes
            if (ch != '\r' && (ch < ' ' || ch > '~'))
                continue;
            if (ch != '\r')
                UARTprintf("%c",ch);

            //Empty FIFO_0 so old samples can't shift the order
            while (TSCADCFIFOWordCountRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0))
                TSCADCFIFOADCDataRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0);

            //Enable bit = StepConfig index + 1 (bit 0 is the TS charge step)
            TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, STEP_BAT+1, 1);
            TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, STEP_POT+1, 1);
            TSCADCModuleStateSet(SOC_ADC_TSC_0_REGS, TSCADC_MODULE_ENABLE); //Enable

            //Wait for both samples (with timeout)
            timeout = ADC_TIMEOUT;
            while ((TSCADCFIFOWordCountRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0) < 2) && --timeout);

            TSCADCModuleStateSet(SOC_ADC_TSC_0_REGS, TSCADC_MODULE_DISABLE); //Disable

            if (timeout == 0){
                UARTprintf("\n\rADC timeout (FIFO=%d)\n\r",
                           TSCADCFIFOWordCountRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0));
                TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, STEP_BAT+1, 0);
                TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, STEP_POT+1, 0);
                continue;
            }

            //Same order as the steps: BAT first, POT second
            raw_bat = TSCADCFIFOADCDataRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0) & 0xFFF;
            raw_pot = TSCADCFIFOADCDataRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0) & 0xFFF;

            mv = (int)((raw_bat * 1800u) / 4096u);  //1.8 V ref, 12 bits
            mv = mv * 2;                            //x2: voltage divider

            UARTprintf("\n\rVoltage = %d.%03d V (%d mV); Pot = %d\n\r",
                       mv / 1000, mv % 1000, mv, raw_pot);
        }
    }
    return 0;
}//fin main.c
