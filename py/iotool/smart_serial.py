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

import sys
import fcntl
import struct
import select
import os
import errno

import serial
import serial.serialposix as serialposix

SerialException = serial.SerialException
class SerialTimeout(SerialException):
    pass

class Serial(serialposix.PosixSerial):
    """Serial port class that differs from the python serial library in three
    key ways:
    (1) Read timeouts raise an exception rather than just returning less
    data than requested. This makes it easier to detect error conditions.
    (2) A blocking read() or read_until() command will not lose any data
    already read if a timeout or KeyboardInterrupt occurs during the read.
    Instead, the next time the read function is called, the data already
    read in will still be there.
    (3) A read_until() command is provided that reads from the serial port
    until some string is matched.
    """
    def __init__(self, port, baudrate=9600, timeout=None, **kwargs):
        self.read_buffer = b''
        super().__init__(port, baudrate=baudrate, timeout=timeout, **kwargs)

    def inWaiting(self):
        """Return the number of characters currently in the input buffer."""
        s = fcntl.ioctl(self.fd, serialposix.TIOCINQ, serialposix.TIOCM_zero_str)
        return struct.unpack('I',s)[0] + len(self.read_buffer)

    def read(self, size=1):
        """Read size bytes from the serial port. If a timeout occurs, an
           exception is raised. With no timeout it will block until the requested
           number of bytes is read. If interrupted by a timeout or
           KeyboardInterrupt, before 'size' bytes have been read, the pending
           bytes read will not be lost but will be available to subsequent read()
           calls."""
        if not self._isOpen: raise serialposix.portNotOpenError
        if len(self.read_buffer) > size:
            read_buffer = self.read_buffer
            try:
                self.read_buffer = read_buffer[size:]
                return read_buffer[:size]
            except KeyboardInterrupt as k:
                self.read_buffer = read_buffer
                raise k
        while len(self.read_buffer) < size:
            try:
                ready,_,_ = select.select([self.fd],[],[], self._timeout)
                # If select was used with a timeout, and the timeout occurs, it
                # returns with empty lists -> thus abort read operation.
                # For timeout == 0 (non-blocking operation) also abort when there
                # is nothing to read.
                if not ready:
                    raise SerialTimeout()   # timeout
                buf = os.read(self.fd, size-len(self.read_buffer))
                # read should always return some data as select reported it was
                # ready to read when we get to this point.
                if not buf:
                    # Disconnected devices, at least on Linux, show the
                    # behavior that they are always ready to read immediately
                    # but reading returns nothing.
                    raise SerialException('device reports readiness to read but returned no data (device disconnected or multiple access on port?)')
                self.read_buffer += buf
            except OSError as e:
                # because SerialException is a IOError subclass, which is a OSError subclass,
                # we could accidentally catch and re-raise SerialExceptions we ourselves raise earlier
                # which is a tad silly.
                if isinstance(e, SerialException):
                    raise

                # ignore EAGAIN errors. all other errors are shown
                if e.errno != errno.EAGAIN:
                    raise SerialException('read failed: %s' % (e,))

        read_buffer = self.read_buffer
        try:
            self.read_buffer = b''
            return read_buffer
        except KeyboardInterrupt as k:
            self.read_buffer = read_buffer
            raise k

    def read_all_buffered(self):
        return self.read(self.inWaiting())

    def read_until(self, match):
        """Read bytes from the serial until the sequence of bytes specified in
           'match' is read out. If a timeout is set and match hasn't been made,
           no bytes will be returned. With no timeout it will block until the
           match is made. If interrupted by a timeout or KeyboardInterrupt before
           the match is made, the pending bytes read will not be lost but will be
           available to subsequent read_until() calls."""
        if not self._isOpen: raise serialposix.portNotOpenError
        search_start = 0
        ml = len(match)
        while True:
            match_pos = self.read_buffer.find(match, search_start)
            if match_pos != -1:
                break
            search_start = len(self.read_buffer) - ml + 1
            try:
                ready,_,_ = select.select([self.fd],[],[], self._timeout)
                # If select was used with a timeout, and the timeout occurs, it
                # returns with empty lists -> thus abort read operation.
                # For timeout == 0 (non-blocking operation) also abort when there
                # is nothing to read.
                if not ready:
                    raise SerialTimeout()   # timeout
                s = fcntl.ioctl(self.fd, serialposix.TIOCINQ, serialposix.TIOCM_zero_str)
                in_waiting = struct.unpack('I',s)[0]
                buf = os.read(self.fd, max(in_waiting, ml))
                # read should always return some data as select reported it was
                # ready to read when we get to this point.
                if not buf:
                    # Disconnected devices, at least on Linux, show the
                    # behavior that they are always ready to read immediately
                    # but reading returns nothing.
                    raise SerialException('device reports readiness to read but returned no data (device disconnected or multiple access on port?)')
                self.read_buffer += buf
            except OSError as e:
                # because SerialException is a IOError subclass, which is a OSError subclass,
                # we could accidentally catch and re-raise SerialExceptions we ourselves raise earlier
                # which is a tad silly.
                if isinstance(e, SerialException):
                    raise

                # ignore EAGAIN errors. all other errors are shown
                if e.errno != errno.EAGAIN:
                    raise SerialException('read failed: %s' % (e,))

        read_buffer = self.read_buffer
        match_last = match_pos + ml
        try:
            self.read_buffer = read_buffer[match_last:]
            return read_buffer[:match_last]
        except KeyboardInterrupt as k:
            self.read_buffer = read_buffer
            raise k