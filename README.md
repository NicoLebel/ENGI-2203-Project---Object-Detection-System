# Object Detection Radar System

Embedded software developed for the **ENGI 2203 Engineering Design II** project at **Dalhousie University**.

The **Object Detection Radar System (ODRS)** is an autonomous object-detection platform designed to provide directional object detection and range information using ultrasonic sensors mounted on a rotating and vertically adjustable platform. The system combines embedded control, ultrasonic sensing, servo control, keypad input, and short-range wireless communication between microcontrollers.

> **Note:** This repository contains the embedded C firmware developed for the project. The complete ODRS also included hardware, mechanical, and communication subsystems that are documented in the project final report.

---

## Project Overview

The ODRS was developed to explore a compact sensing system capable of detecting objects over a 360° field of view while allowing the sensing direction and detection range to be adjusted by the user.

The system incorporated:

* Ultrasonic object detection
* 360° rotational scanning
* Vertical sensor alignment
* User input through a matrix keypad
* LED-based system feedback
* Wireless communication between rotating and stationary sections
* Embedded signal generation and detection
* AVR microcontroller-based real-time control

---

## Repository Contents

| File       | Description                                                                                                                                                      |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `main.c`   | Main embedded application containing timer configuration, servo control, keypad scanning, FSK signal generation and reception, interrupts, and application logic |
| `USART0.c` | UART driver implementing buffered transmission and reception using interrupts                                                                                    |
| `USART0.h` | Header file containing the UART driver interface and declarations                                                                                                |

The repository focuses on the embedded firmware developed for the ODRS rather than the complete mechanical or hardware design files.

---

## System Architecture

The complete ODRS was divided into five major functional modules:

1. **Communication**
2. **Vertical Alignment**
3. **Rotational Control**
4. **Data Collection**
5. **Mechanical Structure**

The embedded firmware interacts primarily with the communication, vertical alignment, rotational control, and data collection functions.

### High-Level Architecture

```text
                    ┌─────────────────────────┐
                    │      User Input         │
                    │       4 × 3 Keypad      │
                    └────────────┬────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │     ATmega328P MCU      │
                    │                         │
                    │  • Keypad Processing    │
                    │  • Servo Control        │
                    │  • Timer Management     │
                    │  • FSK Generation       │
                    │  • FSK Reception        │
                    │  • UART Debugging       │
                    └───────┬─────────┬───────┘
                            │         │
                ┌───────────┘         └───────────┐
                ▼                                 ▼
       ┌─────────────────┐               ┌─────────────────┐
       │ Vertical Servo  │               │ Communication   │
       │    Control      │               │    Interface    │
       └─────────────────┘               └────────┬────────┘
                                                   │
                                                   ▼
                                        ┌────────────────────┐
                                        │ Rotating Section   │
                                        │ / Second MCU       │
                                        └────────────────────┘
```

---

# Embedded Software

## Microcontroller

The firmware targets the **ATmega328P** running at:

```text
F_CPU = 16 MHz
```

The code directly configures AVR registers and interrupts rather than relying on a high-level Arduino framework.

---

## Timer Architecture

Multiple AVR timers are used for different timing-critical functions.

### Timer 0 — FSK Signal Generation

Timer 0 generates the carrier waveform used by the wireless communication system.

A 64-entry lookup table approximates a sine wave. Two carrier frequencies represent binary data:

| Bit | Frequency |
| --- | --------: |
| `0` |    140 Hz |
| `1` |    280 Hz |

The firmware changes the timer TOP value to switch between the two frequencies.

---

### Timer 1 — Servo Control and Input Capture

Timer 1 operates in CTC mode and generates interrupts at approximately:

```text
10 µs
```

The timer performs two functions:

* Generates software PWM signals for the two vertical-alignment servos
* Measures the received communication signal using input capture

Servo pulse widths are represented using timer ticks:

```text
100 ticks → approximately 1.0 ms
150 ticks → approximately 1.5 ms
200 ticks → approximately 2.0 ms
```

The nominal centre position is therefore:

```text
150 ticks
```

---

### Timer 2 — Communication Timing

Timer 2 generates a periodic interrupt used to track the timing of received communication pulses.

These timing counters are used by the input-capture interrupt to distinguish between the two FSK frequencies.

---

# Vertical Alignment

The ODRS uses two SG90 servos to vertically align the ultrasonic sensors.

The firmware generates the servo signals manually using the Timer 1 compare interrupt.

Each 20 ms servo period is divided into 10 µs timer intervals.

```text
20 ms period
│
├── Servo 1 pulse
│   └── 1–2 ms
│
├── idle
│
├── Servo 2 pulse
│   └── 1–2 ms
│
└── remaining period
```

Servo 1 begins its pulse at timer tick `1`.

Servo 2 begins its pulse at timer tick `1000`.

---

# Keypad Interface

A **4 × 3 matrix keypad** provides user input for vertical sensor alignment.

```text
1 2 3
4 5 6
7 8 9
* 0 #
```

The firmware scans each row sequentially and checks the keypad columns for a pressed key.

### User Input

The `*` key begins a new angle-entry sequence.

The user enters the desired angle followed by `#` to confirm.

The firmware supports:

* Positive angles
* Negative angles
* Angles up to ±45°
* Angle conversion to servo position
* Rejection of angles outside the supported range

For example:

```text
* → begin input
30 → enter angle
# → confirm
```

---

# FSK Communication

The ODRS uses **frequency-shift keying (FSK)** for communication between the stationary and rotating sections.

Two frequencies represent binary data:

```text
Binary 0 → 140 Hz
Binary 1 → 280 Hz
```

The transmitter generates the carrier using PWM and a 64-point sine lookup table.

```text
Binary Data
     │
     ▼
Select Frequency
     │
 ┌───┴────┐
 │        │
 ▼        ▼
140 Hz   280 Hz
 │        │
 └───┬────┘
     ▼
PWM Sine-Wave Generation
     │
     ▼
Analog Communication Circuit
```

The implementation transmits different numbers of waveform cycles for each bit while maintaining a consistent bit duration.

---

## FSK Signal Reception

The receiving side uses the ATmega328P's **Timer 1 input-capture peripheral**.

Detected signal edges trigger an input-capture interrupt. The firmware counts signal transitions during a defined measurement interval and uses the resulting count to distinguish between the two carrier frequencies.

```text
Hall Sensor
    │
    ▼
Signal Conditioning
    │
    ▼
Comparator
    │
    ▼
ATmega328P Input Capture
    │
    ▼
Count Signal Edges
    │
    ▼
Determine Frequency
    │
    ▼
Decode Binary Bit
```

The decoded bits are stored and processed by the firmware before being output for debugging.

---

# UART Debugging

The project includes a buffered UART driver in `USART0.c` and `USART0.h`.

The driver uses interrupt-driven transmission and reception with separate circular buffers.

```text
Application
     │
     ▼
UART TX Buffer
     │
     ▼
USART Interrupt
     │
     ▼
UART Hardware
```

The driver provides functions including:

```c
init_uart0()
uart0_putc()
uart0_getc()
uart0_puts()
uart0_puts_P()
uart0_RxCount()
uart0_write_buff_full()
```

UART output was used during development to monitor system behaviour and debug the communication subsystem.

---

# Interrupt-Driven Design

The firmware relies heavily on AVR interrupts to handle timing-sensitive operations.

The primary interrupt service routines include:

| Interrupt           | Function                               |
| ------------------- | -------------------------------------- |
| `TIMER0_OVF_vect`   | FSK waveform generation and bit timing |
| `TIMER1_COMPA_vect` | Software servo PWM generation          |
| `TIMER1_CAPT_vect`  | FSK signal measurement and decoding    |
| `TIMER2_COMPA_vect` | Communication timing                   |
| `USART_UDRE_vect`   | UART transmission                      |
| `USART_RX_vect`     | UART reception                         |

Using interrupts allows multiple timing-sensitive operations to execute concurrently while the main loop handles higher-level processing.

---

# Object Detection

The complete ODRS used **HC-SR04 ultrasonic sensors** to detect objects and determine their range.

The ultrasonic measurement follows a trigger/echo process:

```text
Trigger Pulse
     │
     ▼
HC-SR04 emits ultrasonic burst
     │
     ▼
Object reflects signal
     │
     ▼
Echo pulse received
     │
     ▼
Measure pulse duration
     │
     ▼
Calculate distance
```

The project used:

```text
Distance = T × 0.0343 / 2
```

where `T` is the measured echo duration in microseconds.

The complete system classified detected objects into three distance zones:

| Range   | Status  |
| ------- | ------- |
| > 10 cm | Safe    |
| 1–10 cm | Warning |
| < 1 cm  | Danger  |

---

# 360° Scanning

The sensing platform was mounted to an MG90S continuous-rotation servo.

The servo rotated the upper portion of the ODRS, allowing the ultrasonic sensors to scan the surrounding area.

Testing achieved approximately:

```text
20–30 RPM
```

under operating conditions.

---

# Hardware

The complete prototype incorporated:

### Microcontrollers

* ATmega328P

### Sensors

* HC-SR04 ultrasonic sensors
* Hall-effect sensors

### Actuators

* MG90S continuous-rotation servo
* SG90 positional servos

### User Interface

* 4 × 3 matrix keypad
* LEDs

### Communication Hardware

* Coils
* Signal-conditioning circuitry
* Low-pass filtering
* Amplification
* Comparator

---

# Testing and Results

Individual subsystems were tested throughout development.

Successful subsystem tests included:

* Static object detection
* Moving object detection
* 360° rotation
* Sensor scan synchronization
* Vertical servo alignment
* Keypad angle input
* Negative angle input
* Invalid-angle rejection
* Standalone communication

The final integrated system experienced limitations with the wireless communication integration and real-time range/angle adjustment.

The project demonstrated the importance of integrating hardware and software subsystems early rather than waiting until final assembly.

---

# Technologies Used

### Programming

* Embedded C
* AVR-GCC
* Standard C libraries

### Embedded Systems

* ATmega328P
* GPIO
* Hardware timers
* CTC mode
* PWM
* Input capture
* Interrupt service routines
* UART
* Matrix keypad scanning
* Servo control
* FSK communication

### Hardware

* HC-SR04 ultrasonic sensors
* MG90S continuous-rotation servo
* SG90 servos
* Hall-effect sensors
* Matrix keypad
* LEDs
* Analog signal-conditioning circuitry

---

# Repository Structure

```text
ENGI-2203-Project-Object-Detection-System/
│
├── main.c
├── USART0.c
├── USART0.h
└── README.md
```

---

# Building the Firmware

The repository contains AVR C source files but does not currently include a project-specific Makefile or build configuration.

The firmware targets the ATmega328P with a 16 MHz clock:

```c
#define F_CPU 16000000UL
```

The source can be compiled using an AVR-GCC toolchain configured for the ATmega328P.

---

# Project Context

This project was completed as part of **ENGI 2203 — Engineering Design II** at **Dalhousie University**.

### Team

* Ahmad Zarkawi
* Ahmed Awaad
* Carlo Gonzalez
* Nicholas Lebel

**Project:** Object Detection Radar System
**Course:** ENGI 2203 — Engineering Design II
**University:** Dalhousie University
**Date:** April 2025

---

## Author

**Nicholas Lebel**

Electrical Engineering
Dalhousie University

GitHub: [NicoLebel](https://github.com/NicoLebel)

---

## Documentation

The accompanying project report provides additional documentation of the complete ODRS, including the system architecture, electrical design, embedded software, wireless communication, mechanical design, testing, and system limitations.
