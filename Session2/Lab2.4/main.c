#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "uartStdio.h"
#include "uart_irda_cir.h"   /* UARTCharsAvail() prototype */
#include "hsi2c.h"

/* Both sensors share the same bus: I2C1 (P9.17 SCL, P9.18 SDA) */
#define I2C_BASE      SOC_I2C_1_REGS

#define ADD_TEMP      0x49      /* Temperature sensor */
#define REG_TEMP      0x00      /* 2 bytes, MSB first, 0.125 degC/LSB */

#define ADD_ACEL      0x53      /* ADXL345 (0x1D if SDO = 3.3 V) */
#define POWER_CTL     0x2D
#define DATAX0        0x32      /* X0,X1,Y0,Y1,Z0,Z1: 6 bytes, LSB first */

unsigned char txBuf[20];
unsigned char rxBuf[20];

/* Write phase: START + addr(W) + dcount bytes, NO stop (repeated start or STOP follows) */
void TxI2C1(int dcount)
{
    int i;

    while (I2CMasterBusBusy(I2C_BASE));          /* wait bus free */
    I2CMasterIntClearEx(I2C_BASE, 0x7FF);        /* clear old status flags */

    I2CSetDataCount(I2C_BASE, dcount);
    I2CMasterControl(I2C_BASE, I2C_CFG_MST_TX);
    I2CMasterStart(I2C_BASE);

    for (i = 0; i < dcount; i++) {
        while (!(I2CMasterIntRawStatus(I2C_BASE) & I2C_INT_TRANSMIT_READY));
        I2CMasterDataPut(I2C_BASE, txBuf[i]);
        I2CMasterIntClearEx(I2C_BASE, I2C_INT_TRANSMIT_READY);
    }

    /* ARDY: all bytes sent, bus still held (no STP) */
    while (!(I2CMasterIntRawStatus(I2C_BASE) & I2C_INT_ADRR_READY_ACESS));
    I2CMasterIntClearEx(I2C_BASE, I2C_INT_ADRR_READY_ACESS);
}

/* Read phase: repeated START + addr(R) + dcount bytes + STOP */
void RxI2C1(int dcount)
{
    int rc = 0;

    I2CSetDataCount(I2C_BASE, dcount);
    I2CMasterControl(I2C_BASE, I2C_CFG_MST_RX);
    I2CMasterStart(I2C_BASE);
    I2CMasterStop(I2C_BASE);     /* STP armed: STOP is sent after the last byte */

    while (dcount--) {
        while (!(I2CMasterIntRawStatus(I2C_BASE) & I2C_INT_RECV_READY));
        rxBuf[rc++] = I2CMasterDataGet(I2C_BASE);
        I2CMasterIntClearEx(I2C_BASE, I2C_INT_RECV_READY);
    }
}

/* Read n registers of slave 'addr' starting at 'reg' into rxBuf */
void readRegs(unsigned char addr, unsigned char reg, int n)
{
    I2CMasterSlaveAddrSet(I2C_BASE, addr);
    txBuf[0] = reg;
    TxI2C1(1);
    RxI2C1(n);
}

int main(void)
{
    int temp, x, y, z;

    UARTStdioInit();             /* UART0 */
    UARTprintf("\r\nStarterWare I2C - Temperature + Accelerometer\r\n");

    I2C1ModuleClkConfig();
    I2CPinMuxSetup(1);                       /* I2C1: P9.17 SCL, P9.18 SDA */
    I2CMasterDisable(I2C_BASE);
    I2CAutoIdleDisable(I2C_BASE);
    I2CMasterInitExpClk(I2C_BASE, 48000000, 12000000, 100000);   /* 100 kHz */
    I2CMasterEnable(I2C_BASE);

    /* ADXL345: POWER_CTL = 0x08 (Measure). Without this X/Y/Z stay at 0 */
    I2CMasterSlaveAddrSet(I2C_BASE, ADD_ACEL);
    txBuf[0] = POWER_CTL;
    txBuf[1] = 0x08;
    TxI2C1(2);
    I2CMasterStop(I2C_BASE);

    UARTprintf("Press any key to read both sensors\r\n");

    while (1) {
        if (UARTCharsAvail(SOC_UART_0_REGS)) {
            (void)UARTGetc();

            /* Temperature: 11-bit signed, x 0.125 for degC */
            readRegs(ADD_TEMP, REG_TEMP, 2);
            temp = (short)((rxBuf[0] << 8) | rxBuf[1]) >> 5;

            /* Accelerometer: 16-bit signed per axis, LSB first */
            readRegs(ADD_ACEL, DATAX0, 6);
            x = (short)((rxBuf[1] << 8) | rxBuf[0]);
            y = (short)((rxBuf[3] << 8) | rxBuf[2]);
            z = (short)((rxBuf[5] << 8) | rxBuf[4]);

            UARTprintf("Temp: %d  Accel: X=%d Y=%d Z=%d\r\n", temp, x, y, z);
        }
    }
    return 0;
}
