/* SPDX-License-Identifier: MIT */

/*
 * cli.spec.c
 *
 * Unit tests for CLI parsing helpers.
 */

#include "cli.c"

#include "test-runner.h"

#include <string.h>
#include <unistd.h>
#include <limits.h>

static void reset_getopt(void)
{
	optind = 1;
	opterr = 0;
}

static char *test_cfg_defaults(void)
{
	struct Config cfg;

	cfg_defaults(&cfg);

	_it_should(
		"defaults set core and panel options",
		2000000u == cfg.cpu_hz
		&& 9600u == cfg.baud
		&& 0u == cfg.panel_hz
		&& 300u == cfg.hold_ms
		&& true == cfg.realtime
		&& true == cfg.log_flush
		&& PANEL_TEXT_MODE_BURST == cfg.panel_text_mode
		&& true == cfg.panel_compact
		&& 0u == cfg.load_count
		&& 0u == cfg.save_count
		&& 0u == cfg.default_count
		&& 0u == cfg.max_run_ms
	);

	return NULL;
}

static char *test_parse_u32(void)
{
	uint32_t out = 0;

	_it_should(
		"parse_u32 rejects invalid input",
		-1 == parse_u32(NULL, &out)
		&& -1 == parse_u32("", &out)
		&& -1 == parse_u32("999999999999", &out)
		&& -1 == parse_u32("-1", &out)
		&& -1 == parse_u32("4294967296", &out)
		&& -1 == parse_u32("12x", &out)
	);

	_it_should(
		"parse_u32 accepts valid input",
		0 == parse_u32("123", &out)
		&& 123u == out
		&& 0 == parse_u32("0", &out)
		&& 0u == out
		&& 0 == parse_u32("4294967295", &out)
		&& UINT32_MAX == out
	);

	return NULL;
}

static char *test_parse_i32(void)
{
	int out = 0;

	_it_should(
		"parse_i32 rejects invalid input",
		-1 == parse_i32(NULL, &out)
		&& -1 == parse_i32("", &out)
		&& -1 == parse_i32("999999999999", &out)
		&& -1 == parse_i32("2147483648", &out)
		&& -1 == parse_i32("-2147483649", &out)
		&& -1 == parse_i32("1x", &out)
	);

	_it_should(
		"parse_i32 accepts valid input",
		0 == parse_i32("-12", &out)
		&& -12 == out
		&& 0 == parse_i32("0", &out)
		&& 0 == out
		&& 0 == parse_i32("2147483647", &out)
		&& INT32_MAX == out
		&& 0 == parse_i32("-2147483648", &out)
		&& INT32_MIN == out
	);

	return NULL;
}

static char *test_parse_args_rejects_null_cfg(void)
{
	char *argv[] = { "prog", "rom.bin", NULL };

	reset_getopt();
	_it_should(
		"reject null cfg pointer",
		-2 == cli_parse_args(2, argv, NULL)
	);

	return NULL;
}

static char *test_parse_args_basic(void)
{
	struct Config cfg;
	char *argv[] = { "prog", "rom.bin", NULL };
	int argc = 2;

	reset_getopt();
	_it_should(
		"parse args sets rom path",
		0 == cli_parse_args(argc, argv, &cfg)
		&& 0 == strcmp(cfg.rom_path, "rom.bin")
	);

	return NULL;
}

static char *test_parse_args_flags(void)
{
	struct Config cfg;
	char *argv[] = {
		"prog", "--panel", "--panel-mode", "change",
		"--term-rows", "40", "--term-cols", "100",
		"--log-flush", "0", "rom.bin", NULL
	};
	int argc = 11;

	reset_getopt();
	_it_should(
		"parse args sets flags and overrides",
		0 == cli_parse_args(argc, argv, &cfg)
		&& true == cfg.start_panel
		&& PANEL_TEXT_MODE_CHANGE == cfg.panel_text_mode
		&& true == cfg.term_override
		&& 40 == cfg.term_rows
		&& 100 == cfg.term_cols
		&& false == cfg.log_flush
	);

	return NULL;
}

static char *test_parse_args_more_flags(void)
{
	struct Config cfg;
	char *argv[] = {
		"prog",
		"--ui", "--no-altscreen", "--ascii",
		"--panel-echo-chars",
		"--panel-hz", "60",
		"--hz", "123456",
		"--baud", "19200",
		"--hold", "10",
		"--pty-input",
		"--serial-out", "none",
		"--serial-fd", "stderr",
		"--serial-append",
		"--log", "emu.log",
		"--quiet",
		"--headless",
		"rom.bin",
		NULL
	};
	int argc = 24;

	reset_getopt();
	_it_should(
		"parse args sets more flags and paths",
		0 == cli_parse_args(argc, argv, &cfg)
		&& true == cfg.start_ui
		&& true == cfg.no_altscreen
		&& true == cfg.use_ascii
		&& true == cfg.panel_echo_chars
		&& true == cfg.panel_hz_set
		&& 60u == cfg.panel_hz
		&& 123456u == cfg.cpu_hz
		&& 19200u == cfg.baud
		&& 10u == cfg.hold_ms
		&& true == cfg.use_pty
		&& true == cfg.pty_input
		&& 0 == strcmp(cfg.serial_out_spec, "none")
		&& 0 == strcmp(cfg.serial_fd_spec, "stderr")
		&& true == cfg.serial_append
		&& 0 == strcmp(cfg.log_path, "emu.log")
		&& true == cfg.quiet
		&& true == cfg.headless
		&& 0 == strcmp(cfg.rom_path, "rom.bin")
	);

	return NULL;
}

static char *test_parse_args_panel_compact_precedence(void)
{
	struct Config cfg;
	char *argv_default[] = { "prog", "rom.bin", NULL };
	char *argv_verbose[] = { "prog", "--panel-verbose", "rom.bin", NULL };
	char *argv_compact[] = { "prog", "--panel-compact", "rom.bin", NULL };
	char *argv_v_then_c[] = { "prog", "--panel-verbose", "--panel-compact", "rom.bin", NULL };
	char *argv_c_then_v[] = { "prog", "--panel-compact", "--panel-verbose", "rom.bin", NULL };

	reset_getopt();
	_it_should(
		"default is compact",
		0 == cli_parse_args(2, argv_default, &cfg)
		&& true == cfg.panel_compact
	);

	reset_getopt();
	_it_should(
		"--panel-verbose sets verbose",
		0 == cli_parse_args(3, argv_verbose, &cfg)
		&& false == cfg.panel_compact
	);

	reset_getopt();
	_it_should(
		"--panel-compact sets compact",
		0 == cli_parse_args(3, argv_compact, &cfg)
		&& true == cfg.panel_compact
	);

	reset_getopt();
	_it_should(
		"last panel verbosity flag wins (verbose then compact)",
		0 == cli_parse_args(4, argv_v_then_c, &cfg)
		&& true == cfg.panel_compact
	);

	reset_getopt();
	_it_should(
		"last panel verbosity flag wins (compact then verbose)",
		0 == cli_parse_args(4, argv_c_then_v, &cfg)
		&& false == cfg.panel_compact
	);

	return NULL;
}

static char *test_parse_args_realtime_precedence(void)
{
	struct Config cfg;
	char *argv_turbo_then_rt[] = { "prog", "--turbo", "--realtime", "rom.bin", NULL };
	char *argv_rt_then_turbo[] = { "prog", "--realtime", "--turbo", "rom.bin", NULL };

	reset_getopt();
	_it_should(
		"last realtime/turbo flag wins (turbo then realtime)",
		0 == cli_parse_args(4, argv_turbo_then_rt, &cfg)
		&& true == cfg.realtime
	);

	reset_getopt();
	_it_should(
		"last realtime/turbo flag wins (realtime then turbo)",
		0 == cli_parse_args(4, argv_rt_then_turbo, &cfg)
		&& false == cfg.realtime
	);

	return NULL;
}

static char *test_parse_args_sets_cassette_and_persistence_specs(void)
{
	struct Config cfg;
	char *argv[] = {
		"prog",
		"--cass", "tape.altap",
		"--cass-play",
		"--load", "state:load.state",
		"--save", "state:save.state",
		"--default", "state:altaid.state",
		"--load", "ram@0x0100:test.bin",
		"--load", "ram@2.0x2000:lib.bin",
		"--save", "ram:snapshot.ram",
		"--default", "ram@1.0x0000:altaid.ram",
		"rom.bin",
		NULL
	};
	int argc = 19;

	reset_getopt();
	_it_should(
		"parse args records cassette and load/save/default specs",
		0 == cli_parse_args(argc, argv, &cfg)
		&& 0 == strcmp(cfg.cassette_path, "tape.altap")
		&& true == cfg.cassette_play
		&& false == cfg.cassette_rec
		&& 3u == cfg.load_count
		&& IO_SPEC_STATE == cfg.load_specs[0].kind
		&& 0 == strcmp(cfg.load_specs[0].path, "load.state")
		&& IO_SPEC_RAM == cfg.load_specs[1].kind
		&& true == cfg.load_specs[1].has_addr
		&& 0u == cfg.load_specs[1].bank
		&& 0x0100u == cfg.load_specs[1].addr
		&& 0 == strcmp(cfg.load_specs[1].path, "test.bin")
		&& IO_SPEC_RAM == cfg.load_specs[2].kind
		&& 2u == cfg.load_specs[2].bank
		&& 0x2000u == cfg.load_specs[2].addr
		&& 2u == cfg.save_count
		&& IO_SPEC_STATE == cfg.save_specs[0].kind
		&& 0 == strcmp(cfg.save_specs[0].path, "save.state")
		&& IO_SPEC_RAM == cfg.save_specs[1].kind
		&& false == cfg.save_specs[1].has_addr
		&& 0 == strcmp(cfg.save_specs[1].path, "snapshot.ram")
		&& 2u == cfg.default_count
		&& IO_SPEC_STATE == cfg.default_specs[0].kind
		&& 0 == strcmp(cfg.default_specs[0].path, "altaid.state")
		&& IO_SPEC_RAM == cfg.default_specs[1].kind
		&& 1u == cfg.default_specs[1].bank
		&& 0 == strcmp(cfg.default_specs[1].path, "altaid.ram")
	);

	return NULL;
}

static char *test_parse_args_rejects_bad_spec(void)
{
	struct Config cfg;
	char *argv_missing_colon[] = {
		"prog", "--load", "ram", "rom.bin", NULL
	};
	char *argv_bad_kind[] = {
		"prog", "--load", "zz:foo", "rom.bin", NULL
	};
	char *argv_bad_addr[] = {
		"prog", "--load", "ram@nope:foo", "rom.bin", NULL
	};
	char *argv_save_addr[] = {
		"prog", "--save", "ram@0x100:foo", "rom.bin", NULL
	};

	reset_getopt();
	_it_should(
		"reject spec with no colon",
		-2 == cli_parse_args(4, argv_missing_colon, &cfg)
	);

	reset_getopt();
	_it_should(
		"reject spec with unknown kind",
		-2 == cli_parse_args(4, argv_bad_kind, &cfg)
	);

	reset_getopt();
	_it_should(
		"reject spec with bad address",
		-2 == cli_parse_args(4, argv_bad_addr, &cfg)
	);

	reset_getopt();
	_it_should(
		"reject --save ram@... (save is full-ram only)",
		-2 == cli_parse_args(4, argv_save_addr, &cfg)
	);

	return NULL;
}

static char *test_parse_args_serial_in(void)
{
	struct Config cfg_stdin;
	struct Config cfg_dash;
	struct Config cfg_none;
	struct Config cfg_bad;
	char *argv_stdin[] = { "prog", "--serial-in", "stdin", "rom.bin", NULL };
	char *argv_dash[]  = { "prog", "--serial-in", "-",     "rom.bin", NULL };
	char *argv_none[]  = { "prog", "--serial-in", "none",  "rom.bin", NULL };
	char *argv_bad[]   = { "prog", "--serial-in", "file",  "rom.bin", NULL };

	reset_getopt();
	_it_should(
		"--serial-in stdin records the spec",
		0 == cli_parse_args(4, argv_stdin, &cfg_stdin)
		&& NULL != cfg_stdin.serial_in_spec
		&& 0 == strcmp(cfg_stdin.serial_in_spec, "stdin")
	);

	reset_getopt();
	_it_should(
		"--serial-in - is accepted as stdin alias",
		0 == cli_parse_args(4, argv_dash, &cfg_dash)
		&& NULL != cfg_dash.serial_in_spec
		&& 0 == strcmp(cfg_dash.serial_in_spec, "-")
	);

	reset_getopt();
	_it_should(
		"--serial-in none is accepted",
		0 == cli_parse_args(4, argv_none, &cfg_none)
		&& NULL != cfg_none.serial_in_spec
		&& 0 == strcmp(cfg_none.serial_in_spec, "none")
	);

	reset_getopt();
	_it_should(
		"--serial-in rejects unknown values",
		-2 == cli_parse_args(4, argv_bad, &cfg_bad)
	);

	return NULL;
}

static char *test_parse_args_defaults_serial_in_unset(void)
{
	struct Config cfg;
	char *argv[] = { "prog", "rom.bin", NULL };

	reset_getopt();
	_it_should(
		"default cfg leaves serial_in_spec NULL (mode-based behavior)",
		0 == cli_parse_args(2, argv, &cfg)
		&& NULL == cfg.serial_in_spec
	);

	return NULL;
}

static char *test_parse_args_press_and_switch(void)
{
	struct Config cfg;
	char *argv[] = {
		"prog",
		"--press", "RUN",
		"--press", "D3@500",
		"--press", "mode@1000:50",
		"--switch", "D0=1",
		"--switch", "D7=0@2000",
		"rom.bin",
		NULL
	};
	int argc = 12;

	reset_getopt();
	_it_should(
		"parse args records press and switch events",
		0 == cli_parse_args(argc, argv, &cfg)
		&& 5u == cfg.panel_event_count
		/* --press RUN (key=8, at=0, hold=0 -> default) */
		&& PANEL_EVENT_PRESS == cfg.panel_events[0].kind
		&& 8u == cfg.panel_events[0].key_index
		&& 0u == cfg.panel_events[0].at_ms
		&& 0u == cfg.panel_events[0].hold_ms
		/* --press D3@500 */
		&& PANEL_EVENT_PRESS == cfg.panel_events[1].kind
		&& 3u == cfg.panel_events[1].key_index
		&& 500u == cfg.panel_events[1].at_ms
		&& 0u == cfg.panel_events[1].hold_ms
		/* --press mode@1000:50  (case-insensitive key name) */
		&& PANEL_EVENT_PRESS == cfg.panel_events[2].kind
		&& 9u == cfg.panel_events[2].key_index
		&& 1000u == cfg.panel_events[2].at_ms
		&& 50u == cfg.panel_events[2].hold_ms
		/* --switch D0=1 */
		&& PANEL_EVENT_SWITCH == cfg.panel_events[3].kind
		&& 0u == cfg.panel_events[3].key_index
		&& true == cfg.panel_events[3].value
		&& 0u == cfg.panel_events[3].at_ms
		/* --switch D7=0@2000 */
		&& PANEL_EVENT_SWITCH == cfg.panel_events[4].kind
		&& 7u == cfg.panel_events[4].key_index
		&& false == cfg.panel_events[4].value
		&& 2000u == cfg.panel_events[4].at_ms
	);

	return NULL;
}

static char *test_parse_args_rejects_bad_panel_spec(void)
{
	struct Config cfg;
	char *argv_unknown_key[]  = { "prog", "--press", "FOO", "rom.bin", NULL };
	char *argv_bad_ms[]       = { "prog", "--press", "D0@xx", "rom.bin", NULL };
	char *argv_sw_no_eq[]     = { "prog", "--switch", "D0", "rom.bin", NULL };
	char *argv_sw_bad_val[]   = { "prog", "--switch", "D0=2", "rom.bin", NULL };
	char *argv_sw_non_data[]  = { "prog", "--switch", "RUN=1", "rom.bin", NULL };

	reset_getopt();
	_it_should("reject --press with unknown key name",
		-2 == cli_parse_args(4, argv_unknown_key, &cfg));

	reset_getopt();
	_it_should("reject --press with non-numeric ms",
		-2 == cli_parse_args(4, argv_bad_ms, &cfg));

	reset_getopt();
	_it_should("reject --switch without '='",
		-2 == cli_parse_args(4, argv_sw_no_eq, &cfg));

	reset_getopt();
	_it_should("reject --switch with value outside {0,1}",
		-2 == cli_parse_args(4, argv_sw_bad_val, &cfg));

	reset_getopt();
	_it_should("reject --switch on non-data key (RUN/MODE/NEXT don't latch)",
		-2 == cli_parse_args(4, argv_sw_non_data, &cfg));

	return NULL;
}

static char *test_parse_args_help_version_without_rom(void)
{
	struct Config cfg;
	char *argv_help[] = { "prog", "--help", NULL };
	char *argv_ver[] = { "prog", "--version", NULL };

	reset_getopt();
	_it_should(
		"--help parses without ROM positional arg",
		0 == cli_parse_args(2, argv_help, &cfg)
		&& true == cfg.show_help
		&& false == cfg.show_version
		&& NULL == cfg.rom_path
	);

	reset_getopt();
	_it_should(
		"--version parses without ROM positional arg",
		0 == cli_parse_args(2, argv_ver, &cfg)
		&& true == cfg.show_version
		&& false == cfg.show_help
		&& NULL == cfg.rom_path
	);

	return NULL;
}

static char *test_parse_args_rejects_extra_arg(void)
{
	struct Config cfg;
	char *argv[] = { "prog", "rom.bin", "extra.bin", NULL };
	int argc = 3;
	int pipefd[2];
	int saved;
	char buf[128];
	ssize_t n;

	reset_getopt();

	if (pipe(pipefd) != 0)
		return "pipe() failed";

	saved = dup(STDERR_FILENO);
	if (saved < 0)
		return "dup() failed";

	if (dup2(pipefd[1], STDERR_FILENO) < 0)
		return "dup2() failed";
	close(pipefd[1]);

	_it_should(
		"reject extra positional args",
		-2 == cli_parse_args(argc, argv, &cfg)
	);

	fflush(stderr);
	n = read(pipefd[0], buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';

	dup2(saved, STDERR_FILENO);
	close(saved);
	close(pipefd[0]);

	_it_should(
		"emit extra arg warning",
		strstr(buf, "Unexpected extra argument") != NULL
	);

	return NULL;
}

static char *test_parse_args_rejects_bad_values(void)
{
	struct Config cfg;
	char *argv_bad_mode[] = { "prog", "--panel-mode", "nope", "rom.bin", NULL };
	char *argv_bad_flush[] = { "prog", "--log-flush", "2", "rom.bin", NULL };

	reset_getopt();
	_it_should(
		"reject bad panel mode",
		-2 == cli_parse_args(4, argv_bad_mode, &cfg)
	);

	reset_getopt();
	_it_should(
		"reject bad log flush value",
		-2 == cli_parse_args(4, argv_bad_flush, &cfg)
	);

	return NULL;
}

static char *test_usage_emits_usage(void)
{
	int pipefd[2];
	int saved;
	char buf[256];
	ssize_t n;

	if (pipe(pipefd) != 0)
		return "pipe() failed";

	saved = dup(STDERR_FILENO);
	if (saved < 0)
		return "dup() failed";

	if (dup2(pipefd[1], STDERR_FILENO) < 0)
		return "dup2() failed";
	close(pipefd[1]);

	cli_usage("prog");

	fflush(stderr);
	n = read(pipefd[0], buf, sizeof(buf) - 1);
	if (n < 0)
		n = 0;
	buf[n] = '\0';

	dup2(saved, STDERR_FILENO);
	close(saved);
	close(pipefd[0]);

	_it_should(
		"cli_usage writes usage text",
		n > 0 && strstr(buf, "Usage:") != NULL
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_cfg_defaults);
	_run_test(test_parse_u32);
	_run_test(test_parse_i32);
	_run_test(test_parse_args_rejects_null_cfg);
	_run_test(test_parse_args_basic);
	_run_test(test_parse_args_flags);
	_run_test(test_parse_args_more_flags);
	_run_test(test_parse_args_panel_compact_precedence);
	_run_test(test_parse_args_realtime_precedence);
	_run_test(test_parse_args_sets_cassette_and_persistence_specs);
	_run_test(test_parse_args_rejects_bad_spec);
	_run_test(test_parse_args_serial_in);
	_run_test(test_parse_args_defaults_serial_in_unset);
	_run_test(test_parse_args_press_and_switch);
	_run_test(test_parse_args_rejects_bad_panel_spec);
	_run_test(test_parse_args_help_version_without_rom);
	_run_test(test_parse_args_rejects_extra_arg);
	_run_test(test_parse_args_rejects_bad_values);
	_run_test(test_usage_emits_usage);

	return NULL;
}
