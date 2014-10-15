IOTool
======
IOTool is a firmware for AVR microcontrollers that provides a simple serial
interface for scripting/sequencing input and output actions, controlled by
a host computer.

This firmware was originally developed for running laboratory experiments in
which multiple hardware devices need to signal one another via TTL, and where
the experimental parameters need to be rapidly reconfigurable. (So writing a
new firmware for each experiment was out of the questions.)

In general, this will be useful for any needs for high-speed sequencing of
inputs and outputs, controlled by a host computer.

Developed by [Zach Pincus](zplab.wustl.edu), and provided under a GPL2 license.

Basic Usage Example
-------------------
First, connect the device to a USB port and note the serial port that appears
on your system. (On Windows this will require the IOTool.inf file.)
Next, connect to that serial port in your favorite programming environment
or with a terminal program such as hyperterminal or minicom. Send the following
text to the device:

    program
    sh 13
    dm 500
    sl 13
    dm 500
    lo 0 9
    end

This loads a program to set pin 13 high (turning on the arduino's onboard LED),
delay 500 ms, set the pin low (turning the LED off), delay 500 ms, and then
repeat that from the first step 9 more times. Sending the text `run` followed
by a newline (return key) will cause the LED to blink 10 times.

Hardware Compatibility
----------------------
IOTool is natively compatible with any board powered by an ATmega32u4, including:
 - [Arduino Leonardo](http://arduino.cc/en/Main/arduinoBoardLeonardo)
 - [Arduino Micro](http://arduino.cc/en/Main/ArduinoBoardMicro)
 - [LilyPad Arduino USB](http://arduino.cc/en/Main/ArduinoBoardLilyPadUSB)
 - [Adafruit ATmega32u4 Breakout Board](http://www.adafruit.com/products/296)
 - [Adafruit Flora](http://www.adafruit.com/products/659)
 - [PJRC Teensy 2.0](https://www.pjrc.com/teensy/)
 - [SparkFun ATmega32u4 Breakout](https://www.sparkfun.com/products/11117)
 - [SparkFun Pro Micro](https://www.sparkfun.com/products/12640)

However, it should be possible with minimal effort to port IOTool to any AVR
microcontroller supported by the LUFA USB library (see 'Porting' below).

Installation
------------
0.  Obtain the [LUFA USB library for AVR microcontrollers](http://www.fourwalledcubicle.com/LUFA.php)
    AVR-GCC is also required: [source](http://www.nongnu.org/avr-libc/), [Mac](http://www.obdev.at/products/crosspack)
    [Windows](http://sourceforge.net/projects/winavr).
    
1.  Configure the IOTool Makefile as follows.
    First, set the LUFA_PATH variable to the relative or absolute path to the
    LUFA library obtained above.

    Second, if you are using an Arduino device, uncomment the ARD_PINS line
    in the Makefile.
    
    Third, set AVRDUDE_PORT to the name of the serial port that appears when
    the board is plugged in and the reset button is pressed. (On Windows, 
    an INF file supplied by the board's manufacturer is generally required for
    the board to show up as a serial port.) If you will not be using avrdude
    to upload the firmware (e.g. with a Teensy you need to use TeensyLoader),
    skip this step.
    
    If your microcontroller is not clocked at 16 MHz, adjust the F_CPU value.
    This will change the PWM frequencies and delay times, unless additional
    code modifications are made (see 'Porting' below).
    
2.  Optional: To customize the device's serial number (used on Macs and some Linuxes
    for determining the tty name), edit the relevant `#define` in `src/usb_serial_base.h`.
    
3.  Press the reset button to enable the firmware to be loaded, and run `make`
    from the IOTool directory to compile and upload the code.
    
4.  After the firmware is loaded, a new serial port will appear. On Windows, 
    this requires the supplied `IOTool.inf` file.

Scripting Language Specification
--------------------------------
### Command summary ###
    wh p        wait high: pin name
    wl p        wait low: pin name
    wc p        wait change: pin name
    wt d        set wait time: uint16 µs delay
    rh p        raw wait high: pin name
    rl p        raw wait low: pin name
    rc p        raw wait change: pin name
    dm d        delay ms: uint16 ms delay
    du d        delay µs: uint16 µs delay
    pm p v      set PWM: pin name, uint8 or uint16 value
    sh p        set high: pin name
    sl p        set low: pin name
    st p        set high-impedance "tri-state": pin name
    ct b        character transmit: uint8 byte
    cr          character receive
    lo i c      loop: uint8 index, uint16 count
    go i        goto: uint8 index
    
    program     start programming, clearing previous
    end         end programming, return to immediate-execution mode
    run c       run program: uint16 count (optional)
    \x80\xFF    turn serial echo off for non-interactive use
    !           break out of execution
(See section below on pin names for further details.)

### PWM-capable pins ###
     8-bit @ 62.500 kHz: B7 and D0 (AVR) = 11 and 3 (Arduino)
     8-bit @ 31.250 kHz: C7 and D7 (AVR) = 13 and 6 (Arduino)
    10-bit @ 15.625 kHz: B5 and B6 (AVR) = 9 and 10 (Arduino)

### Detailed command description ###
*Wait for specific pin value*:`wh pin` (wait for high), `wl pin` (wait for
low), and `wc pin` (wait for change), where _pin_ is a one- or two-character
pin name (either the AVR port-letter + pin-number format, or the arduino
format; more about that below). These commands wait for a stable reading on a
given pin of a high, low, or different-from-current value (respectively). The
specified pin is set to input and the internal pull-up resistor is enabled
before readings are taken. A "stable" reading is defined as pulse lasting
longer than the currently set wait time, which defaults to 10 µs. This
provides protection against stray electromagnetic interference.

*Set waiting time for stable readings*: `wt time`, where 0 < _time_ < 2^15,
in microseconds. The `wh`, `wl`, and `wc` commands wait for a pulse to read at
the desired level for at least this many µs before returning. The default is
10 µs.

*Wait for specific pin value without delay*: `rh pin` (raw wait for high), `rl
pin` (raw wait for low), and `rc pin` (raw wait for change), where _pin_ is a
one- or two-character pin name. As soon as a pin change to the desired state
is detected, these commands return. There is no protection against
bounce/electromagnetic interference in raw wait modes. However, the response
time is faster (see timing data below). The specified pin is set to input and
the internal pull-up resistor is enabled before readings are taken.

*Delay a given interval*: `dm ms` (delay milliseconds) and `du us` (delay
micoseconds), where 0 ≤ _ms_ < 2^16 and 0 ≤ _us_ < 2^15. Note: these
delay times are for a chip clocked at 16 MHz. For other clock speeds, adjust
accordingly, or alter the timer counter values (described in the 'Porting'
section below).

*Set a pin's value*: `sh pin` (set high), `sl pin` (set low), and `st pin`
(set tristate), where _pin_ is a one- or two-character pin name. The effects
of setting high and low are obvious. Setting a pin to tri-state
(high-impedance) means that it will not drive a circuit in any direction. (The
pin is set to input and the internal pull-up resistor is disabled.)

*Enable PWM*: `pm pin value`, where _pin_ is a one- or two-character pin name,
and _value_ is a PWM duty cycle in either an 8-bit (0 ≤ _value_ < 2^8) or
10-bit (0 ≤ _value_ < 2^10) range, depending on the pin specified. There are
six PWM capable pins: two 8-bit pins with a PWM frequency of 62.5 kHz (AVR
pins B7 and D0, and Arduino pins 11 and 3), two 8-bit pins at 31.25 kHz (AVR
C7 and D7 / Arduino 13 and 6), and two 10-bit pins at 15.625 kHz (AVR B5 and
B6 / Arduino 9 and 10). Note: these PWM frequencies are for a chip clocked at
16 MHz. For other clock speeds, adjust accordingly.

*Send and Receive Serial Data to/from Host* `cr` (character receive) and `ct
value` (character transmit), where 0 ≤ value < 2^8. These commands are
useful for synchronizing script execution with the host computer. If the `cr`
command is issued, execution halts until a single byte is sent from the
computer host to the microcontroller. The contents of this byte are discarded
and not echoed back to the host. When `ct` commands are run, a single byte
with the value specified (from 0 to 255) will be transmitted to the host.

*Repeating Commands* `lo index count` (loop back), and `go index` (goto),
where 0 ≤ _index_ < 2^8 and 0 ≤ _count_ < 2^16. These commands provide
for repeating script commands either a fixed number of times (`lo`), or
indefinitely (`go`). The _index_ parameter refers to the command number in the
current program (starting from 0 as the first command) to jump to. The _count_
parameter for `lo` commands refers to the number of times to execute that
loop. In the usual configuration of looping back to previous steps, this means
"repeat these steps _count_ additional times", for a total of _count_+1
executions of the looped steps. Loops can be nested within other loops.

### Control Commands ###

`program`: This command starts storing a program to run later, and clears any
previously-stored programs. Any command not placed between `program` and `end`
will be immediately executed, allowing direct control over the device pins.
(The commands `lo` and `go` are nonsensical in immediate-execution mode and
will be ignored.)

`end`: end programming, which returns the device to immediate-execution mode.

`run count`: run the program _count_ times (0 < _count_ < 2^16). If _count_
is not specified, the program is run one time. At the end of the program runs,
the string "DONE\r\n" is transmitted to the host computer.

`\x80\xFF`: These two bytes will turn off the serial echo, which is convenient
for non-interactive use. If echo is off, sending these two bytes will NOT turn
echo back on, but the bytes will be echoed back. Thus the host can
unconditionally send these bytes, read back two bytes, and be guaranteed that
echo mode is now off, regardless of what state it started out in.

`!`: Break out of executing a script. If the exclamation-point character is
sent to the device, program execution immediately halts. The only exception is
that a `du` command will not halt until the full delay has elapsed. (This is
to permit the microsecond timer to have the best possible precision by not
having to poll the USB port during the interval.) This allows the host to
indefinitely run a script using a `go` loop, and then cancel out of the script
when required.
    
AVR and Arduino Pin Names
-------------------------
A compile-time option chooses between the AVR and Arduino pin names listed
below. If -DARDUINO\_PIN\_NAMES is added to the CC_FLAGS line in the Makefile,
then the Arduino names will be used. Otherwise the AVR names are default.

    AVR   Arduino   Comment on Arduino Pin
    B0    SS        Also Receive LED (Not connected to a pin on Leonardo)
    B1    SC        Marked SCK on Micro (in ICSP pin cluster on Leonardo)
    B2    MO        Marked MOSI on Micro (in ICSP pin cluster on Leonardo)
    B3    MI        Marked MISO on Micro (in ICSP pin cluster on Leonardo)
    B4    8         
    B5    9         
    B6    10        
    B7    11        
    C6    5         
    C7    13        Also bootloader LED on Arduino
    D0    3         
    D1    2         
    D2    RX        
    D3    TX        
    D4    4         
    D5    TL        Transmit LED, not connected to any pin on Arduino
    D6    12        
    D7    6         
    E6    7         
    F0    A5        
    F1    A4        
    F4    A3        
    F5    A2        
    F6    A1        
    F7    A0        
    
A Note on Debouncing Switches
-----------------------------
Mechanical switches do not usually transition cleanly, but produce a chatter of
digital 1s and 0s as they open and close. To get a clean reading from a switch,
a "debounce" routine is necessary. The "wait time" set by the `wt` command above
could be used to wait for a steady reading before returning a value. The maximum
wait time is 32 milliseconds, but if a switch bounces longer than that, two
`wh` or `wl` commands could be used back to back to require a clean pulse of
a longer length.

Alternately, one could wait for the leading edge of the switch closure (after
a minimal wait time to exclude spikes from electromagnetic interference), and 
then simply not look for switching events for the length of time the switch 
might bounce for (100 ms, say). For example, here's a simple program that toggles
a LED on pin B0 in accordance with a toggled switch on B1:

    wh B0
    sh B1
    dm 100
    wl B0
    sl B1
    dm 100
    go 0

By simply delaying 100 ms before waiting for the next switch event, the above 
script excludes the possibility of reading switch bounce mistakenly.

Program Step Execution Speed
----------------------------
On a chip clocked at 16 MHz, the following commands were timed as follows:

    wh: 9.0 µs + delay specified by wt + time waiting for signal
    wl: 9.0 µs + delay specified by wt + time waiting for signal
    rh: 6.7 µs + time waiting for signal
    rl: 6.7 µs + time waiting for signal
    dm: 10.6 µs + delay time
    du: 4.8 µs + delay time
    pm: 5.5 µs for 8-bit PWM and 5.8 µs for 10-bit
    sh: 6.0 µs
    sl: 6.0 µs
    st: 6.0 µs
    ct: ~22 µs depending, on USB bus
    lo: 6.0 µs + 5 µs overhead/iteration
    go: 2.9 µs

Porting to Another AVR Microcontroller
--------------------------------------
Porting this to another USB-enabled AVR microcontroller should be relatively
simple. All that is required is to edit the pin definitions in `src/pins.c` to
match the pins provided by the new microcontroller, and adjust the timer usage
in `interpreter.c` to match the capabilities and clock speed of the new chip.

On the ATmega32u4 clocked at 16 MHz, the following timers are used for internal
timing, delay timing, and PWM generation.

### Timer/Counter0 ###
Prescaler: 1 (62.5 ns/count)
Mode: Fast PWM 
Frequency: 8-bit at 62.5 ns/count = 62.5 kHz
OCR0A: used to define the PWM waveform on pin OC0A (B7)
OCR0B: used to define the PWM waveform on pin OC0B (D0)

### Timer/Counter1 ###
Prescaler: 1 (62.5 ns/count)
Mode: Fast PWM, 10-bit (ICR1 = 2**10, WGM13:0 bits set to 14)
Frequency: 10-bit at 62.5 ns/count = 15.625 kHz
OCR1A: used to define the PWM waveform on pin OC1A (B5)
OCR1B: used to define the PWM waveform on pin OC1B (B6)

### Timer/Counter3 ###
Prescaler: 8 (0.5 µs/count)
Mode: Normal
OCR3A: used for ms timer ISR, which increments register by 2000 each call
OCR3B: used for µs timer: set to desired delay time and then wait on OCF3B
OCR3C: used for USB task timer ISR, must be set to 60000 (30 ms) or less

### Timer/Counter4 ###
Prescaler: 2 (125 ns/count)
Mode: Fast PWM, 8-bit (OCR4C = 2**8)
Frequency: 8-bit at 125 ns/count = 31.25 kHz
OCR4A: used to define the PWM waveform on pin OC4A (C7)
OCR4D: used to define the PWM waveform on pin OC4D (D7)
