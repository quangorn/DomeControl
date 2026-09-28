/*
 * DomeControl L3 harness: runs the real firmware ELF under simavr.
 *
 * The firmware USART is bridged to the outside world so that the same protocol test can run against
 * the emulator and against the real COM port:
 *
 *   harness <firmware.elf>            the bridge is a pty (uart_pty), announced as `uart pty <path>`
 *   harness --udp <firmware.elf>      the bridge is the built-in UDP bridge, `uart udp <port>`
 *
 * The pty comes from libsimavrparts, but it needs /dev/ptmx, which sandboxes usually deny; the
 * built-in UDP bridge has no such dependency. `uart_udp` from libsimavrparts is *not* used: it only
 * drains its host-to-AVR FIFO on a UART XON event, and this firmware enables RX once and then never
 * polls UCSRA, so the first byte would sit in that FIFO forever.
 *
 * The control channel is stdin/stdout, one command per line, one reply per line:
 *
 *   wait <ms>              advance simulated time, reply `ok <cycle>` when reached
 *   set <port> <bit> <v>   drive an input pin (v = 0 pulls it low: limits/buttons are active low)
 *   pulse <port> <bit> <ms>  drive low, release after <ms>, reply `ok` on release
 *   state <port>           reply `state <P> <PORT> <DDR> <PIN>` (hex)
 *   reg <addr>             reply `reg <addr> <value>`: a byte of the AVR data space (hex)
 *   uart <byte>            inject one byte into the USART receive IRQ, bypassing the bridge
 *   watch <port>           record output transitions of that port, reply `ok`
 *   pwm <timer> <channel>  record compare-output transitions of a timer channel; they appear in the
 *                          log as bit 8+channel (simavr does not route OC1B to the ATmega8 pin)
 *   log                    reply `log <n>` then `l <cycle>:<bit>:<value>` lines
 *   logclear               drop the recorded transitions, reply `ok`
 *   cycles                 reply `cycle <n>`
 *   quit                   stop
 *
 * Build: cmake -S firmware/test/sim -B firmware/cmake-build-sim (host gcc, libsimavr).
 */
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <avr_ioport.h>
#include <avr_timer.h>
#include <avr_uart.h>
#include <parts/uart_pty.h>
#include <sim_avr.h>
#include <sim_elf.h>

#define PIN_LOG_MAX 8192
#define INPUT_MAX 256
#define BRIDGE_RING 4096
#define BRIDGE_PORT 4321
#define MCU_FREQUENCY 12000000UL
/* ATmega8 data-space address of UCSRA, used only by the `reg` command and the bridge bootstrap */
#define UCSRA_ADDRESS 0x2b

static avr_t *avr;
static uart_pty_t uartPty;
static const char *transport = "pty";

/* built-in UDP bridge */
static int bridgeSocket = -1;
static struct sockaddr_in bridgePeer;
static bool bridgeHavePeer;
static uint8_t ring[BRIDGE_RING];
static size_t ringRead, ringWrite;
static bool uartHasRoom;

static struct {
	avr_cycle_count_t cycle;
	uint8_t bit;
	uint8_t value;
} pinLog[PIN_LOG_MAX];
static int pinLogLength;
static char watchedPort = 'B';

static char input[INPUT_MAX];
static size_t inputLength;

static bool waiting;
static avr_cycle_count_t waitUntil;
static bool pulsing;
static avr_cycle_count_t pulseUntil;
static char pulsePort;
static int pulseBit;

static void reply(const char *format, ...) {
	va_list args;
	putchar('@');	/* marks the control channel: simavr's own printf goes to the same stdout */
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	putchar('\n');
	fflush(stdout);
}

static avr_irq_t *portIrq(char port, int bit) {
	return avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(port), bit);
}

static avr_irq_t *uartIrq(int index) {
	return avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), index);
}

static void pinHook(struct avr_irq_t *irq, uint32_t value, void *param) {
	(void)irq;
	if (pinLogLength < PIN_LOG_MAX) {
		pinLog[pinLogLength].cycle = avr->cycle;
		pinLog[pinLogLength].bit = (uint8_t)(uintptr_t)param;
		pinLog[pinLogLength].value = value ? 1 : 0;
		pinLogLength++;
	}
}

static void pwmHook(struct avr_irq_t *irq, uint32_t value, void *param) {
	(void)irq;
	if (pinLogLength < PIN_LOG_MAX) {
		pinLog[pinLogLength].cycle = avr->cycle;
		pinLog[pinLogLength].bit = (uint8_t)(8 + (uintptr_t)param);
		pinLog[pinLogLength].value = value ? 1 : 0;
		pinLogLength++;
	}
}

static void watchPort(char port) {
	watchedPort = port;
	pinLogLength = 0;
	for (int bit = 0; bit < 8; bit++) {
		avr_irq_register_notify(portIrq(port, bit), pinHook, (void *)(uintptr_t)bit);
	}
}

/* ---- UART bridge (host <-> firmware) -------------------------------------------------------------- */

static size_t ringCount(void) {
	return (ringWrite - ringRead + BRIDGE_RING) % BRIDGE_RING;
}

static void ringPush(uint8_t byte) {
	if (ringCount() + 1 >= BRIDGE_RING) {
		return;	/* the host outran the firmware: drop rather than overwrite */
	}
	ring[ringWrite] = byte;
	ringWrite = (ringWrite + 1) % BRIDGE_RING;
}

/* Hand bytes to the AVR while its USART input buffer has room (the UART raises XON/XOFF). */
static void flushToAvr(void) {
	while (uartHasRoom && ringCount() > 0) {
		uint8_t byte = ring[ringRead];
		ringRead = (ringRead + 1) % BRIDGE_RING;
		avr_raise_irq(uartIrq(UART_IRQ_INPUT), byte);
	}
}

static void uartOutputHook(struct avr_irq_t *irq, uint32_t value, void *param) {
	(void)irq;
	(void)param;
	if (bridgeSocket >= 0 && bridgeHavePeer) {
		uint8_t byte = (uint8_t)value;
		sendto(bridgeSocket, &byte, 1, 0, (struct sockaddr *)&bridgePeer, sizeof(bridgePeer));
	}
}

static void uartXonHook(struct avr_irq_t *irq, uint32_t value, void *param) {
	(void)irq;
	(void)value;
	(void)param;
	uartHasRoom = true;
	flushToAvr();
}

static void uartXoffHook(struct avr_irq_t *irq, uint32_t value, void *param) {
	(void)irq;
	(void)value;
	(void)param;
	uartHasRoom = false;
}

static void bridgeConnect(void) {
	/* without this simavr prints a coloured copy of every transmitted byte to stdout */
	uint32_t flags = 0;
	avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &flags);
	flags &= ~AVR_UART_FLAG_STDIO;
	avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &flags);

	avr_irq_register_notify(uartIrq(UART_IRQ_OUTPUT), uartOutputHook, NULL);
	avr_irq_register_notify(uartIrq(UART_IRQ_OUT_XON), uartXonHook, NULL);
	avr_irq_register_notify(uartIrq(UART_IRQ_OUT_XOFF), uartXoffHook, NULL);
	/* the firmware enables RX after this, which raises XON and starts the flow */
	uartHasRoom = (avr->data[UCSRA_ADDRESS] & 0x80) == 0;
}

static void bridgeInitUdp(void) {
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(BRIDGE_PORT);

	bridgeSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (bridgeSocket < 0 ||
	    bind(bridgeSocket, (struct sockaddr *)&address, sizeof(address)) != 0) {
		fprintf(stderr, "cannot bind UDP %d: %s\n", BRIDGE_PORT, strerror(errno));
		exit(2);
	}
	reply("uart udp %d", BRIDGE_PORT);
}

static void bridgeInitPty(void) {
	uart_pty_init(avr, &uartPty);
	uart_pty_connect(&uartPty, '0');
	if (uartPty.pty.slavename[0] == '\0') {
		reply("uart pty failed");
		return;
	}
	reply("uart pty %s", uartPty.pty.slavename);
	printf("/tmp/simavr-uart0 -> %s\n", uartPty.pty.slavename);
}

static void bridgePoll(void) {
	if (bridgeSocket < 0) {
		return;
	}
	for (;;) {
		uint8_t buffer[512];
		struct sockaddr_in from;
		socklen_t fromLength = sizeof(from);
		ssize_t count = recvfrom(bridgeSocket, buffer, sizeof(buffer), MSG_DONTWAIT,
			(struct sockaddr *)&from, &fromLength);
		if (count <= 0) {
			return;
		}
		bridgePeer = from;
		bridgeHavePeer = true;
		for (ssize_t i = 0; i < count; i++) {
			ringPush(buffer[i]);
		}
		flushToAvr();
	}
}

/* ---- control channel ------------------------------------------------------------------------------ */

static int parseUnsigned(const char *text, unsigned long *out) {
	char *end = NULL;
	unsigned long value = strtoul(text, &end, 0);
	if (end == text) {
		return 0;
	}
	*out = value;
	return 1;
}

static void handleLine(char *line) {
	char command[16];
	if (sscanf(line, "%15s", command) != 1) {
		return;
	}
	unsigned long ms, value;
	char port;
	int bit;

	if (strcmp(command, "wait") == 0) {
		if (!parseUnsigned(line + strlen("wait"), &ms)) {
			reply("err wait");
			return;
		}
		waitUntil = avr->cycle + (avr_cycle_count_t)ms * (MCU_FREQUENCY / 1000);
		waiting = true;
	} else if (strcmp(command, "set") == 0) {
		if (sscanf(line, "%*s %c %d %lu", &port, &bit, &value) != 3) {
			reply("err set");
			return;
		}
		avr_raise_irq(portIrq(port, bit), value ? 1 : 0);
		reply("ok");
	} else if (strcmp(command, "pulse") == 0) {
		if (sscanf(line, "%*s %c %d %lu", &port, &bit, &ms) != 3) {
			reply("err pulse");
			return;
		}
		avr_raise_irq(portIrq(port, bit), 0);
		pulsePort = port;
		pulseBit = bit;
		pulseUntil = avr->cycle + (avr_cycle_count_t)ms * (MCU_FREQUENCY / 1000);
		pulsing = true;
	} else if (strcmp(command, "state") == 0) {
		avr_ioport_state_t state;
		if (sscanf(line, "%*s %c", &port) != 1) {
			reply("err state");
			return;
		}
		if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETSTATE(port), &state) != 0) {
			reply("err state %c", port);
			return;
		}
		reply("state %c %02x %02x %02x", port, state.port, state.ddr, state.pin);
	} else if (strcmp(command, "reg") == 0) {
		if (!parseUnsigned(line + strlen("reg"), &value) || value > 0xffff) {
			reply("err reg");
			return;
		}
		reply("reg %04lx %02x", value, avr->data[value]);
	} else if (strcmp(command, "uart") == 0) {
		/* inject one byte into the USART receive IRQ, bypassing the bridge */
		if (!parseUnsigned(line + strlen("uart"), &value)) {
			reply("err uart");
			return;
		}
		avr_raise_irq(uartIrq(UART_IRQ_INPUT), (uint32_t)value);
		reply("ok");
	} else if (strcmp(command, "watch") == 0) {
		if (sscanf(line, "%*s %c", &port) != 1) {
			reply("err watch");
			return;
		}
		watchPort(port);
		reply("ok");
	} else if (strcmp(command, "pwm") == 0) {
		char timer;
		if (sscanf(line, "%*s %c %lu", &timer, &value) != 2) {
			reply("err pwm");
			return;
		}
		avr_irq_t *irq = avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ(timer),
			TIMER_IRQ_OUT_COMP + (int)value);
		if (!irq) {
			reply("err pwm %c %lu", timer, value);
			return;
		}
		avr_irq_register_notify(irq, pwmHook, (void *)(uintptr_t)value);
		reply("ok");
	} else if (strcmp(command, "log") == 0) {
		reply("log %d", pinLogLength);
		for (int i = 0; i < pinLogLength; i++) {
			reply("l %llu:%u:%u", (unsigned long long)pinLog[i].cycle, pinLog[i].bit,
				pinLog[i].value);
		}
	} else if (strcmp(command, "logclear") == 0) {
		pinLogLength = 0;
		reply("ok");
	} else if (strcmp(command, "cycles") == 0) {
		reply("cycle %llu", (unsigned long long)avr->cycle);
	} else if (strcmp(command, "quit") == 0) {
		exit(0);
	} else {
		reply("err unknown %s", command);
	}
}

static void pollInput(void) {
	char buffer[256];
	ssize_t count;
	while ((count = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		for (ssize_t i = 0; i < count; i++) {
			if (buffer[i] == '\n') {
				input[inputLength] = '\0';
				handleLine(input);
				inputLength = 0;
			} else if (inputLength + 1 < sizeof(input)) {
				input[inputLength++] = buffer[i];
			}
		}
	}
	if (count == 0) {	/* stdin closed: the driver is gone, do not outlive it */
		exit(0);
	}
}

int main(int argc, char **argv) {
	const char *firmwarePath = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--udp") == 0) {
			transport = "udp";
		} else {
			firmwarePath = argv[i];
		}
	}
	if (!firmwarePath) {
		fprintf(stderr, "usage: %s [--udp] <firmware.elf>\n", argv[0]);
		return 2;
	}

	elf_firmware_t firmware;
	memset(&firmware, 0, sizeof(firmware));
	if (elf_read_firmware(firmwarePath, &firmware) != 0) {
		fprintf(stderr, "%s: cannot read firmware\n", firmwarePath);
		return 2;
	}

	avr = avr_make_mcu_by_name("atmega8");
	if (!avr) {
		fprintf(stderr, "simavr: no atmega8 core\n");
		return 2;
	}
	avr_init(avr);
	avr_load_firmware(avr, &firmware);
	avr->frequency = MCU_FREQUENCY;

	watchPort(watchedPort);

	if (strcmp(transport, "udp") == 0) {
		bridgeInitUdp();
	} else {
		bridgeInitPty();
	}
	bridgeConnect();

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
	reply("harness ready transport=%s freq=%lu", transport, (unsigned long)avr->frequency);

	unsigned long checks = 0;
	for (;;) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed) {
			reply("cpu state=%d", state);
			break;
		}
		if (++checks % 2048 == 0) {
			pollInput();
			bridgePoll();
		}
		if (waiting && avr->cycle >= waitUntil) {
			waiting = false;
			reply("ok %llu", (unsigned long long)avr->cycle);
		}
		if (pulsing && avr->cycle >= pulseUntil) {
			pulsing = false;
			avr_raise_irq(portIrq(pulsePort, pulseBit), 1);
			reply("ok");
		}
	}
	return 0;
}
