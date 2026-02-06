/*
 * cli.spec.c
 *
 * Unit tests for CLI parsing helpers.
 */

#include "cli.c"

#include "test-runner.h"

#include <string.h>
#include <unistd.h>

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
		&& 50u == cfg.hold_ms
		&& true == cfg.realtime
		&& true == cfg.log_flush
		&& PANEL_TEXT_MODE_BURST == cfg.panel_text_mode
		&& 0 == strcmp(cfg.state_file, "altaid.state")
		&& 0 == strcmp(cfg.ram_file, "altaid.ram")
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
	);

	_it_should(
		"parse_u32 accepts valid input",
		0 == parse_u32("123", &out)
		&& 123u == out
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
	);

	_it_should(
		"parse_i32 accepts valid input",
		0 == parse_i32("-12", &out)
		&& -12 == out
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
	int argc = 9;

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
	_run_test(test_parse_args_basic);
	_run_test(test_parse_args_flags);
	_run_test(test_parse_args_rejects_extra_arg);
	_run_test(test_parse_args_rejects_bad_values);
	_run_test(test_usage_emits_usage);

	return NULL;
}
