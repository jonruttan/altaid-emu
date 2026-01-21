/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_CASSETTE_H
#define ALTAID_EMU_CASSETTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
* Minimal digital-cassette model.
*
* Hardware view (ALTAID05):
*  - ROM drives cassette output by writing bit0 to OUT 0x44 (CASSETTE).
*  - ROM samples cassette input on IN 0x40 bit6 (INPUT_PORT).
*
* Emulator strategy:
*  - Record/play back edge timings (durations between level changes)
*    measured in CPU ticks.
*  - This avoids assuming a fixed bit rate or encoding; any ROM/software
*    that produces/consumes a digital waveform on those lines should work.
*/

typedef struct {
	bool		attached;
	char		path[512];

	uint32_t	cpu_hz;

	/* idle/"no tape" level */
	bool		idle_level;
	bool		in_level; /* current level presented to the machine */

	/* playback */
	bool		playing;
	bool		play_level;
	size_t		play_index; /* next duration index */
	uint64_t	play_next_edge_tick;

	/* recording */
	bool		recording;
	uint64_t	rec_last_edge_tick;
	bool		rec_last_level;

	/* pulse durations (CPU ticks between edges) */
	uint32_t	*durations;
	size_t		dur_count;
	size_t		dur_cap;
} Cassette;

void cassette_init(Cassette *c, uint32_t cpu_hz);
void cassette_free(Cassette *c);

/* Attach a tape image (loads if it exists; otherwise attaches empty). */
bool cassette_open(Cassette *c, const char *path);

/* Persist current tape contents to the attached file. */
bool cassette_save(Cassette *c);

/* Transport */
void cassette_stop(Cassette *c);
void cassette_rewind(Cassette *c);
void cassette_ff(Cassette *c, uint32_t seconds, uint64_t now_tick);
void cassette_start_play(Cassette *c, uint64_t now_tick);
void cassette_start_record(Cassette *c, uint64_t now_tick);

/* Called when OUT 0x44 changes while recording. */
void cassette_on_out_change(Cassette *c, uint64_t tick, bool new_level);

/* Sample cassette input level at a given tick (returns idle when stopped). */
bool cassette_in_level_at(Cassette *c, uint64_t tick);

const char *cassette_status(const Cassette *c);

#endif /* ALTAID_EMU_CASSETTE_H */
