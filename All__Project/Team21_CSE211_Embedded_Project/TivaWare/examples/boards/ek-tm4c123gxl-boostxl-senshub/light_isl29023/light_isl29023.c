//*****************************************************************************
//
// light_isl29023.c - Example to use of the SensorLib with the ISL29023
//
// Copyright (c) 2013 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 1.0 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "sensorlib/hw_isl29023.h"
#include "sensorlib/i2cm_drv.h"
#include "sensorlib/isl29023.h"
#include "drivers/rgb.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>Light Measurement with the ISL29023 (light_isl29023)</h1>
//!
//! This example demonstrates the basic use of the Sensor Library, TM4C123G
//! LaunchPad and the SensHub BoosterPack to obtain ambient and infrared light
//! measurements with the ISL29023 sensor.
//!
//! Connect a serial terminal program to the LaunchPad's ICDI virtual serial
//! port at 115,200 baud.  Use eight bits per byte, no parity and one stop bit.
//! The raw sensor measurements are printed to the terminal.  The RGB LED
//! blinks at 1Hz once the initialization is complete and the example is
//! running.
//
//*****************************************************************************

//*****************************************************************************
//
// Define ISL29023 I2C Address.
//
//*****************************************************************************
#define ISL29023_I2C_ADDRESS    0x44

//*****************************************************************************
//
// The system tick rate expressed both as ticks per second and a millisecond
// period.
//
//*****************************************************************************
#define SYSTICKS_PER_SECOND     1
#define SYSTICK_PERIOD_MS       (1000 / SYSTICKS_PER_SECOND)

//*****************************************************************************
//
// Global array for holding the color values for the RGB.
//
//*****************************************************************************
uint32_t g_pui32Colors[3];

//*****************************************************************************
//
// Global instance structure for the I2C master driver.
//
//*****************************************************************************
tI2CMInstance g_sI2CInst;

//*****************************************************************************
//
// Global instance structure for the ISL29023 sensor driver.
//
//*****************************************************************************
tISL29023 g_sISL29023Inst;

//*****************************************************************************
//
// Global flags to alert main that ISL29023 data is ready or an error
// has occurred.
//
//*****************************************************************************
volatile unsigned long g_vui8DataFlag;
volatile unsigned long g_vui8ErrorFlag;
volatile unsigned long g_vui8IntensityFlag;

//*****************************************************************************
//
// Constants to hold the floating point version of the thresholds for each
// range setting. Numbers represent an 81% and 19 % threshold levels. This
// creates a +/- 1% hysteresis band between range adjustments.
//
//*****************************************************************************
const float g_fThresholdHigh[4] =
{
    810.0f, 3240.0f, 12960.0f, 64000.0f
};
const float g_fThresholdLow[4] =
{
    0.0f, 760.0f, 3040.0f, 12160.0f
};

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// ISL29023 Sensor callback function.  Called at the end of ISL29023 sensor
// driver transactions. This is called from I2C interrupt context. Therefore,
// we just set a flag and let main do the bulk of the computations and display.
//
//*****************************************************************************
void
ISL29023AppCallback(void *pvCallbackData, uint_fast8_t ui8Status)
{
    //
    // If the transaction succeeded set the data flag to indicate to
    // application that this transaction is complete and data may be ready.
    //
    if(ui8Status == I2CM_STATUS_SUCCESS)
    {
        g_vui8DataFlag = 1;
    }

    //
    // Store the most recent status in case it was an error condition
    //
    g_vui8ErrorFlag = ui8Status;
}

//*****************************************************************************
//
// Called by the NVIC as a result of I2C3 Interrupt. I2C3 is the I2C connection
// to the ISL29023.
//
//*****************************************************************************
void
ISL29023I2CIntHandler(void)
{
    //
    // Pass through to the I2CM interrupt handler provided by sensor library.
    // This is required to be at application level so that I2CMIntHandler can
    // receive the instance structure pointer as an argument.
    //
    I2CMIntHandler(&g_sI2CInst);
}

//*****************************************************************************
//
// Called by the NVIC as a result of GPIO port E interrupt event. For this
// application GPIO port E pin 5 is the interrupt line for the ISL29023
//
// For this application this is a very low priority interrupt, we want to
// get notification of light values outside our thresholds but it is not the
// most important thing.
//
//*****************************************************************************
void
IntGPIOe(void)
{
    unsigned long ulStatus;

    ulStatus = GPIOIntStatus(GPIO_PORTE_BASE, true);

    //
    // Clear all the pin interrupts that are set
    //
    GPIOIntClear(GPIO_PORTE_BASE, ulStatus);

    if(ulStatus & GPIO_PIN_5)
    {
        //
        // ISL29023 has indicated that the light level has crossed outside of
        // the intensity threshold levels set in INT_LT and INT_HT registers.
        //
        g_vui8IntensityFlag = 1;
    }
}

//*****************************************************************************
//
// Interrupt handler for the system tick counter.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
    //
    // Go get the latest data from the sensor.
    //
    ISL29023DataRead(&g_sISL29023Inst, ISL29023AppCallback, &g_sISL29023Inst);
}

//*****************************************************************************
//
// ISL29023 Application error handler.
//
//*****************************************************************************
void
ISL29023AppErrorHandler(char *pcFilename, uint_fast32_t ui32Line)
{
    //
    // Set terminal color to red and print error status and locations
    //
    UARTprintf("\033[31;1m");
    UARTprintf("Error: %d, File: %s, Line: %d\n"
               "See I2C status definitions in utils\\i2cm_drv.h\n",
               g_vui8ErrorFlag, pcFilename, ui32Line);

    //
    // Return terminal color to normal
    //
    UARTprintf("\033[0m");

    //
    // Set RGB Color to RED
    //
    g_pui32Colors[0] = 0xFFFF;
    g_pui32Colors[1] = 0;
    g_pui32Colors[2] = 0;
    RGBColorSet(g_pui32Colors);

    //
    // Increase blink rate to get attention
    //
    RGBBlinkRateSet(10.0f);

    //
    // Go to sleep wait for interventions.  A more robust application could
    // attempt corrective actions here.
    //
    while(1)
    {
        //
        // Do Nothing
        //
    }
}

//*****************************************************************************
//
// Function to wait for the ISL29023 transactions to complete.
//
//*****************************************************************************
void
ISL29023AppI2CWait(char *pcFilename, uint_fast32_t ui32Line)
{
    //
    // Put the processor to sleep while we wait for the I2C driver to
    // indicate that the transaction is complete.
    //
    while((g_vui8DataFlag == 0) && (g_vui8ErrorFlag == 0))
    {
        //
        // Do Nothing
        //
    }

    //
    // If an error occurred call the error handler immediately.
    //
    if(g_vui8ErrorFlag)
    {
        ISL29023AppErrorHandler(pcFilename, ui32Line);
    }

    //
    // clear the data flag for next use.
    //
    g_vui8DataFlag = 0;
}

//*****************************************************************************
//
// Intensity and Range Tracking Function.  This adjusts the range and interrupt
// thresholds as needed.  Uses an 80/20 rule. If light is greather then 80% of
// maximum value in this range then go to next range up. If less than 20% of
// potential value in this range go the next range down.
//
//*****************************************************************************
void
ISL29023AppAdjustRange(tISL29023 *pInst)
{
    float fAmbient;
    uint8_t ui8NewRange;

    ui8NewRange = g_sISL29023Inst.ui8Range;

    //
    // Get a local floating point copy of the latest light data
    //
    ISL29023DataLightVisibleGetFloat(&g_sISL29023Inst, &fAmbient);

    //
    // Check if we crossed the upper threshold.
    //
    if(fAmbient > g_fThresholdHigh[g_sISL29023Inst.ui8Range])
    {
        //
        // The current intensity is over our threshold so adjsut the range
        // accordingly
        //
        if(g_sISL29023Inst.ui8Range < ISL29023_CMD_II_RANGE_64K)
        {
            ui8NewRange = g_sISL29023Inst.ui8Range + 1;
        }
    }

    //
    // Check if we crossed the lower threshold
    //
    if(fAmbient < g_fThresholdLow[g_sISL29023Inst.ui8Range])
    {
        //
        // If possible go to the next lower range setting and reconfig the
        // thresholds.
        //
        if(g_sISL29023Inst.ui8Range > ISL29023_CMD_II_RANGE_1K)
        {
            ui8NewRange = g_sISL29023Inst.ui8Range - 1;
        }
    }

    //
    // If the desired range value changed then send the new range to the sensor
    //
    if(ui8NewRange != g_sISL29023Inst.ui8Range)
    {
        ISL29023ReadModifyWrite(&g_sISL29023Inst, ISL29023_O_CMD_II,
                                ~ISL29023_CMD_II_RANGE_M, ui8NewRange,
                                ISL29023AppCallback, &g_sISL29023Inst);

        //
        // Wait for transaction to complete
        //
        ISL29023AppI2CWait(__FILE__, __LINE__);
    }
}

//*****************************************************************************
//
// Configure the UART and its pins.  This must be called before UARTprintf().
//
//*****************************************************************************
void
ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
    ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}

//*****************************************************************************
//
// Main 'C' Language entry point.
//
//*****************************************************************************
int
main(void)
{
    float fAmbient;
    int32_t i32IntegerPart, i32FractionPart;
    uint8_t ui8Mask;

    //
    // Setup the system clock to run at 40 Mhz from PLL with crystal reference
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    //
    // Enable the peripherals used by this example.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    //
    // Initialize the UART and its pins.
    //
    ConfigureUART();

    //
    // Print the welcome message to the terminal.
    //
    UARTprintf("\033[2JISL29023 Example\n");

    //
    // Set the color to a white approximation.
    //
    g_pui32Colors[RED] = 0x8000;
    g_pui32Colors[BLUE] = 0x8000;
    g_pui32Colors[GREEN] = 0x8000;

    //
    // Initialize RGB driver. Use a default intensity and blink rate.
    //
    RGBInit(0);
    RGBColorSet(g_pui32Colors);
    RGBIntensitySet(0.5f);
    RGBEnable();

    //
    // The I2C3 peripheral must be enabled before use.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C3);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

    //
    // Configure the pin muxing for I2C3 functions on port D0 and D1.
    // This step is not necessary if your part does not support pin muxing.
    //
    ROM_GPIOPinConfigure(GPIO_PD0_I2C3SCL);
    ROM_GPIOPinConfigure(GPIO_PD1_I2C3SDA);

    //
    // Select the I2C function for these pins.  This function will also
    // configure the GPIO pins pins for I2C operation, setting them to
    // open-drain operation with weak pull-ups.  Consult the data sheet
    // to see which functions are allocated per pin.
    //
    GPIOPinTypeI2CSCL(GPIO_PORTD_BASE, GPIO_PIN_0);
    ROM_GPIOPinTypeI2C(GPIO_PORTD_BASE, GPIO_PIN_1);

    //
    // Configure and Enable the GPIO interrupt. Used for INT signal from the
    // ISL29023
    //
    ROM_GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_5);
    GPIOIntEnable(GPIO_PORTE_BASE, GPIO_PIN_5);
    ROM_GPIOIntTypeSet(GPIO_PORTE_BASE, GPIO_PIN_5, GPIO_FALLING_EDGE);
    ROM_IntEnable(INT_GPIOE);

    //
    // Keep only some parts of the systems running while in sleep mode.
    // GPIOE is for the ISL29023 interrupt pin.
    // UART0 is the virtual serial port
    // TIMER0, TIMER1 and WTIMER5 are used by the RGB driver
    // I2C3 is the I2C interface to the ISL29023
    //
    ROM_SysCtlPeripheralClockGating(true);
    ROM_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOE);
    ROM_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);
    ROM_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER0);
    ROM_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER1);
    ROM_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_I2C3);
    ROM_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_WTIMER5);

    //
    // Configure desired interrupt priorities.  Setting the I2C interrupt to be
    // of more priority than SysTick and the GPIO interrupt means those
    // interrupt routines can use the I2CM_DRV Application context does not use
    // I2CM_DRV API and GPIO and SysTick are at the same priority level. This
    // prevents re-entrancy problems with I2CM_DRV but keeps the MCU in sleep
    // state as much as possible. UART is at least priority so it can operate
    // in the background.
    //
    ROM_IntPrioritySet(INT_I2C3, 0x00);
    ROM_IntPrioritySet(FAULT_SYSTICK, 0x40);
    ROM_IntPrioritySet(INT_GPIOE, 0x80);
    ROM_IntPrioritySet(INT_UART0, 0x80);

    //
    // Enable interrupts to the processor.
    //
    ROM_IntMasterEnable();

    //
    // Initialize I2C3 peripheral.
    //
    I2CMInit(&g_sI2CInst, I2C3_BASE, INT_I2C3, 0xff, 0xff,
             ROM_SysCtlClockGet());

    //
    // Initialize the ISL29023 Driver.
    //
    ISL29023Init(&g_sISL29023Inst, &g_sI2CInst, ISL29023_I2C_ADDRESS,
                 ISL29023AppCallback, &g_sISL29023Inst);

    //
    // Wait for transaction to complete
    //
    ISL29023AppI2CWait(__FILE__, __LINE__);

    //
    // Configure the ISL29023 to measure ambient light continuously. Set a 8
    // sample persistence before the INT pin is asserted. Clears the INT flag.
    // Persistence setting of 8 is sufficient to ignore camera flashes.
    //
    ui8Mask = (ISL29023_CMD_I_OP_MODE_M | ISL29023_CMD_I_INT_PERSIST_M |
               ISL29023_CMD_I_INT_FLAG_M);
    ISL29023ReadModifyWrite(&g_sISL29023Inst, ISL29023_O_CMD_I, ~ui8Mask,
                            (ISL29023_CMD_I_OP_MODE_ALS_CONT |
                             ISL29023_CMD_I_INT_PERSIST_8),
                            ISL29023AppCallback, &g_sISL29023Inst);

    //
    // Wait for transaction to complete
    //
    ISL29023AppI2CWait(__FILE__, __LINE__);

    //
    // Configure the upper threshold to 80% of maximum value
    //
    g_sISL29023Inst.pui8Data[1] = 0xCC;
    g_sISL29023Inst.pui8Data[2] = 0xCC;
    ISL29023Write(&g_sISL29023Inst, ISL29023_O_INT_HT_LSB,
                  g_sISL29023Inst.pui8Data, 2, ISL29023AppCallback,
                  &g_sISL29023Inst);

    //
    // Wait for transaction to complete
    //
    ISL29023AppI2CWait(__FILE__, __LINE__);

    //
    // Configure the lower threshold to 20% of maximum value
    //
    g_sISL29023Inst.pui8Data[1] = 0x33;
    g_sISL29023Inst.pui8Data[2] = 0x33;
    ISL29023Write(&g_sISL29023Inst, ISL29023_O_INT_LT_LSB,
                  g_sISL29023Inst.pui8Data, 2, ISL29023AppCallback,
                  &g_sISL29023Inst);
    //
    // Wait for transaction to complete
    //
    ISL29023AppI2CWait(__FILE__, __LINE__);

    //
    //Configure and enable SysTick Timer
    //
    ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / SYSTICKS_PER_SECOND);
    ROM_SysTickIntEnable();
    ROM_SysTickEnable();

    //
    // After all the init and config we start blink the LED
    //
    RGBBlinkRateSet(1.0f);

    //
    // Loop Forever
    //
    while(1)
    {
        ROM_SysCtlSleep();

        if(g_vui8DataFlag)
        {
            g_vui8DataFlag = 0;

            //
            // Get a local floating point copy of the latest light data
            //
            ISL29023DataLightVisibleGetFloat(&g_sISL29023Inst, &fAmbient);

            //
            // Perform the conversion from float to a printable set of integers
            //
            i32IntegerPart = (int32_t)fAmbient;
            i32FractionPart = (int32_t)(fAmbient * 1000.0f);
            i32FractionPart = i32FractionPart - (i32IntegerPart * 1000);
            if(i32FractionPart < 0)
            {
                i32FractionPart *= -1;
            }

            //
            // Print the temperature as integer and fraction parts.
            //
            UARTprintf("Visible Lux: %3d.%03d\n", i32IntegerPart,
                       i32FractionPart);

            //
            // Check if the intensity of light has crossed a threshold. If so
            // then adjust range of sensor readings to track intensity.
            //
            if(g_vui8IntensityFlag)
            {
                //
                // Disable the low priority interrupts leaving only the I2C
                // interrupt enabled.
                //
                ROM_IntPriorityMaskSet(0x40);

                //
                // Reset the intensity trigger flag.
                //
                g_vui8IntensityFlag = 0;

                //
                // Adjust the lux range.
                //
                ISL29023AppAdjustRange(&g_sISL29023Inst);

                //
                // Now we must manually clear the flag in the ISL29023
                // register.
                //
                ISL29023Read(&g_sISL29023Inst, ISL29023_O_CMD_I,
                             g_sISL29023Inst.pui8Data, 1, ISL29023AppCallback,
                             &g_sISL29023Inst);

                //
                // Wait for transaction to complete
                //
                ISL29023AppI2CWait(__FILE__, __LINE__);

                //
                // Disable priority masking so all interrupts are enabled.
                //
                ROM_IntPriorityMaskSet(0);
            }
        }
    }
}
