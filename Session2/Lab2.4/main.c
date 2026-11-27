#include "soc_AM335x.h"
#include "gpio_v2.h"
#include "uartStdio.h"
#include "uart_irda_cir.h"   /* UARTCharsAvail() prototype */
#include "hsi2c.h"

/* Both sensors share the same bus: I2C1 (P9.17 SCL, P9.18 SDA) */
#define I2C_BASE      SOC_I2C_1_REGS
#define I2C_TIMEOUT   200000u   /* loop iterations, not a time unit */

/* Temperature sensor (Lab2.2) */
#define ADD_TEMP      0x49      /* LM75-type sensor */
#define REG_TEMP      0x00      /* Temperature register, 2 bytes, MSB first */

/* Accelerometer ADXL345 (Lab2.3) */
#define ADD_ACEL      0x53      /* SDO/ALT_ADDRESS = GND (0x1D if SDO = 3.3 V) */
#define DEVID         0x00      /* expected 0xE5 */
#define POWER_CTL     0x2D
#define DATA_FORMAT   0x31
#define DATAX0        0x32      /* X0,X1,Y0,Y1,Z0,Z1: 6 bytes, LSB first */
#define ADXL_DEVID    0xE5

/* Error codes */
#define I2C_OK         0
#define I2C_ERR_NACK  -1
#define I2C_ERR_ARB   -2
#define I2C_ERR_TOUT  -3
#define I2C_ERR_BUSY  -4

unsigned char txBuf[20];
unsigned char rxBuf[20];

/* ------------------------------------------------------------------ */
/* Low-level I2C1 helpers (same as Lab2.3)                             */
/* ------------------------------------------------------------------ */

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

/* Write phase: START + addr(W) + dcount bytes, NO stop (repeated start or STOP follows) */
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

/* ------------------------------------------------------------------ */
/* Register-level helpers: pick the slave, then write / read           */
/* ------------------------------------------------------------------ */

/* Write one register: START + addr(W) + reg + val + STOP */
static int i2cWriteReg(unsigned char addr, unsigned char reg, unsigned char val)
{
    int r;

    I2CMasterSlaveAddrSet(I2C_BASE, addr);   /* bus is idle here, safe to change SA */
    txBuf[0] = reg;
    txBuf[1] = val;
    r = TxI2C1(2);
    if (r != I2C_OK) return r;

    I2CMasterStop(I2C_BASE);                 /* close the write with a STOP */
    r = i2cWaitBusFree();
    i2cClearAll();
    return r;
}

/* Read n registers starting at 'reg' into rxBuf (register pointer + repeated START) */
static int i2cReadRegs(unsigned char addr, unsigned char reg, int n)
{
    int r;

    I2CMasterSlaveAddrSet(I2C_BASE, addr);
    txBuf[0] = reg;
    r = TxI2C1(1);
    if (r == I2C_OK) r = RxI2C1(n);
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

/* ------------------------------------------------------------------ */
/* Sensors                                                             */
/* ------------------------------------------------------------------ */

/* ADXL345: check DEVID, full resolution +-2 g (3.9 mg/LSB), measurement mode */
static int accelInit(void)
{
    int r;

    if ((r = i2cReadRegs(ADD_ACEL, DEVID, 1)) != I2C_OK) return r;
    UARTprintf("Accel ADXL345 ID=0x%x%s\r\n", rxBuf[0],
               (rxBuf[0] == ADXL_DEVID) ? "" : "  (expected 0xE5!)");

    if ((r = i2cWriteReg(ADD_ACEL, DATA_FORMAT, 0x08)) != I2C_OK) return r;  /* FULL_RES, +-2 g */
    if ((r = i2cWriteReg(ADD_ACEL, POWER_CTL,   0x08)) != I2C_OK) return r;  /* Measure = 1 */
    return I2C_OK;
}

/* Temperature in 1/8 degC units (11-bit two's complement, 0.125 degC/LSB) */
static int readTemp(int *t8)
{
    int r = i2cReadRegs(ADD_TEMP, REG_TEMP, 2);
    if (r != I2C_OK) return r;

    short raw = (short)((rxBuf[0] << 8) | rxBuf[1]);   /* [0]:MSB [1]:LSB */
    *t8 = raw >> 5;                                     /* arithmetic shift keeps the sign */
    return I2C_OK;
}

/* Raw X, Y, Z counts (16-bit two's complement, LSB first) */
static int readAccel(int *x, int *y, int *z)
{
    int r = i2cReadRegs(ADD_ACEL, DATAX0, 6);           /* multi-byte read auto-increments */
    if (r != I2C_OK) return r;

    *x = (short)((rxBuf[1] << 8) | rxBuf[0]);
    *y = (short)((rxBuf[3] << 8) | rxBuf[2]);
    *z = (short)((rxBuf[5] << 8) | rxBuf[4]);
    return I2C_OK;
}

/* Prints a value given in 1/8 degC as  [-]int.ddd  (UARTprintf has no %f) */
static void printTemp(int t8)
{
    int milli, ip, fp;

    if (t8 < 0) { UARTprintf("-"); t8 = -t8; }
    milli = t8 * 125;            /* 1/8 degC -> mdegC */
    ip = milli / 1000;
    fp = milli % 1000;           /* multiple of 125: 0, 125, ..., 875 */
    UARTprintf("%d.%d%d%d C", ip, fp / 100, (fp / 10) % 10, fp % 10);
}

/* Raw count -> mg in full-res mode (3.9 mg/LSB) */
static int toMg(int raw)
{
    return (raw * 39) / 10;
}

/* ------------------------------------------------------------------ */

int main(void)
{
    int r, t8, x, y, z;

    UARTStdioInit();             /* UART0 */
    UARTprintf("\r\nStarterWare I2C - Temperature + Accelerometer\r\n");

    I2C1ModuleClkConfig();
    I2CPinMuxSetup(1);                       /* I2C1: P9.17 SCL, P9.18 SDA */
    I2CMasterDisable(I2C_BASE);
    I2CAutoIdleDisable(I2C_BASE);
    I2CMasterInitExpClk(I2C_BASE, 48000000, 12000000, 100000);   /* 100 kHz */
    I2CMasterEnable(I2C_BASE);
    UARTprintf("I2C1 configured\r\n");

    r = accelInit();
    if (r != I2C_OK)
        UARTprintf("Accel init error %d: %s\r\n", r, i2cErrStr(r));

    UARTprintf("Press any key to read both sensors\r\n");

    while (1) {
        if (UARTCharsAvail(SOC_UART_0_REGS)) {
            (void)UARTGetc();

            /* Temperature */
            r = readTemp(&t8);
            if (r == I2C_OK) {
                UARTprintf("Temp: ");
                printTemp(t8);
                UARTprintf("  (raw %d)\r\n", t8);
            } else {
                UARTprintf("Temp I2C error %d: %s\r\n", r, i2cErrStr(r));
            }

            /* Accelerometer */
            r = readAccel(&x, &y, &z);
            if (r == I2C_OK)
                UARTprintf("Accel: X=%d Y=%d Z=%d mg  (raw %d %d %d)\r\n",
                           toMg(x), toMg(y), toMg(z), x, y, z);
            else
                UARTprintf("Accel I2C error %d: %s\r\n", r, i2cErrStr(r));
        }
    }
    return 0;
}
