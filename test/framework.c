/*
 * Copyright (C) 2010 Joel Rosdahl
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ccache.h"
#include "framework.h"
#include <stdio.h>
#if defined(HAVE_TERMIOS_H)
#define USE_COLOR
#include <termios.h>
#endif

static unsigned passed_asserts;
static unsigned failed_asserts;
static unsigned passed_tests;
static unsigned failed_tests;
static unsigned passed_suites;
static unsigned failed_suites;
static unsigned failed_asserts_before_suite;
static unsigned failed_asserts_before_test;
static const char *current_suite;
static const char *current_test;
static int verbose;

static const char COLOR_END[] = "\x1b[m";
static const char COLOR_GREEN[] = "\x1b[32m";
static const char COLOR_RED[] = "\x1b[31m";

#define COLOR(tty, color) ((tty) ? COLOR_##color : "")

static int
is_tty(int fd)
{
#ifdef USE_COLOR
	struct termios t;
	return tcgetattr(fd, &t) == 0;
#else
	(void)fd;
	return 0;
#endif
}

static const char *
numerus(unsigned n)
{
	return n == 1 ? "" : "s";
}

int
cct_run(suite_fn *suites, int verbose_output)
{
	suite_fn *suite;
	int tty = is_tty(1);

	verbose = verbose_output;

	for (suite = suites; *suite; suite++) {
		unsigned test_index = 0;
		while (1) {
			test_index = (*suite)(test_index + 1);
			if (test_index == 0) {
				break;
			}
			++failed_suites;
		}
		--passed_tests; /* Fix false increase in first TEST expansion. */
	}

	if (failed_asserts == 0) {
		printf("%sPASSED%s: %u assertion%s, %u test%s, %u suite%s\n",
		       COLOR(tty, GREEN), COLOR(tty, END),
		       passed_asserts,
		       numerus(passed_asserts),
		       passed_tests,
		       numerus(passed_tests),
		       passed_suites,
		       numerus(passed_suites));
	} else {
		printf("%sFAILED%s: %u assertion%s, %u test%s, %u suite%s\n",
		       COLOR(tty, RED), COLOR(tty, END),
		       failed_asserts,
		       numerus(failed_asserts),
		       failed_tests,
		       numerus(failed_tests),
		       failed_suites,
		       numerus(failed_suites));
	}
	return failed_asserts > 0 ? 1 : 0;
}

void cct_suite_begin(const char *name)
{
	if (verbose) {
		printf("Running suite %s...\n", name);
	}
	current_suite = name;
	failed_asserts_before_suite = failed_asserts;
	failed_asserts_before_test = failed_tests; /* For first cct_test_end(). */
}

void cct_suite_end()
{
	if (failed_asserts > failed_asserts_before_suite) {
		++failed_suites;
	} else {
		++passed_suites;
	}
}

void cct_test_begin(const char *name)
{
	if (verbose) {
		printf("Running test %s...\n", name);
	}
	current_test = name;
	failed_asserts_before_test = failed_asserts;
}

void cct_test_end()
{
	if (failed_asserts > failed_asserts_before_test) {
		++failed_tests;
	} else {
		++passed_tests;
	}
}

void
cct_check_passed(void)
{
	++passed_asserts;
}

void
cct_check_failed(const char *file, int line, const char *what,
                 const char *expected, const char *actual)
{
	++failed_asserts;
	fprintf(stderr, "%s:%u: Failed assertion:\n", file, line);
	fprintf(stderr, "  Suite:      %s\n", current_suite);
	fprintf(stderr, "  Test:       %s\n", current_test);
	if (expected && actual) {
		fprintf(stderr, "  Expression: %s\n", what);
		fprintf(stderr, "  Expected:   %s\n", expected);
		fprintf(stderr, "  Actual:     %s\n", actual);
	} else {
		fprintf(stderr, "  Assertion:  %s\n", what);
	}
	fprintf(stderr, "\n");
}

int
cct_check_int_eq(const char *file, int line, const char *expression,
                 int expected, int actual)
{
	if (expected == actual) {
		cct_check_passed();
		return 1;
	} else {
		char *exp_str, *act_str;
		x_asprintf(&exp_str, "%i", expected);
		x_asprintf(&act_str, "%i", actual);
		cct_check_failed(file, line, expression, exp_str, act_str);
		free(exp_str);
		free(act_str);
		return 0;
	}
}

int
cct_check_uns_eq(const char *file, int line, const char *expression,
                 unsigned expected, unsigned actual)
{
	if (expected == actual) {
		cct_check_passed();
		return 1;
	} else {
		char *exp_str, *act_str;
		x_asprintf(&exp_str, "%i", expected);
		x_asprintf(&act_str, "%i", actual);
		cct_check_failed(file, line, expression, exp_str, act_str);
		free(exp_str);
		free(act_str);
		return 0;
	}
}

int
cct_check_str_eq(const char *file, int line, const char *expression,
                 char *expected, char *actual, int free1, int free2)
{
	int result;

	if (expected && actual && strcmp(actual, expected) == 0) {
		cct_check_passed();
		result = 1;
	} else {
		char *exp_str, *act_str;
		if (expected) {
			x_asprintf(&exp_str, "\"%s\"", expected);
		} else {
			exp_str = x_strdup("(null)");
		}
		if (actual) {
			x_asprintf(&act_str, "\"%s\"", actual);
		} else {
			act_str = x_strdup("(null)");
		}
		cct_check_failed(file, line, expression, exp_str, act_str);
		free(exp_str);
		free(act_str);
		result = 0;
	}

	if (free1) {
		free(expected);
	}
	if (free2) {
		free(actual);
	}
	return result;
}

int
cct_check_args_eq(const char *file, int line, const char *expression,
                  struct args *expected, struct args *actual,
                  int free1, int free2)
{
	int result;

	if (expected && actual && args_equal(actual, expected)) {
		cct_check_passed();
		result = 1;
	} else {
		char *exp_str, *act_str;
		exp_str = expected ? args_to_string(expected) : x_strdup("(null)");
		act_str = actual ? args_to_string(actual) : x_strdup("(null)");
		cct_check_failed(file, line, expression, exp_str, act_str);
		free(exp_str);
		free(act_str);
		result = 0;
	}

	if (free1) {
		args_free(expected);
	}
	if (free2) {
		args_free(actual);
	}
	return result;
}