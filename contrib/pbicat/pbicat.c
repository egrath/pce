/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   pbicat.c                                                     *
 * Created:     2018-02-19 by Hampa Hug <hampa@hampa.ch>                     *
 * Copyright:   (C) 2018-2022 Hampa Hug <hampa@hampa.ch>                     *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General *
 * Public License for more details.                                          *
 *****************************************************************************/


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define PBI_MAGIC_PBI  0x50424920
#define PBI_MAGIC_PBIN 0x5042496e

#define PBI_UNIFORM 2


static const char *arg0 = NULL;

static char     par_force = 0;
static unsigned par_block_bits = 12;
static uint64_t par_max_size = 0;


typedef struct {
	FILE          *inp;
	FILE          *out;

	uint64_t      image_size;
	uint64_t      l1_offset;
	uint64_t      ofs;

	unsigned char l1bits;
	unsigned char l2bits;
	unsigned char blbits;

	unsigned long l1size;
	unsigned long l2size;
	unsigned long blsize;

	unsigned char *header;
	unsigned char *l1;
	unsigned char *l2;
	unsigned char *bl;
} pbi_cat_t;


static
void print_help (void)
{
	fputs (
		"pbicat: Convert raw disk images to PBI\n"
		"\n"
		"usage: pbicat [options] [filename]\n"
		"  -b bits   Set the number of block bits [12]\n"
		"  -f        Force writing output to a tty [no]\n"
		"  -h        Print this help\n"
		"  -i fname  Convert fname to PBI\n"
		"  -m size   Set the maximum image size [auto]\n",
		stdout
	);

	fflush (stdout);
}

static
void print_version (void)
{
	fputs (
		"pbicat version 2022-01-20\n\n"
		"Copyright (C) 2018-2022 Hampa Hug <hampa@hampa.ch>\n",
		stdout
	);

	fflush (stdout);
}

static
uint32_t get_uint32_be (const void *buf, unsigned i)
{
	const unsigned char *tmp;
	uint32_t            v;

	tmp = (const unsigned char *) buf + i;

	v = tmp[0];
	v = (v << 8) | tmp[1];
	v = (v << 8) | tmp[2];
	v = (v << 8) | tmp[3];

	return (v);
}

static
void set_uint16_be (void *buf, unsigned i, uint16_t v)
{
	unsigned char *tmp;

	tmp = (unsigned char *) buf + i;

	tmp[0] = (v >> 8) & 0xff;
	tmp[1] = v & 0xff;
}

static
void set_uint32_be (void *buf, unsigned i, uint32_t v)
{
	unsigned char *tmp;

	tmp = (unsigned char *) buf + i;

	tmp[0] = (v >> 24) & 0xff;
	tmp[1] = (v >> 16) & 0xff;
	tmp[2] = (v >> 8) & 0xff;
	tmp[3] = v & 0xff;
}

static
void set_uint64_be (void *buf, unsigned i, uint64_t v)
{
	unsigned char *tmp;

	tmp = (unsigned char *) buf + i;

	tmp[0] = (v >> 56) & 0xff;
	tmp[1] = (v >> 48) & 0xff;
	tmp[2] = (v >> 40) & 0xff;
	tmp[3] = (v >> 32) & 0xff;
	tmp[4] = (v >> 24) & 0xff;
	tmp[5] = (v >> 16) & 0xff;
	tmp[6] = (v >> 8) & 0xff;
	tmp[7] = v & 0xff;
}

static
int get_filesize (FILE *fp, uint64_t *size)
{
	off_t val;

	if (fseeko (fp, 0, SEEK_END)) {
		return (1);
	}

	val = ftello (fp);

	if (val == (off_t) -1) {
		return (1);
	}

	*size = val;

	return (0);
}

static
int pbi_cat_set_pos (FILE *fp, uint64_t pos)
{
	if (fseeko (fp, pos, SEEK_SET)) {
		return (1);
	}

	return (0);
}

static
unsigned long pbi_cat_read (FILE *fp, void *buf, unsigned long cnt)
{
	return (fread (buf, 1, cnt, fp));
}

static
int pbi_cat_write (FILE *fp, const void *buf, unsigned long cnt)
{
	if (fwrite (buf, 1, cnt, fp) != cnt) {
		return (1);
	}

	return (0);
}

static
int pbi_cat_is_zero (const void *buf, unsigned long cnt)
{
	unsigned long       i, n;
	const unsigned long *p;

	p = buf;

	n = cnt / sizeof (unsigned long);

	for (i = 0; i < n; i++) {
		if (*(p++) != 0) {
			return (0);
		}
	}

	return (1);
}

static
int pbi_cat_is_uniform (const void *buf, unsigned long cnt, unsigned long *val)
{
	unsigned long       i;
	const unsigned char *p;

	p = buf;

	for (i = 4; i < cnt; i++) {
		if (p[i] != p[i & 3]) {
			return (0);
		}
	}

	*val = get_uint32_be (buf, 0);

	return (1);
}

static
int pbi_cat_alloc (pbi_cat_t *cat)
{
	if ((cat->header = malloc (cat->blsize)) == NULL) {
		return (1);
	}

	if ((cat->l1 = malloc (cat->l1size)) == NULL) {
		return (1);
	}

	if ((cat->l2 = malloc (cat->l2size)) == NULL) {
		return (1);
	}

	if ((cat->bl = malloc (cat->blsize)) == NULL) {
		return (1);
	}

	memset (cat->header, 0, cat->blsize);
	memset (cat->l1, 0, cat->l1size);
	memset (cat->l2, 0, cat->l2size);
	memset (cat->bl, 0, cat->blsize);

	return (0);
}

static
int pbi_cat_header1 (pbi_cat_t *cat)
{
	unsigned char *p;

	p = cat->header;

	set_uint32_be (p, 0, PBI_MAGIC_PBIN);
	set_uint32_be (p, 4, 0);
	set_uint32_be (p, 8, 48);

	p[12] = cat->l1bits;
	p[13] = cat->l2bits;
	p[14] = cat->blbits;

	set_uint64_be (p, 16, cat->image_size);

	if (pbi_cat_write (cat->out, p, cat->blsize)) {
		return (1);
	}

	cat->ofs = cat->blsize;

	return (0);
}

static
int pbi_cat_header2 (pbi_cat_t *cat)
{
	unsigned char *p;

	p = cat->header;

	set_uint32_be (p, 0, PBI_MAGIC_PBI);
	set_uint32_be (p, 4, 0);
	set_uint32_be (p, 8, 48);

	p[12] = cat->l1bits;
	p[13] = cat->l2bits;
	p[14] = cat->blbits;

	set_uint64_be (p, 16, cat->image_size);
	set_uint64_be (p, 24, cat->l1_offset);
	set_uint64_be (p, 32, cat->ofs);

	set_uint32_be (p, 40, 0);
	set_uint16_be (p, 44, 0);
	set_uint16_be (p, 46, 0);

	if (pbi_cat_write (cat->out, p, cat->blsize)) {
		return (1);
	}

	cat->ofs = cat->blsize;

	return (0);
}

static
int pbi_cat_data (pbi_cat_t *cat)
{
	unsigned long cnt, val;
	int           l2mod;
	unsigned long l1ent, l2ent, l1idx, l2idx;
	unsigned long last_r, last_w, next_r, next_w;
	uint64_t      l2val;

	if (pbi_cat_set_pos (cat->inp, 0)) {
		return (1);
	}

	l2mod = 0;

	l1ent = 1UL << cat->l1bits;
	l2ent = 1UL << cat->l2bits;

	l1idx = 0;
	l2idx = 0;

	cat->image_size = 0;

	last_r = -1;
	last_w = -1;
	next_r = 0;
	next_w = 0;

	while (1) {
		cnt = pbi_cat_read (cat->inp, cat->bl, cat->blsize);

		if (cnt == 0) {
			break;
		}

		if (l1idx >= l1ent) {
			return (1);
		}

		cat->image_size += cnt;

		if (cnt < cat->blsize) {
			memset (cat->bl + cnt, 0, cat->blsize - cnt);
		}

		if (pbi_cat_is_zero (cat->bl, cat->blsize)) {
			;
		}
		else if (pbi_cat_is_uniform (cat->bl, cat->blsize, &val)) {
			l2val = ((uint64_t) val << 32) | PBI_UNIFORM;
			set_uint64_be (cat->l2, l2idx << 3, l2val);
			l2mod = 1;
		}
		else {
			if (pbi_cat_write (cat->out, cat->bl, cat->blsize)) {
				return (1);
			}

			set_uint64_be (cat->l2, l2idx << 3, cat->ofs);
			l2mod = 1;

			cat->ofs += cat->blsize;
		}

		l2idx += 1;

		if (l2idx >= l2ent) {
			if (l2mod) {
				set_uint64_be (cat->l1, l1idx << 3, cat->ofs);

				if (pbi_cat_write (cat->out, cat->l2, cat->l2size)) {
					return (1);
				}

				cat->ofs += cat->l2size;

				memset (cat->l2, 0, cat->l2size);
				l2mod = 0;
			}

			l2idx = 0;
			l1idx += 1;

		}

		next_r = cat->image_size >> 20;
		next_w = cat->ofs >> 20;

		if ((next_r != last_r) || (next_w != last_w)) {
			fprintf (stderr, "\tR=%lu MiB   W=%lu MiB\r",
				next_r, next_w
			);

			fflush (stderr);

			last_r = next_r;
			last_w = next_w;
		}

		if (cnt < cat->blsize) {
			break;
		}
	}

	if (l2mod) {
		set_uint64_be (cat->l1, l1idx << 3, cat->ofs);

		if (pbi_cat_write (cat->out, cat->l2, cat->l2size)) {
			return (1);
		}

		cat->ofs += cat->l2size;

		memset (cat->l2, 0, cat->l2size);
		l2mod = 0;
	}

	cat->l1_offset = cat->ofs;

	set_uint64_be (cat->header, 24, cat->ofs);

	if (pbi_cat_write (cat->out, cat->l1, cat->l1size)) {
		return (1);
	}

	cat->ofs += cat->l2size;

	fprintf (stderr, "\tR=%llu MiB   W=%llu MiB\n",
		(unsigned long long) (cat->image_size >> 20),
		(unsigned long long) (cat->ofs >> 20)
	);

	return (0);
}

static
int pbi_cat (pbi_cat_t *cat, const char *name)
{
	uint64_t max;

	if (par_max_size > 0) {
		cat->image_size = par_max_size;
	}
	else if (get_filesize (cat->inp, &cat->image_size)) {
		cat->image_size = 1ULL << 40;
	}

	cat->blbits = (par_block_bits < 12) ? 12 : par_block_bits;
	cat->l1bits = cat->blbits - 3;
	cat->l2bits = cat->blbits - 3;
	max = 1ULL << (cat->l1bits + cat->l2bits + cat->blbits);

	while (cat->image_size > max) {
		if (cat->l1bits < 17) {
			cat->l1bits += 1;
		}
		else {
			cat->blbits += 1;
		}

		if (cat->l1bits < (cat->blbits - 3)) {
			cat->l1bits = cat->blbits - 3;
		}

		if (cat->l2bits < (cat->blbits - 3)) {
			cat->l2bits = cat->blbits - 3;
		}

		max = 1ULL << (cat->l1bits + cat->l2bits + cat->blbits);
	}

	cat->l1size = 1UL << (cat->l1bits + 3);
	cat->l2size = 1UL << (cat->l2bits + 3);
	cat->blsize = 1UL << cat->blbits;

	fprintf (stderr, "%s\n", name);
	fprintf (stderr, "\tsize:  %llu MiB (%llu)\n",
		(unsigned long long) (cat->image_size >> 20),
		(unsigned long long) cat->image_size
	);
	fprintf (stderr, "\tmax:   %llu MiB (%llu)\n",
		(unsigned long long) (max >> 20),
		(unsigned long long) max
	);
	fprintf (stderr, "\tblock: %lu\n", cat->blsize);
	fprintf (stderr, "\tbits:  %u:%u:%u (%lu:%lu:%lu)\n",
		cat->l1bits, cat->l2bits, cat->blbits,
		cat->l1size, cat->l2size, cat->blsize
	);

	if (pbi_cat_alloc (cat)) {
		fprintf (stderr, "%s: alloc failed\n", arg0);
		return (1);
	}

	if (pbi_cat_header1 (cat)) {
		return (1);
	}

	if (pbi_cat_data (cat)) {
		return (1);
	}

	if (pbi_cat_header2 (cat)) {
		return (1);
	}

	return (0);
}

static
int pbi_cat_name (const char *name)
{
	int       r;
	pbi_cat_t cat;

	if (par_force == 0) {
		if (isatty (fileno (stdout))) {
			fprintf (stderr, "%s: not writing to tty\n", arg0);
			return (1);
		}
	}

	memset (&cat, 0, sizeof (cat));

	if ((cat.inp = fopen (name, "rb")) == NULL) {
		fprintf (stderr, "%s: can't open file (%s)\n", arg0, name);
		return (1);
	}

	cat.out = stdout;

	r = pbi_cat (&cat, name);

	fclose (cat.inp);

	return (r);
}

static
int parse_args (int argc, char **argv, const char *str, int *i)
{
	char *end;

	while (*str != 0) {
		if (*str == 'b') {
			*i += 1;
			if (*i >= argc) {
				fprintf (stderr, "%s: missing block bits\n", arg0);
				return (1);
			}

			par_block_bits = strtoul (argv[*i], &end, 0);

			if (*end != 0) {
				fprintf (stderr, "%s: bad block bits (%s)\n",
					arg0, argv[*i]
				);
				return (1);
			}
		}
		else if (*str == 'f') {
			par_force = 1;
		}
		else if (*str == 'h') {
			print_help();
		}
		else if (*str == 'i') {
			*i += 1;
			if (*i >= argc) {
				fprintf (stderr, "%s: missing file name\n", arg0);
				return (1);
			}

			if (pbi_cat_name (argv[*i])) {
				return (1);
			}
		}
		else if (*str == 'm') {
			*i += 1;
			if (*i >= argc) {
				fprintf (stderr, "%s: missing max size\n",
					arg0
				);
				return (1);
			}

			par_max_size = strtoull (argv[*i], &end, 0);

			if (*end == 0) {
				;
			}
			else if ((*end == 'k') || (*end == 'K')) {
				par_max_size *= 1024ULL;
			}
			else if ((*end == 'm') || (*end == 'M')) {
				par_max_size *= 1024ULL * 1024ULL;
			}
			else if ((*end == 'g') || (*end == 'G')) {
				par_max_size *= 1024ULL * 1024ULL * 1024ULL;
			}
			else if ((*end == 't') || (*end == 'T')) {
				par_max_size *= 1024ULL * 1024ULL * 1024ULL * 1024ULL;
			}
			else {
				fprintf (stderr, "%s: bad max size (%s)\n",
					arg0, argv[*i]
				);
				return (1);
			}
		}
		else {
			fprintf (stderr, "%s: unknown option (%s)\n",
				arg0, str
			);
			return (1);
		}

		str += 1;
	}

	return (0);
}

int main (int argc, char **argv)
{
	int i;

	arg0 = argv[0];

	if (argc >= 2) {
		if (strcmp (argv[1], "--help") == 0) {
			print_help();
			return (0);
		}
		else if (strcmp (argv[1], "--version") == 0) {
			print_version();
			return (0);
		}
	}

	i = 1;

	while (i < argc) {
		if (argv[i][0] == '-') {
			if (parse_args (argc, argv, argv[i] + 1, &i)) {
				return (1);
			}
		}
		else if (pbi_cat_name (argv[i])) {
			return (1);
		}

		i += 1;
	}

	return (0);
}
