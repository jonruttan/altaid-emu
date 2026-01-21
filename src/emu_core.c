/* SPDX-License-Identifier: MIT */

#include "emu_core.h"

#include "altaid_hw.h"
#include "cassette.h"
#include "i8080.h"
#include "serial.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void txbuf_clear(struct EmuCore *core)
{
	core->tx_r = 0;
	core->tx_w = 0;
}

static void txbuf_putch_cb(int ch, void *u)
{
	struct EmuCore *core = (struct EmuCore *)u;
	uint32_t next;

	if (!core) return;

	next = (core->tx_w + 1u) % EMU_TXBUF_SIZE;
	if (next == core->tx_r) {
		/* Drop on overflow (best-effort). */
		return;
	}

	core->tx_buf[core->tx_w] = (uint8_t)ch;
	core->tx_w = next;
}

size_t emu_core_tx_pop(struct EmuCore *core, uint8_t *dst, size_t cap)
{
	size_t n;

	if (!core || !dst || cap == 0) return 0;

	n = 0;
	while (core->tx_r != core->tx_w && n < cap) {
		dst[n++] = core->tx_buf[core->tx_r];
		core->tx_r = (core->tx_r + 1u) % EMU_TXBUF_SIZE;
	}
	return n;
}

void emu_core_init(struct EmuCore *core, uint32_t cpu_hz, uint32_t baud)
{
	if (!core) return;

	core->cfg.cpu_hz = cpu_hz;
	core->cfg.baud = baud;

	altaid_hw_init(&core->hw);

	i8080_reset(&core->cpu);
	core->cpu.pc = 0x0000;

	core->bus.user = &core->hw;
	core->bus.mem_read = altaid_mem_read;
	core->bus.mem_write = altaid_mem_write;
	core->bus.io_in = altaid_io_in;
	core->bus.io_out = altaid_io_out;

	serial_init(&core->ser, cpu_hz, baud);
	cassette_init(&core->cas, cpu_hz);
	core->cas_attached = false;

	core->timer_period = cpu_hz / 1000u;
	if (core->timer_period == 0)
	core->timer_period = 1;
	core->next_timer_tick = 0;

	txbuf_clear(core);
}

bool emu_core_load_rom64k(struct EmuCore *core, const char *rom_path)
{
	if (!core) return false;
	return altaid_hw_load_rom64k(&core->hw, rom_path);
}

void emu_core_reset(struct EmuCore *core)
{
	if (!core) return;

	i8080_reset(&core->cpu);
	core->cpu.pc = 0x0000;
	altaid_hw_reset_runtime(&core->hw);

	serial_init(&core->ser, core->cfg.cpu_hz, core->cfg.baud);
	txbuf_clear(core);

	core->next_timer_tick = 0;

	if (core->cas_attached)
	cassette_stop(&core->cas);
}

static void set_hw_lines(struct EmuCore *core)
{
	bool rx_level;
	bool cas_level;
	bool timer_pulse;
	bool timer_level;

	rx_level = (serial_current_rx_level(&core->ser) != 0);
	cas_level = false;
	if (core->cas_attached)
	cas_level = cassette_in_level_at(&core->cas, core->ser.tick);

	/*
	* Timer is modeled as a single-tick active-low pulse at a fixed period.
	* If the host runs in big batches, we may skip over multiple periods;
	* catch up so the next pulse occurs in the future.
	*/
	timer_pulse = false;
	if (core->timer_period != 0) {
		while (core->ser.tick >= core->next_timer_tick) {
			timer_pulse = true;
			core->next_timer_tick += core->timer_period;
		}
	}

	/* Timer is active-low pulse when enabled; otherwise idle high. */
	timer_level = true;
	if (core->hw.timer_en)
	timer_level = !timer_pulse;

	core->hw.rx_level = rx_level;
	core->hw.cassette_in_level = cas_level;
	core->hw.timer_level = timer_level;
}

void emu_core_run_batch(struct EmuCore *core, uint64_t batch_cycles)
{
	uint64_t batch_end;

	if (!core) return;

	batch_end = core->ser.tick + batch_cycles;
	while (core->ser.tick < batch_end) {
		int t;

		set_hw_lines(core);

		t = i8080_step(&core->cpu, &core->bus);
		serial_advance(&core->ser, (uint32_t)t);

		/* Service pending interrupt (RST7) on RX start-bit edge. */
		if (core->ser.rx_irq_latched && core->cpu.inte) {
			core->ser.rx_irq_latched = false;
			i8080_intr_service(&core->cpu, &core->bus, 7);
		}

		/* TX: decode into the core TX buffer. */
		serial_tick_tx(&core->ser, altaid_hw_tx_level(&core->hw),
		txbuf_putch_cb, core);

		/* Cassette record: capture edges driven by OUT 0x44. */
		if (core->cas_attached && core->hw.cassette_out_dirty) {
			core->hw.cassette_out_dirty = false;
			cassette_on_out_change(&core->cas, core->ser.tick,
			core->hw.cassette_out_level);
		}

		/* Front panel key auto-release. */
		altaid_hw_panel_tick(&core->hw, core->ser.tick);
	}
}
