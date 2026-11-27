#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "uartStdio.h"
#include "uart_irda_cir.h"   /* UARTCharsAvail() prototype */
#include "hsi2c.h"

#define ADD_ACEL      0x53      /* ADXL345 with SDO/ALT_ADDRESS = GND (0x1D if SDO = 3.3 V) */
#define DEVID         0x00      /* Register DEVID, expected 0xE5 */
#define I2C_BASE      SOC_I2C_1_REGS
#define I2C_TIMEOUT   200000u   /* loop iterations, not a time unit */

/* Error codes */
#define I2C_OK         0
#define I2C_ERR_NACK  -1
#define I2C_ERR_ARB   -2
#define I2C_ERR_TOUT  -3
#define I2C_ERR_BUSY  -4

unsigned char txBuf[20];
unsigned char rxBuf[20];

static void i2cClearAll(void)
{
    I2CMasterIntClearEx(I2C_BASE, 0x7FF);     /* clear every W1C status bit */
}

/* Waits for 'flag' in IRQSTATUS_RAW; aborts on NACK, arbitration lost or timeout */
static int i2cWait(unsigned int flag)
{
    unsigned int st, t = I2C_TIMEOUT;
    do {
        st = I2CMasterIntRawStatus(I2C_BASE);
        if (st & I2C_INT_NO_ACK)           return I2C_ERR_NACK;
        if (st & I2C_INT_ARBITRATION_LOST) return I2C_ERR_ARB;
        if (st & flag)                     return I2C_OK;
    } while (--t);
    return I2C_ERR_TOUT;
}

static int i2cWaitBusFree(void)
{
    unsigned int t = I2C_TIMEOUT;
    while (I2CMasterBusBusy(I2C_BASE)) {
        if (--t == 0) return I2C_ERR_BUSY;
    }
    return I2C_OK;
}

static void i2cAbort(void)
{
    I2CMasterStop(I2C_BASE);
    i2cWaitBusFree();
    i2cClearAll();
}

/* Write phase: START + addr(W) + dcount bytes, NO stop (repeated start follows) */
int TxI2C1(int dcount)
{
    int i, r;

    if ((r = i2cWaitBusFree()) != I2C_OK) return r;
    i2cClearAll();

    I2CSetDataCount(I2C_BASE, dcount);
    I2CMasterControl(I2C_BASE, I2C_CFG_MST_TX);
    I2CMasterStart(I2C_BASE);

    for (i = 0; i < dcount; i++) {
        if ((r = i2cWait(I2C_INT_TRANSMIT_READY)) != I2C_OK) { i2cAbort(); return r; }
        I2CMasterDataPut(I2C_BASE, txBuf[i]);
        I2CMasterIntClearEx(I2C_BASE, I2C_INT_TRANSMIT_READY);
    }

    /* ARDY: all dcount bytes sent and ACKed, bus still held (no STP) */
    if ((r = i2cWait(I2C_INT_ADRR_READY_ACESS)) != I2C_OK) { i2cAbort(); return r; }
    I2CMasterIntClearEx(I2C_BASE, I2C_INT_ADRR_READY_ACESS);
    return I2C_OK;
}

/* Read phase: repeated START + addr(R) + dcount bytes + STOP */
int RxI2C1(int dcount)
{
    int rc = 0, r;

    I2CSetDataCount(I2C_BASE, dcount);
    I2CMasterControl(I2C_BASE, I2C_CFG_MST_RX);
    I2CMasterStart(I2C_BASE);
    I2CMasterStop(I2C_BASE);     /* STP armed now: controller NACKs last byte and stops when DCOUNT = 0 */

    while (dcount--) {
        if ((r = i2cWait(I2C_INT_RECV_READY)) != I2C_OK) { i2cAbort(); return r; }
        rxBuf[rc++] = I2CMasterDataGet(I2C_BASE);
        I2CMasterIntClearEx(I2C_BASE, I2C_INT_RECV_READY);
    }

    r = i2cWaitBusFree();        /* wait until STOP is on the bus */
    i2cClearAll();
    return r;
}

static const char *i2cErrStr(int r)
{
    switch (r) {
        case I2C_ERR_NACK: return "NACK (wrong address / sensor not answering)";
        case I2C_ERR_ARB:  return "arbitration lost (check pull-ups / wiring)";
        case I2C_ERR_TOUT: return "timeout";
        case I2C_ERR_BUSY: return "bus stuck busy (SDA/SCL held low?)";
        default:           return "unknown";
    }
}

int main(void)
{
    int r;

    UARTStdioInit();
    UARTprintf("\r\nStarterWare I2C - press any key to read DEVID\r\n");

    I2C1ModuleClkConfig();
    I2CPinMuxSetup(1);                       /* I2C1: P9.17 SCL, P9.18 SDA */
    I2CMasterDisable(I2C_BASE);
    I2CAutoIdleDisable(I2C_BASE);
    I2CMasterInitExpClk(I2C_BASE, 48000000, 12000000, 100000);   /* 100 kHz */
    I2CMasterSlaveAddrSet(I2C_BASE, ADD_ACEL);
    I2CMasterEnable(I2C_BASE);
    UARTprintf("I2C1 configured\r\n");

    while (1) {
        if (UARTCharsAvail(SOC_UART_0_REGS)) {
            (void)UARTGetc();

            txBuf[0] = DEVID;
            r = TxI2C1(1);
            if (r == I2C_OK) r = RxI2C1(1);

            if (r == I2C_OK)
                UARTprintf("Accel ADXL345 ID=0x%x\r\n", rxBuf[0]);
            else
                UARTprintf("I2C error %d: %s\r\n", r, i2cErrStr(r));
        }
    }
    return 0;
}
