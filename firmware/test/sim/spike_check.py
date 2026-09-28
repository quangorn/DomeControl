#!/usr/bin/env python3
"""L3 spike checks (test_plan.md §7.4, steps 2-5).

Drives the firmware ELF through firmware/test/sim/harness.c:
  2. the harness builds and the firmware runs under simavr;
  3. `GEV#` reaches the firmware and the answer comes back over the UART bridge;
  4. driving a limit-switch pin low re-anchors the encoder (`PINC` injection);
  5. `GOF#` makes OC1B/PB2 switch, and the transitions are visible on the pin hook.

The bridge is a pty when /dev/ptmx is available (pyserial opens it the same way the L4 hardware test
opens a COM port); otherwise the harness falls back to its built-in UDP bridge.
Usage: python3 firmware/test/sim/spike_check.py [firmware.elf]
Exit code 0 when every check passes. Not a ctest yet: the L3 suite replaces it.
"""
import os
import re
import select
import socket
import subprocess
import sys
import time

import serial

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")

HERE = os.path.dirname(os.path.abspath(__file__))                    # firmware/test/sim
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))       # repository root
HARNESS = os.path.join(ROOT, "firmware", "cmake-build-sim", "harness")
FIRMWARE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    ROOT, "firmware", "cmake-build-avr", "DomeControl")

checks = 0
failures = []


def check(ok, what, detail=""):
    global checks
    checks += 1
    print(("  PASS  " if ok else "  FAIL  ") + what + ((" -- " + detail) if detail and not ok else ""),
          flush=True)
    if not ok:
        failures.append(what)


class PtyTransport:
    def __init__(self, path):
        self.name = "pty %s" % path
        self.serial = serial.Serial(path, 115200, timeout=5.0)

    def send(self, data):
        self.serial.write(data)

    def receive(self, expect, timeout):
        self.serial.timeout = timeout
        return self.serial.read_until(expect)

    def close(self):
        self.serial.close()


class UdpTransport:
    """One datagram per byte, as sent by the harness bridge."""

    def __init__(self, port):
        self.name = "udp %d" % port
        self.buffer = b""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.connect(("127.0.0.1", port))
        self.socket.settimeout(5.0)

    def send(self, data):
        self.socket.sendall(data)

    def receive(self, expect, timeout):
        deadline = time.time() + timeout
        while expect not in self.buffer:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            self.socket.settimeout(remaining)
            try:
                self.buffer += self.socket.recv(4096)
            except socket.timeout:
                break
        end = self.buffer.find(expect)
        if end < 0:
            data, self.buffer = self.buffer, b""
            return data
        end += len(expect)
        data, self.buffer = self.buffer[:end], self.buffer[end:]
        return data

    def close(self):
        self.socket.close()


class Harness:
    """The control channel is lines on stdin/stdout; the firmware UART is the transport."""

    def __init__(self, elf, force_udp=False):
        self.startup = []
        self.buffer = b""
        self.transport = None
        command = [HARNESS] + (["--udp"] if force_udp else []) + [elf]
        self.proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)
        self.fd = self.proc.stdout.fileno()
        self.pty_failed = False
        self._read_until_ready()

    def line(self, timeout=10.0):
        """Return one control-channel reply: simavr's own output shares stdout, so replies are
        marked with `@` and anything else is skipped."""
        deadline = time.time() + timeout
        while True:
            while b"\n" not in self.buffer:
                remaining = deadline - time.time()
                if remaining <= 0 or not select.select([self.fd], [], [], remaining)[0]:
                    raise TimeoutError("no reply from the harness within %.1fs" % timeout)
                chunk = os.read(self.fd, 4096)
                if not chunk:
                    raise TimeoutError("the harness exited")
                self.buffer += chunk
            raw, _, self.buffer = self.buffer.partition(b"\n")
            text = ANSI.sub("", raw.decode(errors="replace"))
            marker = text.find("@")
            if marker >= 0:
                return text[marker + 1:]

    def _read_until_ready(self):
        while True:
            line = self.line()
            self.startup.append(line)
            if line == "uart pty failed":
                self.pty_failed = True
            match = re.search(r"^uart (pty|udp) (\S+)$", line)
            if match:
                self.transport = (match.group(1), match.group(2))
            if line.startswith("harness ready"):
                return

    def command(self, text, timeout=10.0):
        """Send a control command, return its reply line."""
        self.proc.stdin.write(("%s\n" % text).encode())
        self.proc.stdin.flush()
        return self.line(timeout)

    def pin_edges(self, bit):
        """Ask for the recorded pin transitions, return those of one bit."""
        self.proc.stdin.write(b"log\n")
        self.proc.stdin.flush()
        header = self.line()
        count = int(header.split()[1])
        edges = []
        for _ in range(count):
            _cycle, edge_bit, value = self.line()[2:].split(":")
            if edge_bit == str(bit):
                edges.append(value)
        return edges

    def close(self):
        for closable in (getattr(self, "uart", None),):
            if closable is not None:
                try:
                    closable.close()
                except Exception:
                    pass
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()


def open_harness(elf):
    """Prefer the pty (parity with hardware); fall back to the UDP bridge."""
    harness = Harness(elf)
    if not harness.pty_failed:
        kind, path = harness.transport
        harness.uart = PtyTransport(path)
        return harness
    harness.close()
    harness = Harness(elf, force_udp=True)
    kind, path = harness.transport
    harness.uart = UdpTransport(int(path))
    return harness


def main():
    for path, hint in ((FIRMWARE, "cmake -S firmware -B firmware/cmake-build-avr "
                                  "-DCMAKE_TOOLCHAIN_FILE=<avr toolchain> -DCMAKE_BUILD_TYPE=Release"),
                       (HARNESS, "cmake -S firmware/test/sim -B firmware/cmake-build-sim && "
                                 "cmake --build firmware/cmake-build-sim")):
        if not os.path.exists(path):
            print("not built: %s\n  %s" % (path, hint))
            return 2

    harness = open_harness(FIRMWARE)
    try:
        print("step 2: harness runs the firmware")
        check(harness.transport is not None, "the harness announced a UART bridge",
              "startup: %r" % harness.startup)
        check(any("freq=12000000" in line for line in harness.startup),
              "the MCU runs at 12 MHz", "startup: %r" % harness.startup)
        check(harness.command("cycles").startswith("cycle "), "control channel answers 'cycles'")

        print("step 3: the protocol works over the %s" % harness.uart.name)
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"0\r\n", "GEV# -> 0", "got %r" % data)
        for command, expect in ((b"GOF#", b"OK\r\n"), (b"IM#", b"1\r\n"), (b"ST#", b"OK\r\n"),
                                (b"IM#", b"0\r\n")):
            harness.uart.send(command)
            data = harness.uart.receive(b"\r\n", 5.0)
            check(data == expect, "%r -> %r" % (command.decode(), expect.decode()),
                  "got %r" % data)

        print("released states (an undriven simavr pin may read as pressed)")
        for port, bit in (("C", 0), ("C", 1), ("C", 2), ("D", 3), ("D", 4)):
            harness.command("set %s %d 1" % (port, bit))
        print("  %s" % harness.command("state C"))

        print("step 4: pin injection reaches the firmware (PINC, active low)")
        harness.command("wait 200")
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"0\r\n", "nothing pressed -> 0", "got %r" % data)
        harness.command("set C 0 0")            # press the forward limit
        harness.command("wait 200")
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"100\r\n", "forward limit -> re-anchor to 100", "got %r" % data)
        harness.command("set C 0 1")            # release: the anchor is edge triggered
        harness.command("wait 200")
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"100\r\n", "a held level must not re-anchor", "got %r" % data)

        print("step 4b: an encoder pulse on INT0 (PD2) moves the value")
        # main.c passes the DIRECTION_* constants, so `GOF#` counts the encoder up and `GOR#` counts
        # it down, the same way the buttons and motorGoTo() do. The dome sits on the +100 forward
        # limit anchor here, and motorProceed() stops a forward move at that value, so the first
        # command walks the encoder off the limit and the second walks it back.
        harness.uart.send(b"GOR#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"OK\r\n", "GOR# -> OK", "got %r" % data)
        harness.command("pulse D 2 40")         # `ok` comes back when the pin is released
        harness.command("wait 200")
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"99\r\n", "GOR# + one step from the +100 anchor -> 99 (reverse counts down)",
              "got %r" % data)
        harness.uart.send(b"ST#")
        harness.uart.receive(b"\r\n", 5.0)
        harness.command("wait 400")

        harness.uart.send(b"GOF#")
        harness.uart.receive(b"\r\n", 5.0)
        harness.command("pulse D 2 40")
        harness.command("wait 200")
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"100\r\n", "GOF# + one step -> 100 (forward counts up)", "got %r" % data)
        harness.uart.send(b"ST#")
        harness.uart.receive(b"\r\n", 5.0)
        harness.command("wait 400")

        print("step 5: the motor output is driven (registers) and the relay is visible on the pin")
        harness.command("logclear")
        # GOR# (reverse) is the command that moves here: the encoder sits on the +100 forward limit
        # anchor, so GOF# would be stopped by motorProceed() before it ramps, and reverse is the
        # direction whose relay state (PB1 = 1) is the one worth watching.
        harness.uart.send(b"GOR#")
        harness.uart.receive(b"\r\n", 5.0)
        harness.command("wait 300")
        # simavr raises no Timer1 compare-output events and never drives PB2, so the PWM waveform
        # itself is not observable here; the direction relay (PB1) is an ordinary port pin and is.
        tccr1a = int(harness.command("reg 0x4f").split()[2], 16)   # TCCR1A
        ocr1b = int(harness.command("reg 0x48").split()[2], 16)    # OCR1BL
        check(tccr1a & 0x20, "COM1B1 enables the OC1B output (TCCR1A=0x%02x)" % tccr1a)
        check(ocr1b > 15, "OCR1B ramps above the start speed (OCR1B=%d)" % ocr1b)
        edges = harness.pin_edges(bit=1)
        check("1" in edges, "PB1 sets the direction relay after GOR#", "edges: %s" % edges)
        harness.uart.send(b"ST#")
        harness.uart.receive(b"\r\n", 5.0)
        harness.command("wait 400")
        edges = harness.pin_edges(bit=1)
        check(bool(edges) and edges[-1] == "0", "PB1 clears again after ST#", "edges: %s" % edges)
    finally:
        harness.close()

    print("\n%d checks, %d failures" % (checks, len(failures)))
    for name in failures:
        print("  failed: " + name)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())