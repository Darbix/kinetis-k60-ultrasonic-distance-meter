# Ultrasonic Distance Meter for NXP Kinetis K60

## Overview
This project implements a distance measurement device using an SRF05 ultrasonic sensor [2], a 4-digit 7-segment display, and the FITkit 3 development board based on the Freescale/NXP K60 microcontroller [1] ([datasheet](https://www.nxp.com/docs/en/data-sheet/K60P144M100SF2.pdf)). The software was built in [Kinetis Design Studio](https://www.nxp.com/design/design-center/development-boards-and-designs/design-studio-integrated-development-environment-ide:KDS_IDE).

The system periodically measures the distance to an object by transmitting an ultrasonic pulse and calculating the travel time of the reflected echo. The measured distance is displayed in centimeters on a multiplexed 4-digit 7-segment display.

## Documentation
The full project documentation is available here:

[Documentation (PDF)](Documentation/documentation.pdf)

And the FITkit 3 platform scheme is available here:

[FITkit v3 board wiring diagram (PDF)](Documentation/FITkit_board_v3.0_schematic.pdf)

## Hardware Components
- FITkit 3 development board (K60 MCU)
- SRF05 ultrasonic distance sensor
- 4-digit 7-segment display
- Connection wires

## Features
- Distance measurement range: 2–400 cm
- Automatic measurements every 50 ms
- Real-time distance display
- Accurate pulse-width measurement using hardware timers
- Interrupt-driven echo detection
- Multiplexed 4-digit display control

## System Operation
1) The MCU sends a 10 μs trigger pulse to the SRF05 sensor.
2) The sensor emits an ultrasonic burst and waits for the reflected signal.
3) When the echo signal rises, a timer starts measuring the pulse duration.
4) When the echo signal falls, the timer stops and the pulse width is recorded.
5) The measured time is converted into distance using the speed of sound.
6) The calculated distance is displayed on the 7-segment display.
7) A new measurement cycle begins every 50 ms.

## Software Architecture
### GPIO Initialization
The project configures:
- `PORTA` for sensor communication and display control
- `PORTD` for additional display segments

### Timer Management
Three PIT (Periodic Interrupt Timer) channels are used:
- Trigger pulse generation (10 μs)
- Echo pulse width measurement
- Periodic measurement scheduling (50 ms)

### Interrupt Handlers
- `PORTA_IRQHandler()` handles rising and falling edges on the Echo pin.
- `PITx_IRQHandler()` functions manage timer-based events and trigger generation.

### Display Driver
The display is controlled using multiplexing. Individual digits are refreshed continuously at a high frequency, creating the appearance of a stable four-digit display.

## Distance Calculation
The distance is computed from the measured echo pulse duration:

*Distance* = (*Speed of Sound* × *Time*) / 2

The division by two compensates for the round trip of the ultrasonic wave (sensor -> object -> sensor).

## Development Environment
- Kinetis Design Studio v3.0.0
- C language
- Freescale/NXP K60 MCU

## Results
The system was experimentally validated by comparing measured values with physical measurements obtained using a tape measure. The measured distances closely match real-world values within the operating range of the SRF05 sensor.

## References
[1] [K60 Sub-Family Reference Manual (NXP/Freescale)](https://www.nxp.com/docs/pcn_attachments/16627_K60P144M150SF3RM_release_notes.pdf)

[2] [SRF05 Ultrasonic Sensor Technical Documentation](https://www.robot-electronics.co.uk/htm/srf05tech.htm)
