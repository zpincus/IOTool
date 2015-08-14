IOTool
======
IOTool is a firmware for AVR microcontrollers that provides a simple command
interface over a USB serial port, allowing for a host computer to script
and sequence TTL input and output actions.

This firmware was originally developed for running laboratory experiments in
which multiple hardware devices need to signal one another via TTL, and where
the experimental parameters need to be rapidly re-configurable from the
controlling computer.

In general, IOTool may be useful for any high-speed sequencing of inputs and
outputs controlled by a host computer. There is no provision for branching
logic, or autonomous operation unattached from a computer. In such cases,
Arduino or AVR-GCC code is a better choice, as IOTool does not aim to provide
a Turing-complete interpreted language for a microcontroller. The aim is
simply to allow a host computer to script basic GPIO operations using a
microcontroller as the TTL interface.

Developed by [Zachary Pincus](zplab.wustl.edu), and provided under a GPL2 license.

Basic Usage Example
-------------------
0. Connect the device to a USB port and note the serial port that appears
   on your system. (On Windows this will require the IOTool.inf file.)

1. Connect to that serial port in your favorite programming environment
   or with a terminal program such as hyperterminal or minicom.

2. Send the following text to the device (assuming it's configured for the
   Arduino pin naming scheme, as described below):

        program
        sh 13
        dm 500
        sl 13
        dm 500
        lo 0 9
        end
   This stores a program to set pin 13 high (turning on the Arduino's onboard
   LED), delay 500 ms, set the pin low (turning the LED off), delay 500 ms, and
   then repeat that from the first step 9 more times.

3. Send the text `run` followed by a newline (return key), which will
   cause the LED to blink 10 times.

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

However, it should be possible with minimal effort to port IOTool to any other
AVR microcontroller supported by the LUFA USB library (see 'Porting' below).

Installation
------------
0.  Obtain the [LUFA USB library for AVR microcontrollers](http://www.fourwalledcubicle.com/LUFA.php)
    AVR-GCC is also required: [source](http://www.nongnu.org/avr-libc/), [Mac](http://www.obdev.at/products/crosspack)
    [Windows](http://sourceforge.net/projects/winavr).

1.  Configure the IOTool Makefile as follows:

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
    wt d        set debounce wait time: uint16 µs delay
    rd p        read digital TTL value: pin name
    ra p        read analog value: pin name
    dm d        delay ms: uint16 ms delay
    du d        delay µs: uint16 µs delay
    tb          begin timing
    te          end timing and output elapsed time in µs (~65 sec max time)
    sh p        set high: pin name
    sl p        set low: pin name
    st p        set high-impedance "tri-state": pin name
    pm p v      set PWM: pin name, uint8 or uint16 value
    ct b        character transmit: uint8 byte
    cr          character receive
    lo i c      loop: uint8 index, uint16 count
    go i        goto: uint8 index
    no          no-op

    program     start programming, clearing previous
    end         end programming, return to immediate-execution mode
    run c       run program: uint16 count (optional, defaults to one run)
    \x80\xFF    turn serial echo off for non-interactive use
    !           break out of currently executing run
    reset       hard-reset the microcontroller (jumping back to the bootloader,
                if one is present).
    aref        set analog reference to aref pin
    avcc        set analog reference to Vcc (5V)
(See section below on pin names for further details.)

### PWM-capable pins ###
     8-bit PWM @ 62.500 kHz: pins B7 and D0 (AVR) / 11 and 3 (Arduino)
     8-bit PWM @ 31.250 kHz: pins C7 and D7 (AVR) / 13 and 6 (Arduino)
    10-bit PWM @ 15.625 kHz: pins B5 and B6 (AVR) / 9 and 10 (Arduino)

### Detailed command description ###
**Wait for specific pin value:** `wh pin` (wait for high), `wl pin` (wait for
low), and `wc pin` (wait for change), where _pin_ is a one- or two-character
pin name (either the AVR port-letter + pin-number format, or the arduino
format; described below). These commands wait for a stable reading on a given
pin of a high, low, or different-from-current value (respectively). A "stable"
reading is defined as pulse lasting longer than the currently set wait time,
which defaults to 10 µs. This provides protection against stray
electromagnetic interference. Note that the specified pin is set to input and
the internal pull-up resistor is enabled before readings are taken. To disable
the stability check, set the wait time to 0 (see below).

**Set waiting time for stable readings:** `wt time`, where 0 < _time_ < 2^15,
in microseconds. The `wh`, `wl`, and `wc` commands wait for a pulse to read at
the desired level for at least this many µs before returning. The default is
10 µs.

**Read a pin's value**: `rd pin` (read digital) and `ra pin` (read analog) read
the value on a given pin and output the value as `0` or `1` for digital reads,
or a value in the range [0, 1023] for analog. The analog value returned equals
(1023*v-pin)/v-ref, where v-pin is the voltage on the pin, and v-ref is the
reference voltage chosen by the `aref` or `avcc` commands (see below). For
digital reads, the wait time (above) is used to determine whether the pin is
stable before returning a reading. To disable this check, set wait time to 0.
NB: Not all pins can be used for analog input (see pin information below).

**Delay a given interval:** `dm ms` (delay milliseconds) and `du us` (delay
micoseconds), where 0 ≤ _ms_ < 2^16 and 0 ≤ _us_ < 2^15. Note: these
delay times are for a chip clocked at 16 MHz. For other clock speeds, adjust
accordingly, or alter the timer counter values (described in the 'Porting'
section below).

**Timing:** `tb` (begin timing) and `te` (end timing). The interval (in
microseconds) between `tb` and `te` is output. The maximum timer value is
65567767 microseconds before before overflowing back to zero. NB: back-to-back
`tb` and `te` commands will measure about 4 µs of overhead on a 16 MHz chip.
To measure a high pulse on a pin, for example, run the program: `wh` `tb` `wl`
`te`. Pulses as short as 12 µs can be measured accurately in this context, as
there is some overhead associated with `wh` and `wl` as well (see timing data
below). **NB:** the hardware timers used for these functions are also used for
the delay commands above. As such, delay commands within a timing interval will
essentially randomize the reported timer values, and should not be used.

**Set a pin's value:** `sh pin` (set high), `sl pin` (set low), and `st pin`
(set tristate), where _pin_ is a one- or two-character pin name. The effects
of setting high and low are obvious. Setting a pin to tri-state
(high-impedance) means that it will not drive a circuit in any direction. (The
pin is set to input and the internal pull-up resistor is disabled.)

**Enable PWM on a pin:** `pm pin value`, where _pin_ is a one- or
two-character pin name, and _value_ is a PWM duty cycle in either an 8-bit (0
≤ _value_ < 2^8) or 10-bit (0 ≤ _value_ < 2^10) range, depending on the pin
specified. There are six PWM capable pins: two 8-bit pins with a PWM frequency
of 62.5 kHz (AVR pins B7 and D0, and Arduino pins 11 and 3), two 8-bit pins at
31.25 kHz (AVR C7 and D7 / Arduino 13 and 6), and two 10-bit pins at 15.625
kHz (AVR B5 and B6 / Arduino 9 and 10). Note: these PWM frequencies are for a
chip clocked at 16 MHz. For other clock speeds, adjust accordingly.

**Send and Receive Serial Data to/from Host:** `cr` (character receive) and `ct
value` (character transmit), where 0 ≤ value < 2^8. These commands are
useful for synchronizing script execution with the host computer. If the `cr`
command is issued, execution halts until a single byte is sent from the
computer host to the microcontroller. The contents of this byte are discarded
and not echoed back to the host. When `ct` commands are run, a single byte
with the value specified (from 0 to 255) will be transmitted to the host.

**Repeat Commands:** `lo index count` (loop back), and `go index` (goto),
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
is not specified, the program is run one time.

`\x80\xFF`: These two bytes will turn off the serial echo, which is convenient
for non-interactive use. If echo is off, sending these two bytes will NOT turn
echo back on, but the bytes and `\r\n` will be echoed back. Thus the host can
unconditionally send these bytes, read back four bytes, and be guaranteed that
echo mode is now off, regardless of what state it started out in. Regardless of
echo mode, the '>' prompt will be written after each command is executed.

`!`: Break out of executing a script. If the exclamation-point character is
sent to the device, program execution immediately halts. The only exception is
that a `du` command will not halt until the full delay has elapsed. (This is
to permit the microsecond timer to have the best possible precision by not
having to poll the USB port during the interval.) This allows the host to
indefinitely run a script using a `go` loop, and then cancel out of the script
when required.

`reset`: Perform a hard-reset of the device (via the watchdog timer), which
will put the device in a known-good state. Sending the string `!\nreset\n`, and
then waiting for the device's serial port to disappear and re-appear will
guarantee that the device is has started from scratch.

`aref` and `avcc`: Set whether the analog reference voltage is defined by the
Aref pin, or by the internal Vcc (usually 5V).

AVR and Arduino Pin Names
-------------------------
A compile-time option chooses between the AVR and Arduino pin names listed
below. If the `ARD_PINS` line is uncommented in the Makefile, then the Arduino
pin names will be used. Otherwise the AVR names are default.

    AVR  Arduino  PWM?  Analog?  Comment on Arduino Pin
    B0   SS                      Also Receive LED / Not connected to a pin on Leonardo
    B1   SC                      Marked SCK on Micro / In ICSP pin cluster on Leonardo
    B2   MO                      Marked MOSI on Micro / In ICSP pin cluster on Leonardo
    B3   MI                      Marked MISO on Micro / In ICSP pin cluster on Leonardo
    B4   8                Y
    B5   9        (3)     Y
    B6   10       (3)     Y
    B7   11       (1)
    C6   5
    C7   13       (2)            Also bootloader LED on Arduino
    D0   3        (1)
    D1   2
    D2   RX
    D3   TX
    D4   4                Y
    D5   TL                      Transmit LED, not connected to a pin on Arduino
    D6   12               Y
    D7   6        (2)     Y
    E6   7
    F0   A5               Y
    F1   A4               Y
    F4   A3               Y
    F5   A2               Y
    F6   A1               Y
    F7   A0               Y

    (1)  8-bit @ 62.500 kHz
    (2)  8-bit @ 31.250 kHz
    (3) 10-bit @ 15.625 kHz

Program Step Execution Speed
----------------------------
On a chip clocked at 16 MHz, the following commands were timed as follows:

    wh/wl: 7.2 µs + time waiting for signal (wt = 0)
    wh/wl: 10.4 µs + delay specified by wt + time waiting for signal (wt > 0)
    dm: 15 µs + delay time
    du: 4.5 µs + delay time
    pm: 5.4 µs for 8-bit PWM and 5.7 µs for 10-bit
    sh/sl/st: 5.8 µs
    ct: >17 µs (variability due to USB bus)
    rd: >30 µs + delay specified by wt (variability due to USB bus)
    ra: >90 µs (variability due to USB bus and number of digits returned)
    tb: 5.2 µs
    te: >52 µs (variability due to USB bus and number of digits returned)
    lo: 5.4 µs + 5 µs overhead per iteration
    go: 2.9 µs
    no: 2.6 µs

**Testing methodology:** the following program was run, using an Adafruit
 ATmega32u4 Breakout+, with an oscilloscope attached to pin D6.

    program
    sh D6
    <TEST COMMAND>
    lo 1 1000
    sl D6
    du 500
    go 0
    end

The width of the "high" pulses on pin D6 were measured for all of the commands
listed, and subtracted from the pulse width with no command at all. For `wh`
and `wl`, the test pin was attached to Vcc or Gnd respectively, so there should
be no actual waiting.

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
    Mode: Fast PWM, 10-bit (ICR1 = 2^10, WGM13:0 bits set to 14)
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
    Mode: Fast PWM, 8-bit (OCR4C = 2^8)
    Frequency: 8-bit at 125 ns/count = 31.25 kHz
    OCR4A: used to define the PWM waveform on pin OC4A (C7)
    OCR4D: used to define the PWM waveform on pin OC4D (D7)

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
a LED on pin B1 in accordance with a toggled switch on B0:

    wh B0
    sh B1
    dm 100
    wl B0
    sl B1
    dm 100
    go 0

By simply delaying 100 ms before waiting for the next switch event, the above
script excludes the possibility of reading switch bounce mistakenly.


### Python Module ###
A simple python module is included which eases communication with an IOTool
device. The module works with Python 3 but is likely compatible with version 2
as well. The module requires the [PySerial library](https://pypi.python.org/pypi/pyserial), and is currently only
compatible with OS X and Linux.

Example usage:

    import iotool
    device = iotool.IOTool('/dev/ttyWhatever')
    device.execute('dm 100', 'sh B1') # delay 100 ms
    device.execute(iotool.delay_ms(100), iotool.set_high('B1')) # equivalent
    # set up a program that waits for a character, then sends a character
    device.store_program(iotool.char_receive(), iotool.char_transmit('D'))
    # run the program, then send a character to the IOTool and wait for a reply
    device.start_program()
    device.send_serial_char('F')
    print(device.wait_for_serial_char())
    # wait until the program has finished running
    device.wait_until_done()
