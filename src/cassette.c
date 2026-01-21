#include "cassette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CAS_MAGIC_LEN = 8 };

static const unsigned char k_magic[CAS_MAGIC_LEN] = {'A','L','T','A','P','0','0','1'};

typedef struct {
	unsigned char magic[CAS_MAGIC_LEN];
	uint32_t version;       /* 1 */
	uint32_t cpu_hz;        /* ticks per second for duration units */
	uint8_t  initial_level; /* 0/1 */
	uint8_t  reserved[3];
	uint32_t count;         /* number of durations */
} CassFileHdr;

static void vec_reserve(Cassette* c, size_t want)
{
	if (want <= c->dur_cap) return;
	size_t nc = c->dur_cap ? c->dur_cap : 1024;
	while (nc < want) nc *= 2;
	uint32_t *np = (uint32_t *)realloc(c->durations, nc * sizeof(uint32_t));
	if (!np) return;
	c->durations = np;
	c->dur_cap = nc;
}

static void vec_push(Cassette* c, uint32_t v)
{
	vec_reserve(c, c->dur_count + 1);
	if (c->dur_count < c->dur_cap) {
		c->durations[c->dur_count++] = v;
	}
}

void cassette_init(Cassette* c, uint32_t cpu_hz)
{
	memset(c, 0, sizeof(*c));
	c->cpu_hz = cpu_hz;
	c->idle_level = true; /* idle high is a sane default for the MIO comparator */
	c->in_level = c->idle_level;
}

void cassette_free(Cassette* c)
{
	cassette_stop(c);
	if (c->durations) {
		free(c->durations);
		c->durations = NULL;
	}
	c->dur_count = 0;
	c->dur_cap = 0;
}

static void cassette_clear(Cassette* c)
{
	c->dur_count = 0;
	c->play_index = 0;
	c->play_next_edge_tick = 0;
	c->play_level = c->idle_level;
	c->in_level = c->idle_level;
}

bool cassette_open(Cassette* c, const char *path)
{
	if (!path || !path[0]) return false;
	strncpy(c->path, path, sizeof(c->path)-1);
	c->path[sizeof(c->path)-1] = '\0';

	/* If file exists, try to load it. If not, we simply "attach" it for recording. */
	FILE *f = fopen(path, "rb");
	if (!f) {
		cassette_clear(c);
		c->attached = true;
		return true;
	}

	CassFileHdr h;
	size_t n = fread(&h, 1, sizeof(h), f);
	if (n != sizeof(h) || memcmp(h.magic, k_magic, CAS_MAGIC_LEN) != 0 || h.version != 1) {
		fclose(f);
		cassette_clear(c);
		c->attached = true;
		return true;
	}

	cassette_clear(c);
	c->cpu_hz = h.cpu_hz ? h.cpu_hz : c->cpu_hz;
	c->idle_level = (h.initial_level != 0);
	c->play_level = c->idle_level;
	c->in_level = c->idle_level;

	vec_reserve(c, h.count);
	c->dur_count = 0;
	for (uint32_t i=0;i<h.count;i++) {
		uint32_t d;
		if (fread(&d, 1, sizeof(d), f) != sizeof(d)) break;
		vec_push(c, d);
	}
	fclose(f);

	c->attached = true;
	return true;
}

bool cassette_save(Cassette* c)
{
	if (!c->attached || !c->path[0]) return false;
	FILE *f = fopen(c->path, "wb");
	if (!f) return false;

	CassFileHdr h;
	memset(&h, 0, sizeof(h));
	memcpy(h.magic, k_magic, CAS_MAGIC_LEN);
	h.version = 1;
	h.cpu_hz = c->cpu_hz;
	h.initial_level = (uint8_t)(c->idle_level ? 1 : 0);
	h.count = (uint32_t)c->dur_count;

	if (fwrite(&h, 1, sizeof(h), f) != sizeof(h)) {
		fclose(f);
		return false;
	}
	if (c->dur_count) {
		if (fwrite(c->durations, sizeof(uint32_t), c->dur_count, f) != c->dur_count) {
			fclose(f);
			return false;
		}
	}
	fclose(f);
	return true;
}

void cassette_stop(Cassette* c)
{
	c->playing = false;
	if (c->recording) {
		c->recording = false;
		(void)cassette_save(c);
	}
	c->in_level = c->idle_level;
}

void cassette_rewind(Cassette* c)
{
	c->play_index = 0;
	c->play_level = c->idle_level;
	c->in_level = c->idle_level;
	c->play_next_edge_tick = 0;
}

void cassette_ff(Cassette* c, uint32_t seconds, uint64_t now_tick)
{
	if (!c->playing || c->dur_count == 0) return;
	uint64_t skip = (uint64_t)c->cpu_hz * (uint64_t)seconds;
	uint64_t target = now_tick + skip;

	/* Fast-forward by simulating edges until we pass target. */
	uint64_t t = now_tick;
	while (c->play_index < c->dur_count) {
		uint64_t dt = c->durations[c->play_index];
		if (t + dt >= target) break;
		t += dt;
		c->play_index++;
		c->play_level = !c->play_level;
	}

	c->play_next_edge_tick = t + (c->play_index < c->dur_count ? c->durations[c->play_index] : 0);
	c->in_level = c->play_level;
}

void cassette_start_play(Cassette* c, uint64_t now_tick)
{
	if (!c->attached) return;
	c->recording = false;
	c->playing = true;
	c->play_level = c->idle_level;
	c->in_level = c->play_level;
	c->play_index = 0;
	c->play_next_edge_tick = now_tick + (c->dur_count ? c->durations[0] : 0);
}

void cassette_start_record(Cassette* c, uint64_t now_tick)
{
	if (!c->attached) return;
	cassette_clear(c);
	c->recording = true;
	c->playing = false;
	c->rec_last_edge_tick = now_tick;
	c->rec_last_level = false;
	c->idle_level = true;
	c->in_level = c->idle_level;
}

void cassette_on_out_change(Cassette* c, uint64_t tick, bool new_level)
{
	if (!c->recording) return;
	uint64_t dt64 = tick - c->rec_last_edge_tick;
	uint32_t dt = (dt64 > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)dt64;
	vec_push(c, dt);
	c->rec_last_edge_tick = tick;
	c->rec_last_level = new_level;
}

bool cassette_in_level_at(Cassette* c, uint64_t tick)
{
	if (!c->playing || c->dur_count == 0) {
		c->in_level = c->idle_level;
		return c->in_level;
	}

	/* Advance edges that have occurred. */
	while (c->play_index < c->dur_count && tick >= c->play_next_edge_tick) {
		c->play_level = !c->play_level;
		c->play_index++;
		if (c->play_index < c->dur_count) {
			c->play_next_edge_tick += c->durations[c->play_index];
		}
	}

	c->in_level = c->play_level;
	return c->in_level;
}

const char *cassette_status(const Cassette* c)
{
	if (!c->attached) return "cassette: (none)";
	if (c->recording) return "cassette: REC";
	if (c->playing) return "cassette: PLAY";
	return "cassette: STOP";
}
