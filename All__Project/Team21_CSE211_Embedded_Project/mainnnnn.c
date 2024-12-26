#include "DIO.h"
#include <stdint.h>
#include "TM4C123GH6PM.h"


// Define macros for the LED pins on PORTF (assumes red, blue, green are on bits 1, 2, and 3)
#define RED_LED   1
#define BLUE_LED  2
#define GREEN_LED 3
#define SW1 0
#define SW2 4
// Delay function to create a delay in milliseconds
void delay_ms(int delay) {
    for (int i = 0; i < delay * 4000; i++);  // Rough delay for TM4C123GH6PM at 16 MHz
}

int main() {
    // Initialize PORTF pins for the LEDs as output
    DIO_Init(RED_LED, OUTPUT);
    DIO_Init(BLUE_LED, OUTPUT);
    DIO_Init(GREEN_LED, OUTPUT);
    DIO_Init(SW1, INPUT);
    DIO_Init(SW2, INPUT);
 
     while (1) {
        // Check if SW1 is pressed (logic 0 because of pull-up resistor)
        if (DIO_ReadPin(SW1) == 0) {
            // Turn on red LED and wait
            DIO_WritePin(RED_LED, 1);
            delay_ms(500);
            DIO_WritePin(RED_LED, 0);
        }

        // Check if SW2 is pressed (logic 0 because of pull-up resistor)
        if (DIO_ReadPin(SW2) == 0) {
            // Cycle through blue and green LEDs
            DIO_WritePin(BLUE_LED, 1);
            delay_ms(500);
            DIO_WritePin(BLUE_LED, 0);

            DIO_WritePin(GREEN_LED, 1);
            delay_ms(500);
            DIO_WritePin(GREEN_LED, 0);
        }
}
}







