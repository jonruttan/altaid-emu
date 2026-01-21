#include "serial.h"
#include <string.h>

static inline uint32_t q_next(uint32_t x) { return (uint32_t)((x + 1u) & 4095u); }

void serial_init(SerialDev* s, uint32_t cpu_hz, uint32_t baud)
{
	memset(s, 0, sizeof(*s));
	s->cpu_hz = cpu_hz ? cpu_hz : 2000000u;
	s->baud   = baud   ? baud   : 9600u;

	// integer approximation; for 2MHz/9600 this becomes 208.
	uint32_t tpb = (s->cpu_hz + (s->baud/2u)) / s->baud;
	if (tpb == 0) tpb = 1;
	s->ticks_per_bit = tpb;

	s->last_tx = 1;
	s->tx_active = false;

	s->rx_active = false;
	s->rx_irq_latched = false;
}

void serial_host_enqueue(SerialDev* s, uint8_t ch)
{
	uint32_t n = q_next(s->rx_qt);
	if (n == s->rx_qh) return; // drop
	s->rx_q[s->rx_qt] = ch;
	s->rx_qt = n;
}

static int rx_q_pop(SerialDev* s)
{
	if (s->rx_qh == s->rx_qt) return -1;
	uint8_t v = s->rx_q[s->rx_qh];
	s->rx_qh = q_next(s->rx_qh);
	return (int)v;
}

static void rx_start_frame_if_needed(SerialDev* s)
{
	if (s->rx_active) return;
	int ch = rx_q_pop(s);
	if (ch < 0) return;

	s->rx_active = true;
	s->rx_frame_start = s->tick;
	s->rx_byte = (uint8_t)ch;

	// Edge-trigger the RX interrupt latch at start bit
	s->rx_irq_latched = true;
}

uint8_t serial_current_rx_level(SerialDev* s)
{
	rx_start_frame_if_needed(s);

	if (!s->rx_active) return 1; // idle

	uint64_t dt = s->tick - s->rx_frame_start;
	uint64_t tpb = s->ticks_per_bit;

	// 1 start + 8 data + 1 stop = 10 bits
	uint64_t total = tpb * 10u;
	if (dt >= total) {
		// end frame
		s->rx_active = false;
		return 1;
	}

	uint64_t bit = dt / tpb; // 0..9
	if (bit == 0) return 0;   // start
	if (bit >= 1 && bit <= 8) {
		uint8_t data_bit = (s->rx_byte >> (uint8_t)(bit-1)) & 1u;
		return data_bit;
	}
	return 1; // stop
}

int serial_tick_tx(SerialDev* s, uint8_t tx_level, void (*putch)(int ch, void *u), void *u)
{
	int emitted = 0;
	// Detect start edge: idle high -> low
	if (!s->tx_active) {
		if (s->last_tx == 1 && tx_level == 0) {
			s->tx_active = true;
			s->tx_bit_index = 0;
			s->tx_byte = 0;
			// sample in the middle of bit 0 (1.5 bit times from start)
			s->tx_next_sample = s->tick + s->ticks_per_bit + (s->ticks_per_bit/2u);
		}
		s->last_tx = tx_level;
		return 0;
	}

	// If active, sample as many times as needed (instruction may span many ticks)
	while (s->tx_active && s->tick >= s->tx_next_sample) {
		uint8_t level = tx_level;

		if (s->tx_bit_index < 8) {
			s->tx_byte |= (uint8_t)(level & 1u) << s->tx_bit_index;
			s->tx_bit_index++;
			s->tx_next_sample += s->ticks_per_bit;
		} else {
			// stop bit (expect 1)
			if (level == 1) {
				if (putch) putch((int)s->tx_byte, u);
				emitted++;
			}
			s->tx_active = false;
			// We don't immediately look for another start within the same tick;
			// next call will detect.
		}
	}

	s->last_tx = tx_level;
	return emitted;
}
