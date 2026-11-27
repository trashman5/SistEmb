#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "uartStdio.h"
#include "hsi2c.h"
#define ADD_TEMP  0x49    //Address Temp
#define REG_TEMP  0x00

unsigned char txBuf[20];
unsigned char rxBuf[20];
void TxI2C1(int dcount);
void RxI2C1(int dcount);
int main(void)   //I2C
{
    int dcount=0;
    UARTStdioInit(); //Uart0 enable
    //I2C1 Config
    I2C1ModuleClkConfig();
    I2CPinMuxSetup(1);       //I2C1: P9.17 (SCL) y P9.18(SDA)
    I2CMasterDisable(SOC_I2C_1_REGS);
    I2CAutoIdleDisable(SOC_I2C_1_REGS);
    I2CMasterInitExpClk(SOC_I2C_1_REGS, 48000000, 12000000, 100000); //100khz
    I2CMasterSlaveAddrSet(SOC_I2C_1_REGS, ADD_TEMP);
    I2CMasterEnable(SOC_I2C_1_REGS);

    UARTprintf("\nStarterWare I2C");
    while(1){
        if (UARTCharsAvail(SOC_UART_0_REGS)){
            char ch=UARTGetc();   //Blocking function

            txBuf[0]=REG_TEMP;   //Get Temp ren Reg=0x00
            TxI2C1(1);
            dcount = 2;
            RxI2C1(dcount);       //Wait for 2 bytes in rxBuf
            int temp=(rxBuf[0]<<8) + rxBuf[1]; //[0]:MSB Byte [1]:LSB
            temp = temp>>5;       //Temp x 0.125 para grados
            UARTprintf("\n\rTemp Sensor: %d",temp);
        }
    }
    return 0;
}

void TxI2C1(int dcount)
{
    int i=0;
    I2CSetDataCount(SOC_I2C_1_REGS, dcount);
    I2CMasterControl(SOC_I2C_1_REGS, I2C_CFG_MST_TX);
    I2CMasterStart(SOC_I2C_1_REGS);
    while(I2CMasterBusBusy(SOC_I2C_1_REGS) == 0); //wait bus is free*
    for (i=0;i<dcount;i++) {
        I2CMasterDataPut(SOC_I2C_1_REGS, txBuf[i]);   //use txBuf
    }
    while(I2CDataCountGet(SOC_I2C_1_REGS)>0); //wait to tx end
    I2CMasterStop(SOC_I2C_1_REGS);
}

void RxI2C1(int dcount)
{
    int rc=0;

    I2CSetDataCount(SOC_I2C_1_REGS, dcount); //wait for 'dcount' bytes
    I2CMasterControl(SOC_I2C_1_REGS, I2C_CFG_MST_RX); //Rx mode
    I2CMasterStart(SOC_I2C_1_REGS);

    while((dcount--)){
        while(0 == (I2CMasterIntRawStatus(SOC_I2C_1_REGS) & I2C_INT_RECV_READY));
        rxBuf[rc++] = I2CMasterDataGet(SOC_I2C_1_REGS);
        I2CMasterIntClearEx(SOC_I2C_1_REGS, I2C_INT_RECV_READY);
    }
    I2CMasterStop(SOC_I2C_1_REGS);
}
