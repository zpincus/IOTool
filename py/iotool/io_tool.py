# The MIT License (MIT)
#
# Copyright (c) 2014-2015 WUSTL ZPLAB
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# Authors: Zach Pincus

import time

from . import commands
from . import smart_serial

_ECHO_OFF = b'\x80\xFF'

class IOTool:
    """Class to control IOTool box. See https://github.com/zachrahan/IOTool for
    documentation about the IOTool microcontroller firmware itself, but in this
    case the microcontroller is used to drive and time TTL/PWM signals to various
    microscope hardware."""
    def __init__(self, serial_port_name):
        self._serial_port_name = serial_port_name
        try:
            self.reset()
        except (smart_serial.SerialTimeout, RuntimeError):
            # explicitly clobber traceback from SerialTimeout exception
            raise smart_serial.SerialException('Could not communicate with IOTool device -- is it attached?')
        self.commands = commands.Commands()
        self._serial_port.setTimeout(None) # change to infinite time-out once initialized and in known-good state,
        # so that waiting for IOTool replies won't cause timeouts

    def reset(self):
        """Attempt to reset the IOTool device to a known-good state."""
        if hasattr(self, '_serial_port'):
            del self._serial_port
        self._serial_port = smart_serial.Serial(self._serial_port_name, timeout=1)
        self._serial_port.write(b'!\nreset\n')
        time.sleep(0.5) # give it time to reboot
        self._serial_port = smart_serial.Serial(self._serial_port_name, timeout=1)
        self._serial_port.write(_ECHO_OFF + b'\n') # disable echo
        echo_reply = self._wait_for_ready_prompt()
        assert echo_reply == _ECHO_OFF + b'\r\n' # read back echo of above (no further echoes will come)
        self._assert_empty_buffer()

    def execute(self, *commands):
        """Run a series of commands on the IOTool microcontroller."""
        self._assert_empty_buffer()
        responses = []
        for command in commands:
            self._serial_port.write((command+'\n').encode('ascii'))
            response = self.wait_until_done() # see if there was any output
            responses.append(response if response else None)
        if len(commands) == 1:
            responses = responses[0]
        self._assert_empty_buffer()
        return responses

    def _assert_empty_buffer(self):
        """Verify that there is no IOTool output that should have been read previously."""
        buffered = self._serial_port.read_all_buffered()
        if buffered:
            raise RuntimeError('Unexpected IOTool output: {}'.format(buffered.decode('ascii')))

    def store_program(self, *commands):
        """Send a list of commands to IOTool to run as a program, but do not
        run the program yet."""
        all_commands = ['program'] + list(commands) + ['end']
        responses = self.execute(*all_commands)
        errors = ['{}: {}'.format(command, response) for command, response in zip(all_commands, responses) if response is not None]
        if errors:
            raise RuntimeError('Program errors:\n'+'\n'.join(errors))

    def start_program(self, *commands, iters=1):
        """Run a program a given number of times. If no commands are given here,
        a program should have been stored previously with store_program().
        """
        if commands:
            self.store_program(*commands)
        else:
            self._assert_empty_buffer()
        self._serial_port.write('run {}\n'.format(iters).encode('ascii'))

    def wait_for_serial_char(self):
        """If a program uses the char_transmit command to send a signal to the
        host computer, this function can be used to wait to receive that signal."""
        self._serial_port.read(1)

    def send_serial_char(self, char):
        """If a program uses the char_receive command to wait for a signal from
        the host computer, this function can be used to send that signal."""
        self._serial_port.write(char.encode('ascii'))

    def _wait_for_ready_prompt(self):
        return self._serial_port.read_until(b'>')[:-1]

    def wait_until_done(self):
        """Wait for a command run via execute() or a program started with
        start_program() to terminate, and return any serial data produced by
        that program. A keyboard interrupt while waiting will force-terminate
        the program/command on the IOTool device, via stop()"""
        try:
            return self._wait_for_ready_prompt().decode('ascii')
        except KeyboardInterrupt as k:
            self.stop()
            raise k

    def stop(self):
        """Force-terminate a running program or single-command execution on the
        IOTool device, and wait to confirm that it actually stops."""
        # if a command is running, the below will generate two ready prompts:
        # one from breaking out with '!' and one from the newline.
        # If no command is running, this will generate an error due to the
        # out-of-place '!', and then a ready prompt.
        self._serial_port.write(b'!\n')
        self._wait_for_ready_prompt()
        # wait a bit to see if a second prompt is going to appear,
        # and then clear the buffer. If
        time.sleep(0.1)
        buffered = self._serial_port.read_all_buffered()
        assert buffered in {b'', b'>'} # we should have either gotten nothing or the second ready prompt
