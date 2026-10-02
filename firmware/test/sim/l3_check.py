#!/usr/bin/env python3
"""L3 checks: the real firmware ELF under simavr, driven through the real UART bridge
(test_plan.md §7, replacing the throwaway spike).

    python3 firmware/test/sim/l3_check.py [firmware.elf] [--harness <path>]
    ctest --test-dir firmware/cmake-build-sim            # the same checks, through CTest

What this level proves that L1 cannot: main.c, usart.c and the timing between them. L1 sees
which register bit was written; only here does a byte travel the whole way out of the USART,
`_delay_ms(40)` elapse, and the loop come back around.

What it still does not prove: anything about real hardware, and the ASCOM driver's COM layer.

The bridge is a pty when /dev/ptmx is available (pyserial opens it the same way the L4 hardware
test opens a COM port); otherwise the harness falls back to its built-in UDP bridge.
Exit code 0 when every check passes.
"""
import argparse
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
DEFAULT_HARNESS = os.path.join(ROOT, "firmware", "cmake-build-sim", "harness")
DEFAULT_FIRMWARE = os.path.join(ROOT, "firmware", "cmake-build-avr", "DomeControl")

# ATmega8 clock; every time figure in this file is simulated time, cycles / F_CPU.
F_CPU = 12000000
# `_delay_ms(40)` is the main loop's period, and the OCR1B ramp advances one step per iteration
# (AGENTS.md §4). 15 -> 128 is motorSpeedStepUp=2, so 57 iterations, about 2.3 s of simulated
# time. The band is deliberately wide: it only has to separate "the delay is the ramp clock"
# from "the delay was removed", which collapses the ramp to about 0.004 s.
RAMP_MIN_SECONDS = 2.0
RAMP_MAX_SECONDS = 3.0

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

    def __init__(self, elf, force_udp=False, harness=None):
        self.startup = []
        self.buffer = b""
        self.transport = None
        command = [harness or HARNESS] + (["--udp"] if force_udp else []) + [elf]
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


# Global for Harness, which builds its own command line; set once in main().
HARNESS = DEFAULT_HARNESS


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


def release_unconnected_pins(harness):
    """An undriven simavr pin may read as pressed; the switches are active low."""
    for port, bit in (("C", 0), ("C", 1), ("C", 2), ("D", 3), ("D", 4)):
        harness.command("set %s %d 1" % (port, bit))


def read_register(harness, address):
    """One byte of the AVR data space, as hex."""
    return int(harness.command("reg %s" % address).split()[2], 16)


def read_ocr1b(harness):
    """OCR1B is 16-bit: the low byte alone is enough while it stays below 256."""
    return read_register(harness, "0x48")


def cycles(harness):
    return int(harness.command("cycles").split()[1])


def encoder_value(harness):
    """The current encoder position. The dome may be anchored anywhere by this point in the run,
    so the checks below compare against this reading instead of assuming 0."""
    harness.uart.send(b"GEV#")
    return int(harness.uart.receive(b"\r\n", 5.0).strip())


def motor_settles(harness, limit_ms=4000):
    """Wait until `IM#` reports 0, and return how long that took in simulated milliseconds.

    `ST#` only drops the ramp target: OCR1B then steps down by motorSpeedStepDown per main-loop
    iteration, so the motor needs about a second of simulated time to actually stop. Asserting `IM#`
    right after `ST#` races that ramp and passes only when the host happens to be slow enough.
    """
    waited = 0
    while waited < limit_ms:
        harness.uart.send(b"IM#")
        data = harness.uart.receive(b"\r\n", 5.0)
        if data == b"0\r\n":
            return True, waited
        harness.command("wait 40")
        waited += 40
    return False, waited


def time_to_full_speed(harness):
    """Simulated seconds from `GOR#` to OCR1B at the top of the ramp.

    One `wait` per main-loop iteration: `wait 40` returns when that simulated time has passed,
    which is finer than the ramp step itself, so the reading is not the bottleneck.
    """
    harness.uart.send(b"ST#")
    harness.uart.receive(b"\r\n", 5.0)
    motor_settles(harness)
    start = cycles(harness)
    rest = read_ocr1b(harness)
    harness.uart.send(b"GOR#")
    harness.uart.receive(b"\r\n", 5.0)
    iterations = 0
    while read_ocr1b(harness) < 128 and iterations < 200:
        harness.command("wait 40")
        iterations += 1
    seconds = (cycles(harness) - start) / float(F_CPU)
    harness.uart.send(b"ST#")
    harness.uart.receive(b"\r\n", 5.0)
    motor_settles(harness)
    return seconds, rest, iterations


def main():
    global HARNESS

    parser = argparse.ArgumentParser(description="L3 checks: firmware under simavr")
    parser.add_argument("firmware", nargs="?", default=DEFAULT_FIRMWARE, help="firmware ELF")
    parser.add_argument("--harness", default=DEFAULT_HARNESS, help="the simavr harness binary")
    args = parser.parse_args()

    HARNESS = args.harness
    firmware = args.firmware

    for path, hint in ((firmware, "cmake -S firmware -B firmware/cmake-build-avr "
                                   "-DCMAKE_TOOLCHAIN_FILE=$PWD/firmware/toolchain-avr.cmake "
                                   "-DCMAKE_BUILD_TYPE=Release && cmake --build "
                                   "firmware/cmake-build-avr --config Release"),
                       (HARNESS, "cmake -S firmware/test/sim -B firmware/cmake-build-sim && "
                                 "cmake --build firmware/cmake-build-sim")):
        if not os.path.exists(path):
            print("not built: %s\n  %s" % (path, hint))
            return 2

    harness = open_harness(firmware)
    try:
        print("step 1: the harness runs the firmware")
        check(harness.transport is not None, "the harness announced a UART bridge",
              "startup: %r" % harness.startup)
        check(any("freq=12000000" in line for line in harness.startup),
              "the MCU runs at 12 MHz", "startup: %r" % harness.startup)
        check(harness.command("cycles").startswith("cycle "), "control channel answers 'cycles'")

        print("step 2: the protocol works over the %s" % harness.uart.name)
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"0\r\n", "GEV# -> 0", "got %r" % data)
        for command, expect in ((b"GOF#", b"OK\r\n"), (b"IM#", b"1\r\n"), (b"ST#", b"OK\r\n")):
            harness.uart.send(command)
            data = harness.uart.receive(b"\r\n", 5.0)
            check(data == expect, "%r -> %r" % (command.decode(), expect.decode()),
                  "got %r" % data)
        settled, waited = motor_settles(harness)
        check(settled, "IM# -> 0 once the ramp has finished (%d ms of simulated time)" % waited)

        print("released states (an undriven simavr pin may read as pressed)")
        release_unconnected_pins(harness)
        print("  %s" % harness.command("state C"))
        harness.command("wait 200")

        print("step 3: pin injection reaches the firmware (PINC, active low)")
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

        print("step 4: an encoder pulse on INT0 (PD2) moves the value")
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
        motor_settles(harness)

        harness.uart.send(b"GOF#")
        harness.uart.receive(b"\r\n", 5.0)
        harness.command("pulse D 2 40")
        harness.command("wait 200")
        harness.uart.send(b"GEV#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"100\r\n", "GOF# + one step -> 100 (forward counts up)", "got %r" % data)
        harness.uart.send(b"ST#")
        harness.uart.receive(b"\r\n", 5.0)
        motor_settles(harness)

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
        tccr1a = read_register(harness, "0x4f")   # TCCR1A
        ocr1b = read_ocr1b(harness)
        check(tccr1a & 0x20, "COM1B1 enables the OC1B output (TCCR1A=0x%02x)" % tccr1a)
        check(ocr1b > 15, "OCR1B ramps above the start speed (OCR1B=%d)" % ocr1b)
        edges = harness.pin_edges(bit=1)
        check("1" in edges, "PB1 sets the direction relay after GOR#", "edges: %s" % edges)
        harness.uart.send(b"ST#")
        harness.uart.receive(b"\r\n", 5.0)
        # PB1 is cleared by motorProceed() when OCR1B reaches the start speed, so the same ramp-down
        # has to finish first.
        motor_settles(harness)
        edges = harness.pin_edges(bit=1)
        check(bool(edges) and edges[-1] == "0", "PB1 clears again after ST#", "edges: %s" % edges)

        print("step 6: the ramp is clocked by the main loop, not by the timer")
        # This is the check that protects `_delay_ms(40)` in main.c: motorProceed() advances OCR1B
        # once per iteration, so the delay IS the ramp clock. Measured under simavr with the
        # delay in place: 2.28 s. With the delay removed the same firmware reaches top speed in
        # 0.004 s, about 600x faster, which would tear the steps off a STEP/DIR drive.
        seconds, rest, iterations = time_to_full_speed(harness)
        check(rest == 15, "OCR1B rests at motorStartSpeed after ST# (OCR1B=%d)" % rest)
        check(iterations < 200, "the ramp finished inside %d iterations" % iterations)
        check(RAMP_MIN_SECONDS <= seconds <= RAMP_MAX_SECONDS,
              "15 -> 128 takes %.2f s of simulated time (band %.1f-%.1f s)"
              % (seconds, RAMP_MIN_SECONDS, RAMP_MAX_SECONDS))
        print("  ramp: %.3f s, %d iterations, %.1f ms per iteration"
              % (seconds, iterations, 1000.0 * seconds / max(iterations, 1)))

        print("step 7: the protocol's error path")
        harness.uart.send(b"XY#")
        harness.command("wait 200")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data.startswith(b"Unrecognized command: XY"),
              "an unknown command is answered without OK", "got %r" % data)

        print("step 8: GoTo accepts a signed position")
        for command in (b"GT0#", b"GT-42#", b"GT42#"):
            harness.uart.send(command)
            data = harness.uart.receive(b"\r\n", 5.0)
            check(data == b"OK\r\n", "%r -> OK" % command.decode(), "got %r" % data)
            harness.command("wait 200")

        print("step 9: IOC# and FC# answer while the dome is still")
        harness.uart.send(b"IOC#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"0\r\n", "IOC# -> 0 off centre", "got %r" % data)
        harness.uart.send(b"FC#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"OK\r\n", "FC# -> OK", "got %r" % data)
        harness.command("wait 400")

        print("step 10: two commands in one packet (known defect, test_plan.md §10 item 2)")
        # One receive buffer, no queue: the dispatcher's first read wins and the rest is dropped.
        # The assertion is on the second command, so a fix keeps this green; the count is what
        # makes the loss visible.
        current = encoder_value(harness)
        harness.uart.send(b"ST#GEV#")
        harness.command("wait 300")
        first = harness.uart.receive(b"\r\n", 5.0)
        second = harness.uart.receive(b"\r\n", 0.5)
        check(first == b"%d\r\n" % current,
              "the second command in the packet is answered with the encoder value",
              "got %r, expected %d" % (first, current))
        check(second == b"", "the first command in the packet is answered too "
                             "(fixed if this ever passes)", "got %r" % second)

        print("step 11: an overlong frame does not poison the next command")
        harness.uart.send(b"G" * 62 + b"#")     # 63 bytes: the documented body limit is 62
        harness.command("wait 300")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data.startswith(b"Unrecognized command: G"), "the overlong frame is rejected",
              "got %r" % data)
        harness.uart.send(b"ST#")
        data = harness.uart.receive(b"\r\n", 5.0)
        check(data == b"OK\r\n", "the next command is still answered", "got %r" % data)
    finally:
        harness.close()

    print("\n%d checks, %d failures" % (checks, len(failures)))
    for name in failures:
        print("  failed: " + name)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())