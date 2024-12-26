


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"



#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>  // Ensure snprintf is declared
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_uart.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "tm4c123gh6pm.h"

#define MAGNETIC_SWITCH_PIN GPIO_PIN_0  // PD0 for door sensor input
#define RED_LED GPIO_PIN_1  // PF1 as Red LED
#define BLUE_LED GPIO_PIN_2  // PF2 as Blue LED
#define BUTTON GPIO_PIN_4  // PF4 as Push Button (active low)

void PortD_Init(void);
void PortF_Init(void);
void UART0_Init(void);
void delay_ms(uint32_t delay);

#define SENSOR_PIN GPIO_PIN_0
#define SENSOR_PORT GPIO_PORTB_BASE
#define ALARM_PIN GPIO_PIN_2      // Updated to Pin 1
#define ALARM_PORT GPIO_PORTD_BASE // Updated to Port D
#define BUFFER_SIZE 32

volatile uint32_t temperatureThreshold = 99;
volatile bool alarmActive = false;
volatile bool alarmOverride = false; // New flag to track manual override


void InitializeSensor(void);
void InitializeAlarm(void);
void MicrosecondDelay(uint32_t microseconds);
void MillisecondDelay(uint32_t milliseconds);
bool ReadSensorData(uint8_t *temperature, uint8_t *humidity);
void TransmitTemperature(uint8_t temperature);
void HandleTemperature(void);
void StopAlarm(void);


void PortD_Init(void) {
    // Enable clock for Port D
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD));

    // Configure PD0 as input with pull-up resistor
    GPIOPinTypeGPIOInput(GPIO_PORTD_BASE, MAGNETIC_SWITCH_PIN);
    GPIOPadConfigSet(GPIO_PORTD_BASE, MAGNETIC_SWITCH_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void PortF_Init(void) {
    // Enable clock for Port F
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));

    // Unlock PF4 (required for PF4 configuration)
    GPIOUnlockPin(GPIO_PORTF_BASE, BUTTON);

    // Configure PF1 (Red LED) and PF2 (Blue LED) as outputs
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, RED_LED | BLUE_LED);

    // Configure PF4 (Button) as input with pull-up resistor
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, BUTTON);
    GPIOPadConfigSet(GPIO_PORTF_BASE, BUTTON, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void UART0_Init(void) {
    // Enable UART0 and GPIOA peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));

    // Configure PA0 (U0RX) and PA1 (U0TX)
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure UART0
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    UARTEnable(UART0_BASE);
}

void delay_ms(uint32_t delay) {
    SysCtlDelay((SysCtlClockGet() / 3000) * delay); // Delay for ~1 ms per unit
}



// Sensor Initialization
void InitializeSensor(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB)) {}
    GPIOPinTypeGPIOOutput(SENSOR_PORT, SENSOR_PIN);
    GPIOPinWrite(SENSOR_PORT, SENSOR_PIN, SENSOR_PIN);
}

// Alarm Initialization
void InitializeAlarm(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD); // Enable Port D
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD)) {}
    GPIOPinTypeGPIOOutput(ALARM_PORT, ALARM_PIN); // Set Port D, Pin 1 as output
    GPIOPinWrite(ALARM_PORT, ALARM_PIN, 0);      // Ensure alarm is off initially
}

// Microsecond Delay
void MicrosecondDelay(uint32_t microseconds) {
    SysCtlDelay((SysCtlClockGet() / 3 / 1000000) * microseconds);
}

// Millisecond Delay
void MillisecondDelay(uint32_t milliseconds) {
    SysCtlDelay((SysCtlClockGet() / 3 / 1000) * milliseconds);
}

// Transmit Temperature to UART
void TransmitTemperature(uint8_t temperature) {
    char transmitBuffer[20];
    snprintf(transmitBuffer, sizeof(transmitBuffer), "T:%d\r\n", temperature);
    for (int i = 0; transmitBuffer[i] != '\0'; i++) {
        UARTCharPut(UART0_BASE, transmitBuffer[i]);
    }
}

// Stop the alarm and send a status message
void StopAlarm(void) {
    GPIOPinWrite(ALARM_PORT, ALARM_PIN, 0); // Turn off the alarm
    alarmActive = false;
    alarmOverride = true; // Prevent the alarm from being reactivated
    char alarmMsg[] = "Alarm: Deactivated\r\n";
    for (int i = 0; alarmMsg[i] != '\0'; i++) {
        UARTCharPut(UART0_BASE, alarmMsg[i]);
    }
}

// Read Temperature and Control Alarm
void HandleTemperature(void) {
    uint8_t currentTemperature, currentHumidity;
    if (ReadSensorData(&currentTemperature, &currentHumidity)) {
        TransmitTemperature(currentTemperature);

        // Only activate the alarm if it's not manually overridden
        if (!alarmOverride) {
            if (currentTemperature > temperatureThreshold && !alarmActive) {
                GPIOPinWrite(ALARM_PORT, ALARM_PIN, ALARM_PIN); // Turn on alarm
                alarmActive = true;
                char alarmMsg[] = "Alarm: Activated\r\n";
                for (int i = 0; alarmMsg[i] != '\0'; i++) {
                    UARTCharPut(UART0_BASE, alarmMsg[i]);
                }
            } else if (currentTemperature <= temperatureThreshold && alarmActive) {
                GPIOPinWrite(ALARM_PORT, ALARM_PIN, 0); // Turn off alarm
                alarmActive = false;
                char alarmMsg[] = "Alarm: Deactivated\r\n";
                for (int i = 0; alarmMsg[i] != '\0'; i++) {
                    UARTCharPut(UART0_BASE, alarmMsg[i]);
                }
            }
        }
    }
}

// Read Sensor Data
bool ReadSensorData(uint8_t *temperature, uint8_t *humidity) {
    uint8_t data[5] = {0};
    int bitCounter = 7, byteCounter = 0;

    GPIOPinTypeGPIOOutput(SENSOR_PORT, SENSOR_PIN);
    GPIOPinWrite(SENSOR_PORT, SENSOR_PIN, 0);
    MillisecondDelay(18);
    GPIOPinWrite(SENSOR_PORT, SENSOR_PIN, SENSOR_PIN);
    MicrosecondDelay(20);
    GPIOPinTypeGPIOInput(SENSOR_PORT, SENSOR_PIN);

    if (GPIOPinRead(SENSOR_PORT, SENSOR_PIN)) return false;
    MicrosecondDelay(80);
    if (!GPIOPinRead(SENSOR_PORT, SENSOR_PIN)) return false;
    MicrosecondDelay(80);

    for (int i = 0; i < 40; i++) {
        while (!GPIOPinRead(SENSOR_PORT, SENSOR_PIN));
        MicrosecondDelay(30);
        if (GPIOPinRead(SENSOR_PORT, SENSOR_PIN)) {
            data[byteCounter] |= (1 << bitCounter);
        }
        while (GPIOPinRead(SENSOR_PORT, SENSOR_PIN));
        if (bitCounter == 0) {
            bitCounter = 7;
            byteCounter++;
        } else {
            bitCounter--;
        }
    }

    if (data[0] + data[1] + data[2] + data[3] != data[4]) return false;

    *humidity = data[0];
    *temperature = data[2];
    return true;
}

// Main Function
int main(void) {
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    PortD_Init();     // Initialize Port D for magnetic switch
    PortF_Init();     // Initialize Port F for LEDs and button
    UART0_Init();     // Initialize UART
    InitializeSensor();
    InitializeAlarm();

    char commandBuffer[BUFFER_SIZE];
    memset(commandBuffer, 0, sizeof(commandBuffer));
    int commandIndex = 0;

    while (1) {
        HandleTemperature(); // Handle temperature and alarm control

        // Check UART for incoming characters
        while (UARTCharsAvail(UART0_BASE)) {
            char receivedChar = (char)UARTCharGetNonBlocking(UART0_BASE);

            if (receivedChar == '\n' || receivedChar == '\r') {
                commandBuffer[commandIndex] = '\0';

                if (strcmp(commandBuffer, "STOP_ALARM") == 0) {
                    StopAlarm();
                } else if (isdigit((unsigned char)commandBuffer[0])) {
                    temperatureThreshold = atoi(commandBuffer);
                    alarmOverride = false;
                }

                commandIndex = 0;
                memset(commandBuffer, 0, sizeof(commandBuffer));
            } else if (commandIndex < BUFFER_SIZE - 1) {
                commandBuffer[commandIndex++] = receivedChar;
            }

            // Handle single-character UART commands
            if (receivedChar == 'd') { // Send door sensor state
                if (GPIOPinRead(GPIO_PORTD_BASE, MAGNETIC_SWITCH_PIN) == 0) {
                    UARTCharPut(UART0_BASE, 'C'); // Door is closed
                } else {
                    UARTCharPut(UART0_BASE, 'O'); // Door is open
                }
            } else if (receivedChar == 'r') { // Turn on red LED
                GPIOPinWrite(GPIO_PORTF_BASE, RED_LED, RED_LED);
            } else if (receivedChar == 'b') { // Turn on blue LED
                GPIOPinWrite(GPIO_PORTF_BASE, BLUE_LED, BLUE_LED);
            } else if (receivedChar == 'x') { // Turn off all LEDs
                GPIOPinWrite(GPIO_PORTF_BASE, RED_LED | BLUE_LED, 0);
            }
        }

        // Handle button press to toggle the red LED
        if (GPIOPinRead(GPIO_PORTF_BASE, BUTTON) == 0) { // Button pressed
            while (GPIOPinRead(GPIO_PORTF_BASE, BUTTON) == 0); // Wait for release
            GPIOPinWrite(GPIO_PORTF_BASE, RED_LED, ~GPIOPinRead(GPIO_PORTF_BASE, RED_LED) & RED_LED);
            delay_ms(200); // Debounce delay
        }
    }
}