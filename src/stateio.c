/* SPDX-License-Identifier: MIT */

#include "stateio.h"

#include "cassette.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * File formats:
 *
 *   STATE: magic "ALTAIDST" + u32 version + u32 rom_hash + u32 cpu_hz + u32 baud
 *   RAM:   magic "ALTAIDRM" + u32 version + u32 rom_hash + u32 cpu_hz + u32 baud
 *
 * All multi-byte fields are little-endian.
 */

enum { STATEIO_VER = 1 };

static const unsigned char k_state_magic[8] =
	{ 'A', 'L', 'T', 'A', 'I', 'D', 'S', 'T' };
static const unsigned char k_ram_magic[8] =
	{ 'A', 'L', 'T', 'A', 'I', 'D', 'R', 'M' };

static void err_set(char *err, unsigned cap, const char *msg)
{
	if (!err || cap == 0)
		return;
	if (!msg)
		msg = "error";
	snprintf(err, cap, "%s", msg);
}

static void err_set_errno(char *err, unsigned cap, const char *prefix)
{
	char buf[256];

	if (!prefix)
		prefix = "error";
	snprintf(buf, sizeof(buf), "%s: %s", prefix, strerror(errno));
	err_set(err, cap, buf);
}

static bool write_u8(FILE *f, uint8_t v)
{
	return (fwrite(&v, 1, 1, f) == 1);
}

static bool write_u32le(FILE *f, uint32_t v)
{
	unsigned char b[4];

	b[0] = (unsigned char)(v & 0xffu);
	b[1] = (unsigned char)((v >> 8) & 0xffu);
	b[2] = (unsigned char)((v >> 16) & 0xffu);
	b[3] = (unsigned char)((v >> 24) & 0xffu);
	return (fwrite(b, 1, sizeof(b), f) == sizeof(b));
}

static bool write_u64le(FILE *f, uint64_t v)
{
	unsigned char b[8];

	b[0] = (unsigned char)(v & 0xffu);
	b[1] = (unsigned char)((v >> 8) & 0xffu);
	b[2] = (unsigned char)((v >> 16) & 0xffu);
	b[3] = (unsigned char)((v >> 24) & 0xffu);
	b[4] = (unsigned char)((v >> 32) & 0xffu);
	b[5] = (unsigned char)((v >> 40) & 0xffu);
	b[6] = (unsigned char)((v >> 48) & 0xffu);
	b[7] = (unsigned char)((v >> 56) & 0xffu);
	return (fwrite(b, 1, sizeof(b), f) == sizeof(b));
}

static bool write_bool(FILE *f, bool v)
{
	return write_u8(f, v ? 1u : 0u);
}

static bool read_exact(FILE *f, void *dst, size_t n)
{
	return (fread(dst, 1, n, f) == n);
}

static bool read_u8(FILE *f, uint8_t *out)
{
	unsigned char v;

	if (!out)
		return false;
	if (!read_exact(f, &v, 1))
		return false;
	*out = (uint8_t)v;
	return true;
}

static bool read_u32le(FILE *f, uint32_t *out)
{
	unsigned char b[4];

	if (!out)
		return false;
	if (!read_exact(f, b, sizeof(b)))
		return false;
	*out = ((uint32_t)b[0]) |
		((uint32_t)b[1] << 8) |
		((uint32_t)b[2] << 16) |
		((uint32_t)b[3] << 24);
	return true;
}

static bool read_u64le(FILE *f, uint64_t *out)
{
	unsigned char b[8];

	if (!out)
		return false;
	if (!read_exact(f, b, sizeof(b)))
		return false;
	*out = ((uint64_t)b[0]) |
		((uint64_t)b[1] << 8) |
		((uint64_t)b[2] << 16) |
		((uint64_t)b[3] << 24) |
		((uint64_t)b[4] << 32) |
		((uint64_t)b[5] << 40) |
		((uint64_t)b[6] << 48) |
		((uint64_t)b[7] << 56);
	return true;
}

static bool read_bool(FILE *f, bool *out)
{
	uint8_t v;

	if (!out)
		return false;
	if (!read_u8(f, &v))
		return false;
	*out = (v != 0);
	return true;
}

static uint32_t fnv1a32(const void *buf, size_t len, uint32_t seed)
{
	const unsigned char *p = (const unsigned char *)buf;
	uint32_t h = seed;

	for (size_t i = 0; i < len; i++) {
		h ^= (uint32_t)p[i];
		h *= 16777619u;
	}
	return h;
}

uint32_t stateio_rom_hash32(const struct EmuCore *core)
{
	uint32_t h;

	if (!core)
		return 0;

	h = 2166136261u;
	h = fnv1a32(core->hw.rom[0], sizeof(core->hw.rom[0]), h);
	h = fnv1a32(core->hw.rom[1], sizeof(core->hw.rom[1]), h);
	return h;
}

static bool write_header(FILE *f, const unsigned char magic[8],
			 uint32_t ver, uint32_t rom_hash,
			 uint32_t cpu_hz, uint32_t baud)
{
	if (fwrite(magic, 1, 8, f) != 8)
		return false;
	if (!write_u32le(f, ver))
		return false;
	if (!write_u32le(f, rom_hash))
		return false;
	if (!write_u32le(f, cpu_hz))
		return false;
	if (!write_u32le(f, baud))
		return false;
	return true;
}

static bool read_header(FILE *f, const unsigned char magic[8],
			uint32_t *out_ver, uint32_t *out_rom_hash,
			uint32_t *out_cpu_hz, uint32_t *out_baud)
{
	unsigned char m[8];

	if (!read_exact(f, m, 8))
		return false;
	if (memcmp(m, magic, 8) != 0)
		return false;
	if (!read_u32le(f, out_ver))
		return false;
	if (!read_u32le(f, out_rom_hash))
		return false;
	if (!read_u32le(f, out_cpu_hz))
		return false;
	if (!read_u32le(f, out_baud))
		return false;
	return true;
}

static bool write_i8080(FILE *f, const I8080 *cpu)
{
	uint8_t flags;

	if (!cpu)
		return false;

	flags = 0;
	if (cpu->z)
		flags |= 1u << 0;
	if (cpu->s)
		flags |= 1u << 1;
	if (cpu->p)
		flags |= 1u << 2;
	if (cpu->cy)
		flags |= 1u << 3;
	if (cpu->ac)
		flags |= 1u << 4;
	if (cpu->inte)
		flags |= 1u << 5;
	if (cpu->ei_pending)
		flags |= 1u << 6;
	if (cpu->halted)
		flags |= 1u << 7;

	return write_u8(f, cpu->a) &&
		write_u8(f, cpu->b) &&
		write_u8(f, cpu->c) &&
		write_u8(f, cpu->d) &&
		write_u8(f, cpu->e) &&
		write_u8(f, cpu->h) &&
		write_u8(f, cpu->l) &&
		write_u32le(f, cpu->pc) &&
		write_u32le(f, cpu->sp) &&
		write_u8(f, flags);
}

static bool read_i8080(FILE *f, I8080 *cpu)
{
	uint8_t flags;
	uint32_t pc;
	uint32_t sp;

	if (!cpu)
		return false;

	if (!read_u8(f, &cpu->a) ||
	    !read_u8(f, &cpu->b) ||
	    !read_u8(f, &cpu->c) ||
	    !read_u8(f, &cpu->d) ||
	    !read_u8(f, &cpu->e) ||
	    !read_u8(f, &cpu->h) ||
	    !read_u8(f, &cpu->l))
		return false;
	if (!read_u32le(f, &pc) || !read_u32le(f, &sp))
		return false;
	if (!read_u8(f, &flags))
		return false;

	cpu->pc = (uint16_t)pc;
	cpu->sp = (uint16_t)sp;
	cpu->z = ((flags >> 0) & 1u) != 0;
	cpu->s = ((flags >> 1) & 1u) != 0;
	cpu->p = ((flags >> 2) & 1u) != 0;
	cpu->cy = ((flags >> 3) & 1u) != 0;
	cpu->ac = ((flags >> 4) & 1u) != 0;
	cpu->inte = ((flags >> 5) & 1u) != 0;
	cpu->ei_pending = ((flags >> 6) & 1u) != 0;
	cpu->halted = ((flags >> 7) & 1u) != 0;

	return true;
}

static bool write_serial(FILE *f, const SerialDev *s)
{
	if (!s)
		return false;

	if (!write_u32le(f, s->cpu_hz) ||
	    !write_u32le(f, s->baud) ||
	    !write_u32le(f, s->ticks_per_bit) ||
	    !write_u64le(f, s->tick) ||
	    !write_u8(f, s->last_tx) ||
	    !write_bool(f, s->tx_active) ||
	    !write_u64le(f, s->tx_next_sample) ||
	    !write_u8(f, s->tx_bit_index) ||
	    !write_u8(f, s->tx_byte) ||
	    !write_u32le(f, s->rx_qh) ||
	    !write_u32le(f, s->rx_qt) ||
	    !write_bool(f, s->rx_active) ||
	    !write_u64le(f, s->rx_frame_start) ||
	    !write_u8(f, s->rx_byte) ||
	    !write_bool(f, s->rx_irq_latched))
		return false;

	if (fwrite(s->rx_q, 1, sizeof(s->rx_q), f) != sizeof(s->rx_q))
		return false;
	return true;
}

static bool read_serial(FILE *f, SerialDev *s)
{
	uint32_t cpu_hz;
	uint32_t baud;
	uint32_t ticks_per_bit;
	uint32_t rx_qh;
	uint32_t rx_qt;

	if (!s)
		return false;

	if (!read_u32le(f, &cpu_hz) ||
	    !read_u32le(f, &baud) ||
	    !read_u32le(f, &ticks_per_bit) ||
	    !read_u64le(f, &s->tick) ||
	    !read_u8(f, &s->last_tx) ||
	    !read_bool(f, &s->tx_active) ||
	    !read_u64le(f, &s->tx_next_sample) ||
	    !read_u8(f, &s->tx_bit_index) ||
	    !read_u8(f, &s->tx_byte) ||
	    !read_u32le(f, &rx_qh) ||
	    !read_u32le(f, &rx_qt) ||
	    !read_bool(f, &s->rx_active) ||
	    !read_u64le(f, &s->rx_frame_start) ||
	    !read_u8(f, &s->rx_byte) ||
	    !read_bool(f, &s->rx_irq_latched))
		return false;

	if (!read_exact(f, s->rx_q, sizeof(s->rx_q)))
		return false;

	s->cpu_hz = cpu_hz;
	s->baud = baud;
	s->ticks_per_bit = ticks_per_bit;
	s->rx_qh = rx_qh;
	s->rx_qt = rx_qt;
	return true;
}

static bool write_hw(FILE *f, const AltaidHW *hw)
{
	if (!hw)
		return false;

	/* RAM contents. */
	if (fwrite(hw->ram, 1, sizeof(hw->ram), f) != sizeof(hw->ram))
		return false;

	if (!write_u8(f, hw->ram_a16) ||
	    !write_u8(f, hw->ram_a17) ||
	    !write_u8(f, hw->ram_a18) ||
	    !write_u8(f, hw->ram_bank) ||
	    !write_u8(f, hw->rom_half) ||
	    !write_bool(f, hw->rom_low_mapped) ||
	    !write_bool(f, hw->rom_hi_mapped) ||
	    !write_u8(f, hw->out_c0) ||
	    !write_bool(f, hw->tx_line) ||
	    !write_bool(f, hw->rx_level) ||
	    !write_bool(f, hw->timer_en) ||
	    !write_bool(f, hw->timer_level) ||
	    !write_bool(f, hw->cassette_out_level) ||
	    !write_bool(f, hw->cassette_out_dirty) ||
	    !write_bool(f, hw->cassette_in_level) ||
	    !write_u8(f, hw->scan_row) ||
	    !write_u8(f, hw->led_row_mask) ||
	    !write_bool(f, hw->panel_latched_valid) ||
	    !write_u32le(f, hw->panel_latched_seq) ||
	    !write_u32le(f, hw->panel_latched_addr) ||
	    !write_u8(f, hw->panel_latched_data) ||
	    !write_u8(f, hw->panel_latched_stat))
		return false;

	if (fwrite(hw->led_row_nibble, 1, sizeof(hw->led_row_nibble), f) !=
	    sizeof(hw->led_row_nibble))
		return false;

	for (size_t i = 0; i < 11; i++) {
		if (!write_bool(f, hw->fp_key_down[i]))
			return false;
		if (!write_u64le(f, hw->fp_key_until[i]))
			return false;
	}

	return true;
}

static bool read_hw(FILE *f, AltaidHW *hw)
{
	uint32_t seq;
	uint32_t addr;

	if (!hw)
		return false;

	if (!read_exact(f, hw->ram, sizeof(hw->ram)))
		return false;

	if (!read_u8(f, &hw->ram_a16) ||
	    !read_u8(f, &hw->ram_a17) ||
	    !read_u8(f, &hw->ram_a18) ||
	    !read_u8(f, &hw->ram_bank) ||
	    !read_u8(f, &hw->rom_half) ||
	    !read_bool(f, &hw->rom_low_mapped) ||
	    !read_bool(f, &hw->rom_hi_mapped) ||
	    !read_u8(f, &hw->out_c0) ||
	    !read_bool(f, &hw->tx_line) ||
	    !read_bool(f, &hw->rx_level) ||
	    !read_bool(f, &hw->timer_en) ||
	    !read_bool(f, &hw->timer_level) ||
	    !read_bool(f, &hw->cassette_out_level) ||
	    !read_bool(f, &hw->cassette_out_dirty) ||
	    !read_bool(f, &hw->cassette_in_level) ||
	    !read_u8(f, &hw->scan_row) ||
	    !read_u8(f, &hw->led_row_mask) ||
	    !read_bool(f, &hw->panel_latched_valid) ||
	    !read_u32le(f, &seq) ||
	    !read_u32le(f, &addr) ||
	    !read_u8(f, &hw->panel_latched_data) ||
	    !read_u8(f, &hw->panel_latched_stat))
		return false;

	hw->panel_latched_seq = seq;
	hw->panel_latched_addr = (uint16_t)addr;

	if (!read_exact(f, hw->led_row_nibble, sizeof(hw->led_row_nibble)))
		return false;

	for (size_t i = 0; i < 11; i++) {
		if (!read_bool(f, &hw->fp_key_down[i]))
			return false;
		if (!read_u64le(f, &hw->fp_key_until[i]))
			return false;
	}

	return true;
}

static bool write_cassette(FILE *f, const Cassette *c)
{
	if (!c)
		return false;

	if (!write_bool(f, c->attached))
		return false;
	if (fwrite(c->path, 1, sizeof(c->path), f) != sizeof(c->path))
		return false;
	if (!write_u32le(f, c->cpu_hz) ||
	    !write_bool(f, c->idle_level) ||
	    !write_bool(f, c->in_level) ||
	    !write_bool(f, c->playing) ||
	    !write_bool(f, c->play_level) ||
	    !write_u64le(f, c->play_index) ||
	    !write_u64le(f, c->play_next_edge_tick) ||
	    !write_bool(f, c->recording) ||
	    !write_u64le(f, c->rec_last_edge_tick) ||
	    !write_bool(f, c->rec_last_level) ||
	    !write_u64le(f, c->dur_count))
		return false;

	for (size_t i = 0; i < c->dur_count; i++) {
		if (!write_u32le(f, c->durations[i]))
			return false;
	}

	return true;
}

static bool read_cassette(FILE *f, Cassette *c)
{
	uint64_t play_index;
	uint64_t dur_count;

	if (!c)
		return false;

	cassette_free(c);
	cassette_init(c, c->cpu_hz);

	if (!read_bool(f, &c->attached))
		return false;
	if (!read_exact(f, c->path, sizeof(c->path)))
		return false;
	c->path[sizeof(c->path) - 1] = '\0';

	if (!read_u32le(f, &c->cpu_hz) ||
	    !read_bool(f, &c->idle_level) ||
	    !read_bool(f, &c->in_level) ||
	    !read_bool(f, &c->playing) ||
	    !read_bool(f, &c->play_level) ||
	    !read_u64le(f, &play_index) ||
	    !read_u64le(f, &c->play_next_edge_tick) ||
	    !read_bool(f, &c->recording) ||
	    !read_u64le(f, &c->rec_last_edge_tick) ||
	    !read_bool(f, &c->rec_last_level) ||
	    !read_u64le(f, &dur_count))
		return false;

	c->play_index = (size_t)play_index;
	c->dur_count = (size_t)dur_count;
	c->dur_cap = c->dur_count;
	if (c->dur_count) {
		c->durations = (uint32_t *)calloc(c->dur_count, sizeof(uint32_t));
		if (!c->durations)
			return false;
		for (size_t i = 0; i < c->dur_count; i++) {
			if (!read_u32le(f, &c->durations[i]))
				return false;
		}
	}

	return true;
}

bool stateio_save_ram(const struct EmuCore *core, const char *path,
			char *err, unsigned err_cap)
{
	FILE *f;
	uint32_t rom_hash;

	if (!core || !path || !*path) {
		err_set(err, err_cap, "invalid arguments");
		return false;
	}

	rom_hash = stateio_rom_hash32(core);

	f = fopen(path, "wb");
	if (!f) {
		err_set_errno(err, err_cap, "open ram for write");
		return false;
	}

	if (!write_header(f, k_ram_magic, STATEIO_VER, rom_hash,
			  core->cfg.cpu_hz, core->cfg.baud)) {
		err_set_errno(err, err_cap, "write ram header");
		fclose(f);
		return false;
	}

	if (fwrite(core->hw.ram, 1, sizeof(core->hw.ram), f) != sizeof(core->hw.ram)) {
		err_set_errno(err, err_cap, "write ram");
		fclose(f);
		return false;
	}

	if (fclose(f) != 0) {
		err_set_errno(err, err_cap, "close ram");
		return false;
	}

	return true;
}

bool stateio_load_ram(struct EmuCore *core, const char *path,
			char *err, unsigned err_cap)
{
	FILE *f;
	uint32_t ver;
	uint32_t rom_hash;
	uint32_t cpu_hz;
	uint32_t baud;
	uint32_t want_hash;

	if (!core || !path || !*path) {
		err_set(err, err_cap, "invalid arguments");
		return false;
	}

	f = fopen(path, "rb");
	if (!f) {
		err_set_errno(err, err_cap, "open ram for read");
		return false;
	}

	if (!read_header(f, k_ram_magic, &ver, &rom_hash, &cpu_hz, &baud)) {
		err_set(err, err_cap, "bad ram file (magic/header)");
		fclose(f);
		return false;
	}
	if (ver != STATEIO_VER) {
		err_set(err, err_cap, "unsupported ram file version");
		fclose(f);
		return false;
	}

	want_hash = stateio_rom_hash32(core);
	if (rom_hash != want_hash) {
		err_set(err, err_cap, "ram file ROM hash mismatch");
		fclose(f);
		return false;
	}
	if (cpu_hz != core->cfg.cpu_hz || baud != core->cfg.baud) {
		err_set(err, err_cap, "ram file CPU/baud mismatch");
		fclose(f);
		return false;
	}

	if (!read_exact(f, core->hw.ram, sizeof(core->hw.ram))) {
		err_set_errno(err, err_cap, "read ram");
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

bool stateio_save_state(const struct EmuCore *core, const char *path,
			char *err, unsigned err_cap)
{
	FILE *f;
	uint32_t rom_hash;

	if (!core || !path || !*path) {
		err_set(err, err_cap, "invalid arguments");
		return false;
	}

	rom_hash = stateio_rom_hash32(core);

	f = fopen(path, "wb");
	if (!f) {
		err_set_errno(err, err_cap, "open state for write");
		return false;
	}

	if (!write_header(f, k_state_magic, STATEIO_VER, rom_hash,
			  core->cfg.cpu_hz, core->cfg.baud)) {
		err_set_errno(err, err_cap, "write state header");
		fclose(f);
		return false;
	}

	if (!write_u64le(f, core->timer_period) ||
	    !write_u64le(f, core->next_timer_tick) ||
	    !write_u32le(f, core->tx_r) ||
	    !write_u32le(f, core->tx_w)) {
		err_set_errno(err, err_cap, "write state core fields");
		fclose(f);
		return false;
	}
	if (fwrite(core->tx_buf, 1, sizeof(core->tx_buf), f) != sizeof(core->tx_buf)) {
		err_set_errno(err, err_cap, "write state txbuf");
		fclose(f);
		return false;
	}

	if (!write_i8080(f, &core->cpu) ||
	    !write_serial(f, &core->ser) ||
	    !write_hw(f, &core->hw) ||
	    !write_bool(f, core->cas_attached) ||
	    !write_cassette(f, &core->cas)) {
		err_set_errno(err, err_cap, "write state body");
		fclose(f);
		return false;
	}

	if (fclose(f) != 0) {
		err_set_errno(err, err_cap, "close state");
		return false;
	}

	return true;
}

bool stateio_load_state(struct EmuCore *core, const char *path,
			char *err, unsigned err_cap)
{
	FILE *f;
	uint32_t ver;
	uint32_t rom_hash;
	uint32_t cpu_hz;
	uint32_t baud;
	uint32_t want_hash;
	uint32_t tx_r;
	uint32_t tx_w;
	bool cas_attached;

	if (!core || !path || !*path) {
		err_set(err, err_cap, "invalid arguments");
		return false;
	}

	f = fopen(path, "rb");
	if (!f) {
		err_set_errno(err, err_cap, "open state for read");
		return false;
	}

	if (!read_header(f, k_state_magic, &ver, &rom_hash, &cpu_hz, &baud)) {
		err_set(err, err_cap, "bad state file (magic/header)");
		fclose(f);
		return false;
	}
	if (ver != STATEIO_VER) {
		err_set(err, err_cap, "unsupported state file version");
		fclose(f);
		return false;
	}

	want_hash = stateio_rom_hash32(core);
	if (rom_hash != want_hash) {
		err_set(err, err_cap, "state file ROM hash mismatch");
		fclose(f);
		return false;
	}
	if (cpu_hz != core->cfg.cpu_hz || baud != core->cfg.baud) {
		err_set(err, err_cap, "state file CPU/baud mismatch");
		fclose(f);
		return false;
	}

	if (!read_u64le(f, &core->timer_period) ||
	    !read_u64le(f, &core->next_timer_tick) ||
	    !read_u32le(f, &tx_r) ||
	    !read_u32le(f, &tx_w)) {
		err_set(err, err_cap, "read state core fields");
		fclose(f);
		return false;
	}
	core->tx_r = tx_r % EMU_TXBUF_SIZE;
	core->tx_w = tx_w % EMU_TXBUF_SIZE;

	if (!read_exact(f, core->tx_buf, sizeof(core->tx_buf))) {
		err_set(err, err_cap, "read state txbuf");
		fclose(f);
		return false;
	}

	if (!read_i8080(f, &core->cpu) ||
	    !read_serial(f, &core->ser) ||
	    !read_hw(f, &core->hw) ||
	    !read_bool(f, &cas_attached) ||
	    !read_cassette(f, &core->cas)) {
		err_set(err, err_cap, "read state body");
		fclose(f);
		return false;
	}

	core->cas_attached = cas_attached;

	fclose(f);
	return true;
}
