---
id: electronics/wp2-microcontroller
title: Microcontroller
category: electronics
tags: [microcontroller, microcontroller unit, mcu, electronics]
summary: A microcontroller (MC, uC, or μC) or microcontroller unit (MCU) is a small computer on a single integrated circuit.
source: https://en.wikipedia.org/w/index.php?title=Microcontroller&oldid=1381884829
source_title: Wikipedia: Microcontroller
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Microcontroller", Wikipedia, The Free Encyclopedia (revision 1381884829), https://en.wikipedia.org/w/index.php?title=Microcontroller&oldid=1381884829 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Microcontroller

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **microcontroller** (**MC**, **uC**, or **μC**) or **microcontroller unit** (**MCU**) is a small computer on a single integrated circuit. A microcontroller contains one or more processor cores along with memory and programmable input/output peripherals. Program memory in the form of NOR flash, OTP ROM, or ferroelectric RAM is also often included on the chip, as well as a small amount of RAM. Microcontrollers are designed for embedded applications, in contrast to the microprocessors used in personal computers or other general-purpose applications consisting of various discrete chips.

In modern terminology, a microcontroller is similar to, but less sophisticated than, a system on a chip (SoC). A SoC may include a microcontroller as one of its components but usually integrates it with advanced peripherals like a graphics processing unit (GPU), a Wi-Fi module, or one or more coprocessors.

Microcontrollers are used in automatically controlled products and devices, such as automobile engine control systems, implantable medical devices, remote controls, office machines, appliances, power tools, toys, and other embedded systems. By reducing the size and cost compared to a design that uses a separate microprocessor, memory, and input/output devices, microcontrollers make digital control of more devices and processes practical. Mixed-signal microcontrollers are common, integrating analog components needed to control non-digital electronic systems. In the context of the Internet of things, microcontrollers are an economical and popular means of data collection, sensing and actuating the physical world as edge devices.

Some microcontrollers may use four-bit words and operate at frequencies as low as 4 kHz for low power consumption (single-digit milliwatts or microwatts). They generally have the ability to retain functionality while waiting for an event such as a button press or other interrupt; power consumption while sleeping (with the CPU clock and most peripherals off) may be just nanowatts, making many of them well suited for long lasting battery applications. Other microcontrollers may serve performance-critical roles, where they may need to act more like a digital signal processor (DSP), with higher clock speeds and power consumption.

## Embedded design

A microcontroller can be considered a self-contained system with a processor, memory and peripherals and can be used as an embedded system. The majority of microcontrollers in use today are embedded in other machinery, such as automobiles, telephones, appliances, and peripherals for computer systems.

While some embedded systems are very sophisticated, many have minimal requirements for memory and program length, with no operating system, and low software complexity. Typical input and output devices include switches, relays, solenoids, LEDs, small or custom liquid-crystal displays, radio frequency devices, and sensors for data such as temperature, humidity, light level etc. Embedded systems usually have no keyboard, screen, disks, printers, or other recognizable I/O devices of a personal computer, and may lack human interaction devices of any kind.

### Interrupts

Microcontrollers must provide real-time (predictable, though not necessarily fast) response to events in the embedded system they are controlling. When certain events occur, an interrupt system can signal the processor to suspend processing the current instruction sequence and to begin an interrupt service routine (ISR, or "interrupt handler") which will perform any processing required based on the source of the interrupt, before returning to the original instruction sequence. Possible interrupt sources are device-dependent and often include events such as an internal timer overflow, completing an analog-to-digital conversion, a logic-level change on an input such as from a button being pressed, and data received on a communication link. Where power consumption is important as in battery devices, interrupts may also wake a microcontroller from a low-power sleep state where the processor is halted until required to do something by a peripheral event.

### Programs

Typically microcontroller programs must fit in the available on-chip memory, since it would be costly to provide a system with external, expandable memory. Compilers and assemblers are used to convert both high-level and assembly language code into a compact machine code for storage in the microcontroller's memory. Depending on the device, the program memory may be permanent, read-only memory that can only be programmed at the factory, or it may be field-alterable flash or erasable read-only memory.

Manufacturers have often produced special versions of their microcontrollers in order to help the hardware and software development of the target system. Originally these included EPROM versions that have a "window" on the top of the device through which program memory can be erased by ultraviolet light, ready for reprogramming after a programming ("burn") and test cycle. Since 1998, EPROM versions are rare and have been replaced by EEPROM and flash, which are easier to use (can be erased electronically) and cheaper to manufacture.

Other versions may be available where the ROM is accessed as an external device rather than as internal memory, however these are becoming rare due to the widespread availability of cheap microcontroller programmers.

The use of field-programmable devices on a microcontroller may allow field update of the firmware or permit late factory revisions to products that have been assembled but not yet shipped. Programmable memory also reduces the lead time required for deployment of a new product.

Where hundreds of thousands of identical devices are required, using parts programmed at the time of manufacture can be economical. These "mask-programmed" parts have the program laid down in the same way as the logic of the chip, at the same time.

A customized microcontroller incorporates a block of digital logic that can be personalized for additional processing capability, peripherals and interfaces that are adapted to the requirements of the application. One example is the AT91CAP from Atmel.

### Other microcontroller features

Microcontrollers usually contain from several to dozens of general purpose input/output pins (GPIO). GPIO pins are software configurable to either an input or an output state. When GPIO pins are configured to an input state, they are often used to read sensors or external signals. Configured to the output state, GPIO pins can drive external devices such as LEDs or motors, often indirectly, through external power electronics.

Many embedded systems need to read sensors that produce analog signals. However, because they are built to interpret and process digital data, i.e. 1s and 0s, they are not able to do anything with the analog signals that may be sent to it by a device. So, an analog-to-digital converter (ADC) is used to convert the incoming data into a form that the processor can recognize. A less common feature on some microcontrollers is a digital-to-analog converter (DAC) that allows the processor to output analog signals or voltage levels.

In addition to the converters, many embedded microprocessors include a variety of timers as well. One of the most common types of timers is the programmable interval timer (PIT). A PIT may either count down from some value to zero, or up to the capacity of the count register, overflowing to zero. Once it reaches zero, it sends an interrupt to the processor indicating that it has finished counting. This is useful for devices such as thermostats, which periodically test the temperature around them to see if they need to turn the air conditioner on/off, the heater on/off, etc.

A dedicated pulse-width modulation (PWM) block makes it possible for the CPU to control power converters, resistive loads, motors, etc., without using many CPU resources in tight timer loops.

A universal asynchronous receiver/transmitter (UART) block makes it possible to receive and transmit data over a serial line with very little load on the CPU. Dedicated on-chip hardware also often includes capabilities to communicate with other devices (chips) in digital formats such as Inter-Integrated Circuit (I²C), Serial Peripheral Interface (SPI), Universal Serial Bus (USB), and Ethernet.

_(Later sections of the original article were left out of this offline copy for length: Higher integration, Programming environments, Types, Interrupt latency, Memory technology.)_
