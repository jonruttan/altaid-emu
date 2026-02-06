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
	cfg->hold_ms = 50u;
	cfg->realtime = true;
	cfg->log_flush = true;
	cfg->panel_text_mode = PANEL_TEXT_MODE_BURST;
	cfg->panel_compact = true;
	cfg->state_file = "altaid.state";
	cfg->ram_file = "altaid.ram";
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
		"  -a, --serial-append       When --serial-out is a file, append instead of truncating.\n"
		"\n"
		"Cassette options (Altaid05 @ ports 0x44/0x45):\n"
		"  -c, --cass <file>         Attach cassette file (ALTAP001).\n"
		"  -L, --cass-play           Start playing at tick 0.\n"
		"  -R, --cass-rec            Start recording at tick 0 (overwrites on exit).\n"
		"\n"
		"State / RAM options:\n"
		"  -s, --state-file <file>   Default state file for Ctrl-P save/load.\n"
		"  -J, --state-load <file>   Load full machine state at startup.\n"
		"  -W, --state-save <file>   Save full machine state on exit.\n"
		"  -M, --ram-file <file>     Default RAM file for Ctrl-P save/load.\n"
		"  -G, --ram-load <file>     Load RAM banks at startup.\n"
		"  -B, --ram-save <file>     Save RAM banks on exit.\n"
		"\n"
		"Other options:\n"
		"  -H, --hold <ms>           Momentary key press duration (default 50).\n"
		"  -r, --realtime            Throttle emulation to real-time (default on).\n"
		"  -z, --turbo               Run as fast as possible (disables --realtime).\n"
		"  -l, --log <file>          Write non-panel messages to a log file.\n"
		"  -f, --log-flush <0|1>     Flush log on each write (default 1).\n"
		"  -q, --quiet               Suppress non-essential messages (still prints PTY path).\n"
		"  -n, --headless            Do not enter raw mode and do not enable UI keybindings.\n"
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
		{"serial-fd",     required_argument, 0, 'S'},
		{"serial-append", no_argument,       0, 'a'},
		{"cass",          required_argument, 0, 'c'},
		{"cass-play",     no_argument,       0, 'L'},
		{"cass-rec",      no_argument,       0, 'R'},
		{"state-file",    required_argument, 0, 's'},
		{"state-load",    required_argument, 0, 'J'},
		{"state-save",    required_argument, 0, 'W'},
		{"ram-file",      required_argument, 0, 'M'},
		{"ram-load",      required_argument, 0, 'G'},
		{"ram-save",      required_argument, 0, 'B'},
		{"log",           required_argument, 0, 'l'},
		{"log-flush",     required_argument, 0, 'f'},
		{"quiet",         no_argument,       0, 'q'},
		{"headless",      no_argument,       0, 'n'},
		{"realtime",      no_argument,       0, 'r'},
		{"turbo",         no_argument,       0, 'z'},
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
			"hVputIEm:b:C:o:S:alqnc:LRNAF:y:x:H:rzf:kv"
			"s:J:W:M:G:B:",
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
		case 's':
			cfg->state_file = optarg;
			break;
		case 'J':
			cfg->state_load_path = optarg;
			break;
		case 'W':
			cfg->state_save_path = optarg;
			break;
		case 'M':
			cfg->ram_file = optarg;
			break;
		case 'G':
			cfg->ram_load_path = optarg;
			break;
		case 'B':
			cfg->ram_save_path = optarg;
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
