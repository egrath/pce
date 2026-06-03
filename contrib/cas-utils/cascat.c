/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   cas-utils/cascat.c                                           *
 * Created:     2009-02-18 by Hampa Hug <hampa@hampa.ch>                     *
 * Copyright:   (C) 2008-2020 Hampa Hug <hampa@hampa.ch>                     *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cassette.h"
#include "util.h"


static int           par_raw = 0;
static int           par_verbose = 0;
static int           par_pcm_input = 0;
static int           par_drop = 0;
static unsigned long par_delay = 1000;


static
void print_help (void)
{
	fputs (
		"cascat: concatenate cassette files\n"
		"\n"
		"usage: cascat [options] [file...]\n"
		"  -b, --drop-bad-blocks  Drop blocks with bad CRC [no]\n"
		"  -d, --motor-delay int  Set the motor delay in milliseconds [1000]\n"
		"  -i, --input string     Set an input file\n"
		"  -p, --pcm-input        Read a pcm file [no]\n"
		"  -P, --pcm-output       Write a pcm file [no]\n"
		"  -r, --raw              Do a simple bit copy [no]\n"
		"  -v, --verbose          Verbose operation [no]\n",
		stdout
	);

	fflush (stdout);
}

static
void print_version (void)
{
	fputs (
		"cascat version 0.1.1\n\n"
		"Copyright (C) 2008-2020 Hampa Hug <hampa@hampa.ch>\n",
		stdout
	);

	fflush (stdout);
}

static
int cas_cat_raw (cas_file_t *src, cas_file_t *out)
{
	int           val;
	unsigned long cnt;

	cnt = 0;

	while (cas_read_bit (src, &val) == 0) {
		if (cas_write_bit (out, val)) {
			return (1);
		}

		cnt += 1;
	}

	if (par_verbose) {
		fprintf (stderr, "%lu bits copied\n", cnt);
	}

	return (0);
}

static
int cas_cat_ibm (cas_file_t *src, cas_file_t *out)
{
	unsigned      i;
	int           ok, first;
	unsigned long lpos, spos;
	unsigned char buf[256];

	spos = src->ofs;

	first = 1;

	while (cas_read_leader (src, &lpos) == 0) {
		if (par_verbose) {
			if (first == 0) {
				fputs ("\n", stderr);
			}

			fprintf (stderr, "%lu\tleader (%lu)\n",
				out->ofs, src->ofs - spos
			);
		}

		if (cas_write_leader (out, 256, par_delay)) {
			return (1);
		}

		i = 0;

		if (par_verbose) {
			fprintf (stderr, "%lu\tblock %u start\n",
				src->ofs, i
			);
		}

		while (cas_read_block (src, buf, 64, &ok) == 0) {
			if (par_verbose || (ok == 0)) {
				fprintf (stderr, "%lu\tblock %u%s\n",
					src->ofs, i,
					ok ? "" : " (BAD CRC)"
				);
			}

			if ((par_drop == 0) || ok) {
				if (cas_write_block (out, buf, 256)) {
					return (1);
				}
			}

			i += 1;

			if (par_verbose) {
				fprintf (stderr, "%lu\tblock %u start\n",
					src->ofs, i
				);
			}
		}

		if (cas_write_trailer (out)) {
			return (1);
		}

		spos = src->ofs;

		first = 0;
	}

	return (0);
}

static
int cas_cat_fp (FILE *inp, cas_file_t *out)
{
	int        r;
	cas_file_t src;

	cas_open_fp (&src, inp);

	if (par_pcm_input) {
		cas_set_pcm (&src, 1);
	}

	if (par_raw) {
		r = cas_cat_raw (&src, out);
	}
	else {
		r = cas_cat_ibm (&src, out);
	}

	return (r);
}

static
int cas_cat (const char *inp, cas_file_t *out)
{
	int  r;
	FILE *fp;

	if ((fp = fopen (inp, "rb")) == NULL) {
		return (1);
	}

	if (par_verbose) {
		fprintf (stderr, "%s:\n", inp);
	}

	r = cas_cat_fp (fp, out);

	fclose (fp);

	return (r);
}

int main (int argc, char **argv)
{
	int        i;
	int        done;
	cas_file_t out;

	if (argc == 2) {
		if (str_isarg (argv[1], "h", "help")) {
			print_help();
			return (0);
		}
		else if (str_isarg (argv[1], "V", "version")) {
			print_version();
			return (0);
		}
	}

	done = 0;

	cas_open_fp (&out, stdout);

	i = 1;
	while (i < argc) {
		if (str_isarg (argv[i], "b", "drop-bad-blocks")) {
			par_drop = 1;
		}
		else if (str_isarg (argv[i], "d", "motor-delay")) {
			if (++i >= argc) {
				return (1);
			}

			par_delay = strtoul (argv[i], NULL, 0);
		}
		else if (str_isarg (argv[i], "i", "input")) {
			if (++i >= argc) {
				return (1);
			}

			if (cas_cat (argv[i], &out)) {
				return (1);
			}

			done = 1;
		}
		else if (str_isarg (argv[i], "p", "pcm")) {
			par_pcm_input = 1;
		}
		else if (str_isarg (argv[i], "P", "pcm-output")) {
			cas_set_pcm (&out, 1);
		}
		else if (str_isarg (argv[i], "r", "raw")) {
			par_raw = 1;
		}
		else if (str_isarg (argv[i], "v", "verbose")) {
			par_verbose = 1;
		}
		else if (argv[i][0] == '-') {
			fprintf (stderr, "%s: unknown option (%s)\n",
				argv[0], argv[i]
			);
			return (1);
		}
		else {
			if (cas_cat (argv[i], &out)) {
				return (1);
			}

			done = 1;
		}

		i += 1;
	}

	if (done == 0) {
		if (cas_cat_fp (stdin, &out)) {
			return (1);
		}
	}

	if (par_raw == 0) {
		cas_write_silence (&out, par_delay);
	}

	fflush (stdout);

	return (0);
}
