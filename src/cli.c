/* SPDX-License-Identifier: MIT */

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#include "cli.h"

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cfg_defaults(struct Config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->cpu_hz = 2000000u;
	cfg->baud = 9600u;
	cfg->panel_hz = 0;
	cfg->hold_ms = 300u;
	cfg->realtime = true;
	cfg->log_flush = true;
	cfg->panel_text_mode = PANEL_TEXT_MODE_BURST;
	cfg->panel_compact = true;
}

/*
 * Parse a persistence spec.  Returns 0 on success, -1 on error.
 *
 *   "state:<path>"
 *   "ram:<path>"
 *   "ram@<addr>:<path>"
 *   "ram@<bank>.<addr>:<path>"
 *
 * <addr> and <bank> accept hex (0x…) or decimal.
 */
static int parse_io_spec(const char *arg, struct IoSpec *out)
{
	if (!arg || !out)
		return -1;

	memset(out, 0, sizeof(*out));

	if (!strncmp(arg, "state:", 6)) {
		if (!arg[6])
			return -1;
		out->kind = IO_SPEC_STATE;
		out->path = arg + 6;
		return 0;
	}

	if (!strncmp(arg, "ram:", 4)) {
		if (!arg[4])
			return -1;
		out->kind = IO_SPEC_RAM;
		out->path = arg + 4;
		return 0;
	}

	if (!strncmp(arg, "ram@", 4)) {
		const char *p = arg + 4;
		const char *colon;
		char buf[64];
		char *dot;
		char *endp;
		unsigned long bank = 0;
		unsigned long addr;
		size_t len;

		colon = strchr(p, ':');
		if (!colon || colon == p || !colon[1])
			return -1;

		len = (size_t)(colon - p);
		if (len >= sizeof(buf))
			return -1;
		memcpy(buf, p, len);
		buf[len] = '\0';

		dot = strchr(buf, '.');
		if (dot) {
			*dot = '\0';
			errno = 0;
			bank = strtoul(buf, &endp, 0);
			if (errno || endp == buf || *endp || bank > 7)
				return -1;
			errno = 0;
			addr = strtoul(dot + 1, &endp, 0);
			if (errno || endp == dot + 1 || *endp || addr > 0xFFFF)
				return -1;
		} else {
			errno = 0;
			addr = strtoul(buf, &endp, 0);
			if (errno || endp == buf || *endp || addr > 0xFFFF)
				return -1;
		}

		out->kind = IO_SPEC_RAM;
		out->bank = (unsigned)bank;
		out->addr = (uint16_t)addr;
		out->has_addr = true;
		out->path = colon + 1;
		return 0;
	}

	return -1;
}

static int push_spec(struct IoSpec *list, unsigned *count, unsigned max,
		     const char *arg)
{
	if (*count >= max) {
		fprintf(stderr, "too many load/save/default specs (max %u)\n", max);
		return -1;
	}
	if (parse_io_spec(arg, &list[*count]) < 0) {
		fprintf(stderr, "bad spec: %s\n", arg);
		return -1;
	}
	(*count)++;
	return 0;
}

static int parse_u32(const char *s, uint32_t *out)
{
	char *end = NULL;
	unsigned long v;

	if (!s || !*s || !out)
		return -1;

	errno = 0;
	v = strtoul(s, &end, 10);
	if (errno || end == s || *end)
		return -1;
	if (v > UINT32_MAX)
		return -1;

	*out = (uint32_t)v;
	return 0;
}

static int parse_i32(const char *s, int *out)
{
	char *end = NULL;
	long v;

	if (!s || !*s || !out)
		return -1;

	errno = 0;
	v = strtol(s, &end, 10);
	if (errno || end == s || *end)
		return -1;
	if (v < INT32_MIN || v > INT32_MAX)
		return -1;

	*out = (int)v;
	return 0;
}

void cli_usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s <rom64k.bin> [options]\n"
		"\n"
		"ROM (required):\n"
		"  <rom64k.bin>     64 KiB ROM image. Not distributed with this project.\n"
		"\n"
		"Core options:\n"
		"  -C, --hz <cpu_hz>         CPU clock (default 2000000).\n"
		"  -b, --baud <baud>         Serial baud for bit-level UART (default 9600).\n"
		"\n"
		"Panel / TUI options:\n"
		"  -p, --panel               Enable front-panel display.\n"
		"  -m, --panel-mode <burst|change>\n"
		"                          Text-mode output policy (default burst).\n"
		"  -E, --panel-echo-chars    In burst mode, also snapshot on single-char echoes.\n"
		"  -k, --panel-compact       Text panel: one-line compact output (default).\n"
		"  -v, --panel-verbose       Text panel: verbose multi-line dump.\n"
		"  -u, --ui                  Full-screen terminal UI (panel + serial split).\n"
		"  -N, --no-altscreen        In --ui refresh mode, do not enter alternate screen.\n"
		"  -A, --ascii               In --ui mode, force ASCII (panel borders and LED glyphs).\n"
		"  -F, --panel-hz <n>        Panel refresh rate override.\n"
		"  -y, --term-rows <n>       Override probed terminal rows (0 means probe).\n"
		"  -x, --term-cols <n>       Override probed terminal cols (0 means probe).\n"
		"\n"
		"I/O options:\n"
		"  -t, --pty                 Expose emulated serial via a host PTY.\n"
		"  -I, --pty-input           In --pty mode, allow local keyboard input as serial RX.\n"
		"  -S, --serial-fd <stdout|stderr>\n"
		"                          Choose terminal stream for decoded TX bytes (non-PTY).\n"
		"  -o, --serial-out <dest>   Send decoded TX bytes to: stdout|stderr|-|none|<file>.\n"
		"  -i, --serial-in <src>     Feed emulated UART RX from: stdin|-|none.\n"
		"                            Default: stdin when --headless without --pty, else none.\n"
		"  -a, --serial-append       When --serial-out is a file, append instead of truncating.\n"
		"\n"
		"Cassette options (Altaid05 @ ports 0x44/0x45):\n"
		"  -c, --cass <file>         Attach cassette file (ALTAP001).\n"
		"  -L, --cass-play           Start playing at tick 0.\n"
		"  -R, --cass-rec            Start recording at tick 0 (overwrites on exit).\n"
		"\n"
		"Persistence options (repeatable):\n"
		"  --load <spec>             Load bytes at startup (see SPECS below).\n"
		"  --save <spec>             Save bytes on exit.\n"
		"  --default <spec>          Default spec for Ctrl-P save/load.\n"
		"\n"
		"  SPECS:\n"
		"    state:<file>                CPU + devices + RAM snapshot.\n"
		"    ram:<file>                  Full 512 KiB RAM.\n"
		"    ram@<addr>:<file>           Raw blob, bank 0, at address.\n"
		"    ram@<bank>.<addr>:<file>    Raw blob, specific bank, at address.\n"
		"\n"
		"Other options:\n"
		"  -H, --hold <ms>           Momentary key press duration (default 300).\n"
		"  -r, --realtime            Throttle emulation to real-time (default on).\n"
		"  -z, --turbo               Run as fast as possible (disables --realtime).\n"
		"  -l, --log <file>          Write non-panel messages to a log file.\n"
		"  -f, --log-flush <0|1>     Flush log on each write (default 1).\n"
		"  -q, --quiet               Suppress non-essential messages (still prints PTY path).\n"
		"  -n, --headless            Do not enter raw mode and do not enable UI keybindings.\n"
		"  -D, --debug-panel         Log front-panel press/release/scan events (pair with --log).\n"
		"  -T, --run-ms <ms>         Exit after <ms> of emulated CPU time (0 = unlimited, default).\n"
		"  -h, --help                Show this help and exit.\n"
		"  -V, --version             Print version and exit.\n",
		argv0);
}

int cli_parse_args(int argc, char **argv, struct Config *cfg)
{
	int opt;

	static const struct option kLongOpts[] = {
		{"hz",            required_argument, 0, 'C'},
		{"baud",          required_argument, 0, 'b'},
		{"panel",         no_argument,       0, 'p'},
		{"panel-mode",    required_argument, 0, 'm'},
		{"panel-echo-chars", no_argument,     0, 'E'},
		{"panel-compact",    no_argument,       0, 'k'},
		{"panel-verbose",    no_argument,       0, 'v'},
		{"ui",            no_argument,       0, 'u'},
		{"no-altscreen",  no_argument,       0, 'N'},
		{"ascii",         no_argument,       0, 'A'},
		{"panel-hz",      required_argument, 0, 'F'},
		{"term-rows",     required_argument, 0, 'y'},
		{"term-cols",     required_argument, 0, 'x'},
		{"hold",          required_argument, 0, 'H'},
		{"pty",           no_argument,       0, 't'},
		{"pty-input",     no_argument,       0, 'I'},
		{"serial-out",    required_argument, 0, 'o'},
		{"serial-in",     required_argument, 0, 'i'},
		{"serial-fd",     required_argument, 0, 'S'},
		{"serial-append", no_argument,       0, 'a'},
		{"cass",          required_argument, 0, 'c'},
		{"cass-play",     no_argument,       0, 'L'},
		{"cass-rec",      no_argument,       0, 'R'},
		{"load",          required_argument, 0,  1 },
		{"save",          required_argument, 0,  2 },
		{"default",       required_argument, 0,  3 },
		{"log",           required_argument, 0, 'l'},
		{"log-flush",     required_argument, 0, 'f'},
		{"quiet",         no_argument,       0, 'q'},
		{"headless",      no_argument,       0, 'n'},
		{"realtime",      no_argument,       0, 'r'},
		{"turbo",         no_argument,       0, 'z'},
		{"debug-panel",   no_argument,       0, 'D'},
		{"run-ms",        required_argument, 0, 'T'},
		{"help",          no_argument,       0, 'h'},
		{"version",       no_argument,       0, 'V'},
		{0, 0, 0, 0},
	};

	if (!cfg)
		return -2;

	cfg_defaults(cfg);
	opterr = 0;

	for (;;) {
		opt = getopt_long(argc, argv,
			"hVputIEm:b:C:o:i:S:alqnc:LRNAF:y:x:H:rzf:kvDT:",
			kLongOpts, NULL);
		if (opt == -1)
			break;

		switch (opt) {
		case 'C':
			if (parse_u32(optarg, &cfg->cpu_hz) < 0)
				return -2;
			break;
		case 'b':
			if (parse_u32(optarg, &cfg->baud) < 0)
				return -2;
			break;
		case 'p':
			cfg->start_panel = true;
			break;
		case 'm':
			if (!optarg)
				return -2;
			if (!strcmp(optarg, "burst"))
				cfg->panel_text_mode = PANEL_TEXT_MODE_BURST;
			else if (!strcmp(optarg, "change"))
				cfg->panel_text_mode = PANEL_TEXT_MODE_CHANGE;
			else
				return -2;
			break;
		case 'E':
			cfg->panel_echo_chars = true;
			break;
		case 'k':
			cfg->panel_compact = true;
			break;
		case 'v':
			cfg->panel_compact = false;
			break;
		case 'u':
			cfg->start_ui = true;
			break;
		case 'N':
			cfg->no_altscreen = true;
			break;
		case 'A':
			cfg->use_ascii = true;
			break;
		case 'F':
			if (parse_u32(optarg, &cfg->panel_hz) < 0)
				return -2;
			cfg->panel_hz_set = true;
			break;
		case 'y':
			if (parse_i32(optarg, &cfg->term_rows) < 0)
				return -2;
			cfg->term_override = true;
			break;
		case 'x':
			if (parse_i32(optarg, &cfg->term_cols) < 0)
				return -2;
			cfg->term_override = true;
			break;
		case 'H':
			if (parse_u32(optarg, &cfg->hold_ms) < 0)
				return -2;
			break;
		case 't':
			cfg->use_pty = true;
			break;
		case 'I':
			cfg->use_pty = true;
			cfg->pty_input = true;
			break;
		case 'o':
			cfg->serial_out_spec = optarg;
			break;
		case 'i':
			if (!optarg) return -2;
			if (strcmp(optarg, "stdin") != 0
			    && strcmp(optarg, "-") != 0
			    && strcmp(optarg, "none") != 0) {
				fprintf(stderr, "--serial-in: expected stdin|-|none, got %s\n", optarg);
				return -2;
			}
			cfg->serial_in_spec = optarg;
			break;
		case 'S':
			cfg->serial_fd_spec = optarg;
			break;
		case 'a':
			cfg->serial_append = true;
			break;
		case 'c':
			cfg->cassette_path = optarg;
			break;
		case 'L':
			cfg->cassette_play = true;
			break;
		case 'R':
			cfg->cassette_rec = true;
			break;
		case 1: /* --load */
			if (push_spec(cfg->load_specs, &cfg->load_count,
				      CLI_IO_SPEC_MAX, optarg) < 0)
				return -2;
			break;
		case 2: /* --save */
			if (push_spec(cfg->save_specs, &cfg->save_count,
				      CLI_IO_SPEC_MAX, optarg) < 0)
				return -2;
			if (cfg->save_specs[cfg->save_count - 1].kind == IO_SPEC_RAM
			    && cfg->save_specs[cfg->save_count - 1].has_addr) {
				fprintf(stderr,
					"--save ram@addr not supported (save is full-ram only)\n");
				return -2;
			}
			break;
		case 3: /* --default */
			if (push_spec(cfg->default_specs, &cfg->default_count,
				      CLI_IO_SPEC_MAX, optarg) < 0)
				return -2;
			break;
		case 'l':
			cfg->log_path = optarg;
			break;
		case 'f': {
			uint32_t v;
			if (parse_u32(optarg, &v) < 0)
				return -2;
			if (v != 0 && v != 1)
				return -2;
			cfg->log_flush = v ? true : false;
			break;
		}
		case 'q':
			cfg->quiet = true;
			break;
		case 'n':
			cfg->headless = true;
			break;
		case 'r':
			cfg->realtime = true;
			break;
		case 'z':
			cfg->realtime = false;
			break;
		case 'D':
			cfg->debug_panel = true;
			break;
		case 'T':
			if (parse_u32(optarg, &cfg->max_run_ms) < 0)
				return -2;
			break;
		case 'h':
			cfg->show_help = true;
			break;
		case 'V':
			cfg->show_version = true;
			break;
		default:
			return -2;
		}
	}

	/* ROM is the required positional argument. */
	if (optind < argc)
		cfg->rom_path = argv[optind++];
	if (optind < argc) {
		fprintf(stderr, "Unexpected extra argument: %s\n", argv[optind]);
		return -2;
	}

	return 0;
}
