/*
 * serial.spec.c
 *
 * Unit tests for serial helpers.
 */

#include "serial.c"

#include "test-runner.h"

typedef struct {
	int calls;
	int last;
} TxCapture;

static void capture_putch(int ch, void *u)
{
	TxCapture *cap = u;

	cap->calls++;
	cap->last = ch;
}

static char *test_serial_tx_decode_emits_byte(void)
{
	SerialDev s;
	TxCapture cap;
	uint8_t bits[8] = { 1u, 0u, 1u, 0u, 0u, 1u, 0u, 1u };
	int emitted;
	uint8_t i;

	cap.calls = 0;
	cap.last = -1;

	serial_init(&s, 2000000u, 9600u);

	s.tick = 0;
	(void)serial_tick_tx(&s, 1, capture_putch, &cap);

	s.tick = 1;
	(void)serial_tick_tx(&s, 0, capture_putch, &cap);

	for (i = 0; i < 8u; i++) {
		s.tick = s.tx_next_sample;
		(void)serial_tick_tx(&s, bits[i], capture_putch, &cap);
	}

	s.tick = s.tx_next_sample;
	emitted = serial_tick_tx(&s, 1, capture_putch, &cap);

	_it_should(
		"emit byte on stop bit",
		1 == emitted
		&& 1 == cap.calls
		&& 0xA5 == (uint8_t)cap.last
	);

	return NULL;
}

static char *test_serial_init_defaults(void)
{
	SerialDev s;

	serial_init(&s, 0, 0);

	_it_should(
		"default cpu hz and baud when zero",
		2000000u == s.cpu_hz
		&& 9600u == s.baud
	);

	_it_should(
		"compute ticks-per-bit",
		208u == s.ticks_per_bit
	);

	_it_should(
		"initialize RX/TX state",
		1 == s.last_tx
		&& false == s.tx_active
		&& false == s.rx_active
		&& false == s.rx_irq_latched
	);

	return NULL;
}

static char *test_serial_rx_frame_levels(void)
{
	SerialDev s;
	uint8_t level;

	serial_init(&s, 2000000u, 9600u);
	serial_host_enqueue(&s, 0xA5u); /* 0b10100101 */

	s.tick = 0;
	level = serial_current_rx_level(&s);
	_it_should(
		"start bit is low",
		0 == level
		&& true == s.rx_active
		&& true == s.rx_irq_latched
	);

	s.tick = s.ticks_per_bit * 1u;
	_it_should(
		"bit0 (LSB) is 1",
		1 == serial_current_rx_level(&s)
	);

	s.tick = s.ticks_per_bit * 2u;
	_it_should(
		"bit1 is 0",
		0 == serial_current_rx_level(&s)
	);

	s.tick = s.ticks_per_bit * 9u;
	_it_should(
		"stop bit is high",
		1 == serial_current_rx_level(&s)
	);

	s.tick = s.ticks_per_bit * 10u;
	_it_should(
		"frame ends after stop bit",
		1 == serial_current_rx_level(&s)
		&& false == s.rx_active
	);

	return NULL;
}

static char *test_serial_rx_idle_when_empty(void)
{
	SerialDev s;

	serial_init(&s, 2000000u, 9600u);

	_it_should(
		"idle line is high when no RX data",
		1 == serial_current_rx_level(&s)
		&& false == s.rx_active
		&& false == s.rx_irq_latched
	);

	return NULL;
}

static char *test_serial_rx_irq_latch_persists(void)
{
	SerialDev s;

	serial_init(&s, 2000000u, 9600u);
	serial_host_enqueue(&s, 0x55u);

	s.tick = 0;
	(void)serial_current_rx_level(&s);

	s.tick = s.ticks_per_bit * 5u;
	(void)serial_current_rx_level(&s);

	_it_should(
		"rx irq latch stays set until cleared by core",
		true == s.rx_irq_latched
	);

	return NULL;
}

static char *test_serial_tx_stop_bit_low_no_emit(void)
{
	SerialDev s;
	TxCapture cap;
	uint8_t bits[8] = { 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u };
	int emitted;
	uint8_t i;

	cap.calls = 0;
	cap.last = -1;

	serial_init(&s, 2000000u, 9600u);

	s.tick = 0;
	(void)serial_tick_tx(&s, 1, capture_putch, &cap);

	s.tick = 1;
	(void)serial_tick_tx(&s, 0, capture_putch, &cap);

	for (i = 0; i < 8u; i++) {
		s.tick = s.tx_next_sample;
		(void)serial_tick_tx(&s, bits[i], capture_putch, &cap);
	}

	s.tick = s.tx_next_sample;
	emitted = serial_tick_tx(&s, 0, capture_putch, &cap);

	_it_should(
		"do not emit when stop bit is low",
		0 == emitted
		&& 0 == cap.calls
	);

	return NULL;
}

static char *test_serial_rx_queue_drop_when_full(void)
{
	SerialDev s;
	uint32_t i;
	uint32_t qt_before;

	serial_init(&s, 2000000u, 9600u);

	for (i = 0; i < 4095u; i++) {
		serial_host_enqueue(&s, (uint8_t)i);
	}

	_it_should(
		"queue accepts 4095 bytes",
		4095u == s.rx_qt
		&& 0u == s.rx_qh
	);

	qt_before = s.rx_qt;
	serial_host_enqueue(&s, 0xEEu);

	_it_should(
		"queue drops when full",
		qt_before == s.rx_qt
	);

	return NULL;
}

static char *test_serial_tx_multi_sample_step(void)
{
	SerialDev s;
	TxCapture cap;
	int emitted;

	cap.calls = 0;
	cap.last = -1;

	serial_init(&s, 2000000u, 9600u);
	s.ticks_per_bit = 4u;

	s.tick = 0;
	(void)serial_tick_tx(&s, 1, capture_putch, &cap);

	s.tick = 1;
	(void)serial_tick_tx(&s, 0, capture_putch, &cap);

	s.tick = s.tx_next_sample + (s.ticks_per_bit * 9u);
	emitted = serial_tick_tx(&s, 1, capture_putch, &cap);

	_it_should(
		"tx samples multiple bits in one step",
		1 == emitted
		&& 1 == cap.calls
		&& 0xff == (uint8_t)cap.last
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_serial_init_defaults);
	_run_test(test_serial_rx_idle_when_empty);
	_run_test(test_serial_rx_irq_latch_persists);
	_run_test(test_serial_rx_frame_levels);
	_run_test(test_serial_tx_decode_emits_byte);
	_run_test(test_serial_tx_stop_bit_low_no_emit);
	_run_test(test_serial_rx_queue_drop_when_full);
	_run_test(test_serial_tx_multi_sample_step);

	return NULL;
}
