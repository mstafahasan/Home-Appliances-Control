# CSE211: Introduction to Embedded Systems
**Fall 2024**

## Project Report

### Team Members
- Mazen Saeed Mohamed (21P0125)
- Youssef Habil Abou Elkhier (21P0187)
- Sara Mohamed Ashour (21P0337)
- Mostafa Hassan Mohamed (21P0349)
- Adel Mohamed Adel (21P0113)

### Table of Contents
1. [Project Description](#project-description)
2. [The Contribution of Each Member of the Group](#the-contribution-of-each-member)
3. [Features Overview](#features-overview)
4. [C Code Description](#c-code-description)
    - 4.1 [Includes and Definitions](#includes-and-definitions)
    - 4.2 [Global Variables](#global-variables)
    - 4.3 [Peripheral Initialization Functions](#peripheral-initialization-functions)
    - 4.4 [Helper Functions](#helper-functions)
    - 4.5 [Sensor Communication](#sensor-communication)
    - 4.6 [UART Communication](#uart-communication)
    - 4.7 [Alarm and Temperature Control](#alarm-and-temperature-control)
    - 4.8 [Main Loop](#main-loop)
5. [Flow Charts of the Main Flow of the Program](#flow-charts-of-the-main-flow-of-the-program)
6. [List of Components](#list-of-components)
7. [Wiring Diagram](#wiring-diagram)
8. [Prototype Photos](#prototype-photos)
9. [Problems Faced and How We Managed to Solve Them](#problems-faced-and-how-we-managed-to-solve-them)

### 1. Project Description
You are required to build a small prototype for a desktop-based home appliances control system that satisfies the following requirements:
1. Develop a Desktop Application to:
    - Control a 220 Volt lamp on/off (without removing the manual switch).
    - Control a plug on/off: If it’s off, no one can use the plug.
    - Display the door status indicated through a magnetic switch.
    - Display room temperature.
    - Trigger an alarm if the temperature exceeds a set value (Software Alarm + Physical Alarm).
    - Save and show the times of opening and closing the door.
2. Hardware Setup:
    - Your components must be mounted on a suitable surface, and all wires should be hidden.
3. Microcontroller Board:
    - You should use the TM4C123GH6PM board to implement this project.
4. Desktop Application Features:
    - The application should have a user-friendly interface to monitor and control the system.
    - Real-time updates for all connected devices.
    - Historical logs for door activity and temperature alarms.
    - Adjustable temperature thresholds for triggering alarms.

### 2. The Contribution of Each Member
| Member Name      | Tasks Completed                |
|------------------|--------------------------------|
| Mazen Saeed      | Temperature Sensor             |
| Youssef Habil    | Lamp/Plug/Relay                |
| Sara Mohamed     | Limit Switch                   |
| Mostafa Hassan   | Door Sensor                    |
| Adel Mohamed     | Buzzer                         |

### 3. Features Overview
1. **Temperature and Alarm Management**:
    - Reads temperature from a sensor.
    - Activates an alarm if the temperature exceeds the threshold.
    - Deactivates the alarm based on UART command or low temperature.
2. **Door Monitoring**:
    - Reads the state of a magnetic switch (open/closed).
    - Sends door state over UART.
3. **LED Control**:
    - Toggles Red/Blue LEDs via UART or a button press.
4. **UART Communication**:
    - Sends/receives commands to control the system and query states.

### 4. C Code Description

#### 4.1 Includes and Definitions
**Header Files**:
- `stdint.h` and `stdbool.h`: Standard libraries for integer types and boolean values.
- `string.h`, `ctype.h`, `stdio.h`, `stdlib.h`: Libraries for string manipulation, character functions, and I/O operations.
- `hw_memmap.h`, `driverlib/*`: Hardware-specific headers for the TM4C123GH6PM microcontroller.
- `tm4c123gh6pm.h`: Defines hardware registers and addresses specific to the TM4C123GH6PM microcontroller.

**Pin Definitions**:
- `MAGNETIC_SWITCH_PIN (PD0)`: Door sensor input.
- `RED_LED (PF1)`: Red LED output.
- `BLUE_LED (PF2)`: Blue LED output.
- `BUTTON (PF4)`: Push button input (active low).
- `SENSOR_PIN (PB0)`: Pin used for temperature sensor communication.
- `ALARM_PIN (PD2)`: Alarm control output.

#### 4.2 Global Variables
- `temperatureThreshold`: The maximum temperature before the alarm triggers. Defaults to 99.
- `alarmActive`: Indicates if the alarm is currently active.
- `alarmOverride`: Tracks whether the alarm has been manually disabled to prevent reactivation.

#### 4.3 Peripheral Initialization Functions

**PortD_Init()**:
1. Enables the clock for Port D.
2. Configures PD0 (door sensor input) with a pull-up resistor, making it active-low.

**PortF_Init()**:
1. Enables the clock for Port F.
2. Unlocks PF4 (required for buttons on Port F).
3. Configures:
    - PF1 (Red LED) and PF2 (Blue LED) as outputs.
    - PF4 (button) as an input with a pull-up resistor.

**UART0_Init()**:
1. Enables UART0 and Port A.
2. Configures PA0 (RX) and PA1 (TX) for UART functionality.
3. Initializes UART0 with:
    - Baud rate: 9600.
    - 8 data bits, 1 stop bit, no parity.
4. Enables UART communication.

#### 4.4 Helper Functions

**InitializeSensor()**:
1. Enables the clock for Port B.
2. Configures PB0 as an output and sets it high initially.

**InitializeAlarm()**:
1. Enables the clock for Port D.
2. Configures PD2 as an output and ensures the alarm is off initially.

**MicrosecondDelay()** and **MillisecondDelay()**:
- Create precise delays by calculating cycles based on the system clock.

#### 4.5 Sensor Communication

**ReadSensorData()**:
This function reads temperature and humidity from a DHT sensor:
1. Sends a start signal:
    - Drives PB0 low for 18 ms to initiate communication.
    - Pulls it high for 20 µs, then switches to input mode.
2. Waits for the sensor’s response.
3. Reads 40 bits of data:
    - 8 bits each for humidity integer, humidity decimal, temperature integer, temperature decimal, and checksum.
4. Validates the checksum.
5. Returns true if successful, along with temperature and humidity values.

#### 4.6 UART Communication

**TransmitTemperature()**:
1. Formats the temperature into a string (e.g., "T:25\r\n").
2. Sends the string byte-by-byte via UART.

#### 4.7 Alarm and Temperature Control

**HandleTemperature()**:
1. Reads the current temperature and humidity using `ReadSensorData()`.
2. Transmits the temperature via UART.
3. Alarm Logic:
    - Activates the alarm if the temperature exceeds the threshold, provided it hasn’t been overridden.
    - Deactivates the alarm if the temperature falls below the threshold.

**StopAlarm()**:
1. Turns off the alarm.
2. Sends an alarm deactivation message via UART.
3. Sets `alarmOverride` to true, preventing further activation.

#### 4.8 Main Loop

**Initialization**:
1. Configures system clock for 40 MHz operation.
2. Initializes peripherals:
    - Port D (door sensor), Port F (LEDs, button).
    - UART0, sensor, and alarm.

**Infinite Loop**:
1. **Temperature Monitoring**:
    - Continuously checks the temperature using `HandleTemperature()`.
2. **UART Commands**:
    - Handles multi-character commands like:
        - `"STOP_ALARM"`: Disables the alarm and sets the override flag.
        - Numeric commands: Updates the `temperatureThreshold`.
    - Handles single-character commands:
        - `'d'`: Sends door sensor status ('C' for closed, 'O' for open).
        - `'r'`: Turns on the red LED.
        - `'b'`: Turns on the blue LED.
        - `'x'`: Turns off all LEDs.
3. **Button Handling**:
    - Toggles the red LED when the button is pressed.

### 5. Flow Charts of the Main Flow of the Program
(Insert detailed flow charts here)

### 6. List of Components
- Tiva™ TM4C123GH6PM Microcontroller
- Relay Module (2 channels)
- Adaptor 5V/2A
- DC female power plug
- Limit Switch
- Magnetic Door Switch
- Buzzer
- Male and Female Plugs
- Humidity Temperature Sensor DHT11
- Bread board
- Jumpers
- Lamp
- Lamp Holder

### 7. Wiring Diagram
![image](https://github.com/user-attachments/assets/9ca90468-adcd-48dd-9a7b-a5797d3bb2d7)


### 9. Problems Faced and How We Managed to Solve Them

### 9. Problems Faced and How We Managed to Solve Them

**1. Misunderstanding of Relay Connections**
- **Problem**:
    We encountered a misunderstanding regarding the relay connection. Specifically, we were unsure whether the relay should be connected to the "normally open" (NO) or "normally closed" (NC) terminal.
- **Reason**:
    The ambiguity arose from a lack of clear documentation and experience with the relay module connections.
- **Result**:
    The relay did not switch correctly, leading to operational issues with the lamp control.
- **Solution**:
    We reviewed the relay module's datasheet and experimented with both terminals. Eventually, we found that connecting to the "normally open" (NO) terminal provided the desired control.

**2. Issues with LM35 Sensor**
- **Problem**:
    The LM35 temperature sensor provided random and inaccurate readings. Additionally, the sensor short-circuited during the testing phase.
- **Reason**:
    The random readings were likely caused by electrical noise or incorrect connections. The short circuit occurred due to an accidental voltage spike or miswiring.
- **Result**:
    The temperature readings were unreliable, which rendered the system ineffective for monitoring temperature. The short circuit also damaged the sensor, necessitating a replacement.
- **Solution**:
    We replaced the LM35 sensor with a DHT11 sensor, which offered more stable and accurate temperature readings. This change also improved the overall reliability of the system.

**3. Integration with the GUI**
- **Problem**:
    When integrating the door sensor and the temperature sensor with the GUI, the status would intermittently show "unknown" instead of the correct values.
- **Reason**:
    The issue was caused by the simultaneous querying of both sensors. The system attempted to send and receive data for both sensors at the same time, which led to communication conflicts and occasional data loss.
- **Result**:
    The GUI displayed incorrect or missing statuses for the sensors, which diminished the usability of the system.
- **Solution**:
    We implemented a delay between the readings of each sensor. By staggering the queries and allowing a brief pause between them, we ensured that the readings for both the door sensor and the temperature sensor were received and displayed correctly.

