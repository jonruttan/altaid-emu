/*
 * cassette.spec.c
 *
 * Unit tests for cassette helpers (no file I/O).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cassette.c"

#include "test-runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int make_temp_path(char *out, size_t cap)
{
	int fd;

	if (!out || cap < 32)
		return -1;

	snprintf(out, cap, "/tmp/altaid-cass-XXXXXX");
	fd = mkstemp(out);
	if (fd < 0)
		return -1;
	close(fd);
	return 0;
}

static void record_two_pulses(Cassette *c, uint64_t start, uint64_t t1,
			      uint64_t t2)
{
	c->attached = true;
	cassette_start_record(c, start);
	cassette_on_out_change(c, t1, true);
	cassette_on_out_change(c, t2, false);
	cassette_stop(c);
}

static char *test_cassette_init_defaults(void)
{
	Cassette c;

	cassette_init(&c, 1234u);

	_it_should(
		"init sets defaults",
		1234u == c.cpu_hz
		&& true == c.idle_level
		&& true == c.in_level
		&& false == c.attached
	);

	_it_should(
		"status shows none when detached",
		0 == strcmp(cassette_status(&c), "cassette: (none)")
	);

	cassette_free(&c);
	return NULL;
}

static char *test_cassette_record_stop_status(void)
{
	Cassette c;

	cassette_init(&c, 2000000u);
	c.attached = true;

	cassette_start_record(&c, 100u);

	_it_should(
		"record mode sets status",
		CASSETTE_RECORDING == c.state
		&& 0 == strcmp(cassette_status(&c), "cassette: REC")
	);

	cassette_on_out_change(&c, 110u, true);
	cassette_on_out_change(&c, 140u, false);

	_it_should(
		"record pushes durations",
		2u == c.dur_count
		&& 10u == c.durations[0]
		&& 30u == c.durations[1]
	);

	cassette_stop(&c);

	_it_should(
		"stop clears record/play flags",
		CASSETTE_STOPPED == c.state
		&& true == c.in_level
		&& 0 == strcmp(cassette_status(&c), "cassette: STOP")
	);

	cassette_free(&c);
	return NULL;
}

static char *test_cassette_playback_levels(void)
{
	Cassette c;

	cassette_init(&c, 2000000u);
	record_two_pulses(&c, 100u, 110u, 140u);

	cassette_start_play(&c, 200u);

	_it_should(
		"playback starts at idle",
		CASSETTE_PLAYING == c.state
		&& 210u == c.play_next_edge_tick
		&& true == c.play_level
		&& true == c.in_level
		&& 0 == strcmp(cassette_status(&c), "cassette: PLAY")
	);

	_it_should(
		"level stays high before first edge",
		true == cassette_in_level_at(&c, 205u)
	);

	_it_should(
		"level flips at first edge",
		false == cassette_in_level_at(&c, 210u)
	);

	_it_should(
		"level flips at second edge",
		true == cassette_in_level_at(&c, 240u)
	);

	cassette_free(&c);
	return NULL;
}

static char *test_cassette_ff_skips_edges(void)
{
	Cassette c;

	cassette_init(&c, 15u);
	record_two_pulses(&c, 0u, 10u, 20u);
	cassette_start_play(&c, 0u);

	cassette_ff(&c, 1u, 0u);

	_it_should(
		"ff advances play index",
		1u == c.play_index
		&& false == c.play_level
		&& 20u == c.play_next_edge_tick
		&& false == c.in_level
	);

	cassette_free(&c);
	return NULL;
}

static char *test_cassette_round_trip_playback(void)
{
	Cassette c;

	cassette_init(&c, 2000000u);

	c.attached = true;
	cassette_start_record(&c, 0u);
	cassette_on_out_change(&c, 5u, true);
	cassette_on_out_change(&c, 15u, false);
	cassette_on_out_change(&c, 30u, true);
	cassette_stop(&c);

	_it_should(
		"recorded transcript has three durations",
		3u == c.dur_count
		&& 5u == c.durations[0]
		&& 10u == c.durations[1]
		&& 15u == c.durations[2]
	);

	cassette_start_play(&c, 100u);

	_it_should(
		"playback follows recorded edges",
		true == cassette_in_level_at(&c, 102u)
		&& false == cassette_in_level_at(&c, 105u)
		&& false == cassette_in_level_at(&c, 110u)
		&& true == cassette_in_level_at(&c, 115u)
		&& false == cassette_in_level_at(&c, 130u)
	);

	cassette_free(&c);
	return NULL;
}

static int write_file(const char *path, const void *data, size_t len)
{
	FILE *f;
	size_t n;

	f = fopen(path, "wb");
	if (!f)
		return -1;
	n = fwrite(data, 1, len, f);
	fclose(f);
	return (n == len) ? 0 : -1;
}

static char *test_cassette_open_missing_file_attaches_empty(void)
{
	Cassette c;
	char path[64];
	bool ok;

	cassette_init(&c, 2000000u);

	/* Build a unique path that definitely does not exist. */
	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";
	unlink(path); /* delete before opening */

	ok = cassette_open(&c, path);
	_it_should(
		"attach empty when file is missing",
		true == ok
		&& true == c.attached
		&& 0u == c.dur_count
	);

	cassette_free(&c);
	return NULL;
}

static char *test_cassette_open_truncated_header_attaches_empty(void)
{
	Cassette c;
	char path[64];
	unsigned char short_buf[4] = { 'A', 'L', 'T', 'A' };
	bool ok;

	cassette_init(&c, 2000000u);

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";
	if (write_file(path, short_buf, sizeof(short_buf)) != 0) {
		unlink(path);
		return "write_file() failed";
	}

	ok = cassette_open(&c, path);
	_it_should(
		"treat truncated header as empty attach (no crash)",
		true == ok
		&& true == c.attached
		&& 0u == c.dur_count
	);

	unlink(path);
	cassette_free(&c);
	return NULL;
}

static char *test_cassette_open_bad_magic_attaches_empty(void)
{
	Cassette c;
	char path[64];
	unsigned char buf[sizeof(CassFileHdr)];
	bool ok;

	cassette_init(&c, 2000000u);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, "BADMAGIC", 8);

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";
	if (write_file(path, buf, sizeof(buf)) != 0) {
		unlink(path);
		return "write_file() failed";
	}

	ok = cassette_open(&c, path);
	_it_should(
		"reject bad magic by attaching empty",
		true == ok
		&& true == c.attached
		&& 0u == c.dur_count
	);

	unlink(path);
	cassette_free(&c);
	return NULL;
}

static char *test_cassette_open_wrong_version_attaches_empty(void)
{
	Cassette c;
	char path[64];
	unsigned char buf[sizeof(CassFileHdr)];
	CassFileHdr h;
	bool ok;

	cassette_init(&c, 2000000u);

	memset(&h, 0, sizeof(h));
	memcpy(h.magic, k_magic, CAS_MAGIC_LEN);
	h.version = 99;	/* unsupported */
	h.cpu_hz = 2000000u;
	h.initial_level = 1;
	h.count = 0;
	memcpy(buf, &h, sizeof(buf));

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";
	if (write_file(path, buf, sizeof(buf)) != 0) {
		unlink(path);
		return "write_file() failed";
	}

	ok = cassette_open(&c, path);
	_it_should(
		"reject unsupported version by attaching empty",
		true == ok
		&& true == c.attached
		&& 0u == c.dur_count
	);

	unlink(path);
	cassette_free(&c);
	return NULL;
}

static char *test_cassette_open_truncated_durations_loads_partial(void)
{
	Cassette c;
	char path[64];
	unsigned char buf[sizeof(CassFileHdr) + 2 * sizeof(uint32_t)];
	CassFileHdr h;
	uint32_t d0 = 42u;
	uint32_t d1 = 99u;
	bool ok;

	cassette_init(&c, 2000000u);

	memset(&h, 0, sizeof(h));
	memcpy(h.magic, k_magic, CAS_MAGIC_LEN);
	h.version = 1;
	h.cpu_hz = 2000000u;
	h.initial_level = 1;
	h.count = 10;	/* claim 10, provide 2 */
	memcpy(buf, &h, sizeof(h));
	memcpy(buf + sizeof(h), &d0, sizeof(d0));
	memcpy(buf + sizeof(h) + sizeof(d0), &d1, sizeof(d1));

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";
	if (write_file(path, buf, sizeof(buf)) != 0) {
		unlink(path);
		return "write_file() failed";
	}

	ok = cassette_open(&c, path);
	_it_should(
		"load only the durations actually present (no OOB read)",
		true == ok
		&& true == c.attached
		&& 2u == c.dur_count
		&& 42u == c.durations[0]
		&& 99u == c.durations[1]
	);

	unlink(path);
	cassette_free(&c);
	return NULL;
}

static char *test_cassette_file_round_trip(void)
{
	Cassette src;
	Cassette dst;
	char path[64];
	bool ok;

	cassette_init(&src, 2000000u);
	src.attached = true;
	cassette_start_record(&src, 0u);
	cassette_on_out_change(&src, 4u, true);
	cassette_on_out_change(&src, 9u, false);
	cassette_on_out_change(&src, 20u, true);
	cassette_stop(&src);

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";

	strncpy(src.path, path, sizeof(src.path) - 1);
	src.path[sizeof(src.path) - 1] = '\0';

	ok = cassette_save(&src);
	_it_should(
		"save cassette to file",
		true == ok
	);

	cassette_init(&dst, 0u);
	ok = cassette_open(&dst, path);
	_it_should(
		"load cassette from file",
		true == ok
		&& true == dst.attached
		&& 3u == dst.dur_count
		&& 4u == dst.durations[0]
		&& 5u == dst.durations[1]
		&& 11u == dst.durations[2]
	);

	unlink(path);
	cassette_free(&src);
	cassette_free(&dst);
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_cassette_init_defaults);
	_run_test(test_cassette_record_stop_status);
	_run_test(test_cassette_playback_levels);
	_run_test(test_cassette_ff_skips_edges);
	_run_test(test_cassette_round_trip_playback);
	_run_test(test_cassette_open_missing_file_attaches_empty);
	_run_test(test_cassette_open_truncated_header_attaches_empty);
	_run_test(test_cassette_open_bad_magic_attaches_empty);
	_run_test(test_cassette_open_wrong_version_attaches_empty);
	_run_test(test_cassette_open_truncated_durations_loads_partial);
	_run_test(test_cassette_file_round_trip);

	return NULL;
}
