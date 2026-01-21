/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_SERIAL_H
#define ALTAID_EMU_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	/* configuration */
	uint32_t	cpu_hz;
	uint32_t	baud;
	uint32_t	ticks_per_bit;	/* integer approximation */

	/* current time in cpu ticks */
	uint64_t	tick;

	/* TX decode (emulated machine -> host) */
	uint8_t		last_tx;
	bool		tx_active;
	uint64_t	tx_next_sample;
	uint8_t		tx_bit_index;	/* 0..8 (8 data + stop) */
	uint8_t		tx_byte;

	/* RX inject (host -> emulated machine) */
	uint8_t		rx_q[4096];
	uint32_t	rx_qh;
	uint32_t	rx_qt;

	bool		rx_active;
	uint64_t	rx_frame_start;
	uint8_t		rx_byte;

	/* interrupt latch: goes true on RX start-bit edge */
	bool		rx_irq_latched;
} SerialDev;

void serial_init(SerialDev *s, uint32_t cpu_hz, uint32_t baud);

int serial_tick_tx(SerialDev *s, uint8_t tx_level,
void (*putch)(int ch, void *u), void *u);

void serial_host_enqueue(SerialDev *s, uint8_t ch);

uint8_t serial_current_rx_level(SerialDev *s);

static inline void serial_advance(SerialDev *s, uint32_t ticks)
{
	s->tick += ticks;
}

#endif /* ALTAID_EMU_SERIAL_H */
