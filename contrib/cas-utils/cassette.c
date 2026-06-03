/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   cas-utils/cassette.c                                         *
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


#include <stdio.h>
#include <string.h>

#include "cassette.h"


unsigned cas_crc (unsigned crc, const void *buf, unsigned cnt)
{
	unsigned            i;
	const unsigned char *p;

	p = buf;

	crc ^= 0xffff;

	while (cnt-- > 0) {
		crc ^= *(p++) << 8;

		for (i = 0; i < 8; i++) {
			if (crc & 0x8000) {
				crc = (crc << 1) ^ 0x1021;
			}
			else {
				crc = crc << 1;
			}
		}
	}

	crc ^= 0xffff;

	return (crc & 0xffff);
}

int cas_open_fp (cas_file_t *cf, FILE *fp)
{
	cf->fp = fp;

	cf->pcm = 0;
	cf->txt = 0;

	cf->need_header = 1;

	cf->clock = 1193182;
	cf->srate = 44100;
	cf->acc = 0;

	cf->buf = 0;
	cf->cnt = 0;

	cf->ofs = 0;

	return (0);
}

void cas_set_pcm (cas_file_t *cf, int pcm)
{
	cf->pcm = (pcm != 0);
	cf->acc = 0;
}

void cas_set_txt (cas_file_t *cf, int val)
{
	cf->txt = (val != 0);
}

void cas_set_srate (cas_file_t *cf, unsigned long srate)
{
	cf->srate = srate;
	cf->acc = 0;
}

static
int cas_read_bit_cas (cas_file_t *cf, int *val)
{
	int c;

	if (cf->cnt == 0) {
		if ((c = fgetc (cf->fp)) == EOF) {
			return (1);
		}

		cf->ofs += 1;

		cf->buf = c & 0xff;
		cf->cnt = 8;
	}

	*val = ((cf->buf & 0x80) != 0);

	cf->buf = (cf->buf << 1) & 0xff;
	cf->cnt -= 1;

	return (0);
}

static
int cas_read_bit_pcm (cas_file_t *cf, int *val)
{
	int           smp, old;
	unsigned long cnt, cmp;

	old = -1;
	cnt = 0;

	cmp = (3 * cf->srate) / 4000;

	while ((smp = fgetc (cf->fp)) != EOF) {
		cf->ofs += 1;
		cnt += 1;

		if (smp & 0x80) {
			smp -= 256;
		}

		smp = (3 * old + smp) / 4;

		if ((old >= 0) && (smp < 0)) {
			if (cnt < cmp) {
				*val = 0;
			}
			else {
				*val = 1;
			}

			return (0);
		}

		old = smp;
	}

	return (1);
}

int cas_read_bit (cas_file_t *cf, int *val)
{
	if (cf->pcm) {
		return (cas_read_bit_pcm (cf, val));
	}
	else {
		return (cas_read_bit_cas (cf, val));
	}
}

int cas_read_byte (cas_file_t *cf, unsigned char *val)
{
	unsigned i;
	int      bit;

	*val = 0;

	for (i = 0; i < 8; i++) {
		if (cas_read_bit (cf, &bit)) {
			return (1);
		}

		*val = (*val << 1) | (bit != 0);
	}

	return (0);
}

int cas_read_leader (cas_file_t *cf, unsigned long *start)
{
	unsigned      bitcnt;
	int           bit;
	unsigned char byte;

	*start = cf->ofs;

	bitcnt = 0;

	while (1) {
		if (cas_read_bit (cf, &bit)) {
			return (1);
		}

		if (bit) {
			bitcnt += 1;

			if (bitcnt == 0) {
				bitcnt -= 1;
			}
		}
		else {
			if (bitcnt < (64 * 8)) {
				bitcnt = 0;
			}
			else {
				if (cas_read_byte (cf, &byte)) {
					return (1);
				}

				if (byte == 0x16) {
					return (0);
				}

				bitcnt = 0;
			}

			*start = cf->ofs;
		}
	}

	return (1);
}

int cas_read_block (cas_file_t *cf, void *buf, unsigned maxff, int *ok)
{
	unsigned      i, n;
	unsigned      crc1, crc2;
	unsigned char val[2];
	unsigned long pos;
	unsigned char *blk;

	*ok = 0;

	blk = buf;

	pos = cf->ofs;

	for (i = 0; i < 256; i++) {
		if (cas_read_byte (cf, blk + i)) {
			return (1);
		}
	}

	if (cas_read_byte (cf, &val[0])) {
		return (1);
	}

	if (cas_read_byte (cf, &val[1])) {
		return (1);
	}

	crc1 = (val[0] << 8) | val[1];
	crc2 = cas_crc (0, blk, 256);

	if (crc1 != crc2) {
		n = 0;
		for (i = 0; i < 256; i++) {
			if (blk[i] == 0xff) {
				n += 1;
			}
		}
		if (n < maxff) {
			return (0);
		}
		if (fseek (cf->fp, pos, SEEK_SET) == 0) {
			cf->ofs = pos;
		}
		return (1);
	}

	*ok = 1;

	return (0);
}


int cas_flush_out (cas_file_t *cf)
{
	if (cf->pcm || cf->txt) {
		return (0);
	}

	if (cf->cnt > 0) {
		fputc ((cf->buf << (8 - cf->cnt)) & 0xff, cf->fp);
		cf->ofs += 1;
	}

	cf->buf = 0;
	cf->cnt = 0;

	return (0);
}

int cas_write_header_check (cas_file_t *cf)
{
	if (cf->need_header == 0) {
		return (0);
	}

	if (cf->txt) {
		fprintf (cf->fp, "PTI 0\n\n");
		fprintf (cf->fp, "CLOCK %lu\n\n", cf->clock);
	}

	cf->need_header = 0;

	return (0);
}

int cas_write_silence (cas_file_t *cf, unsigned long ms)
{
	if (cas_write_header_check (cf)) {
		return (1);
	}

	if (cf->txt) {
		fprintf (cf->fp, "=%lu\n",
			(unsigned long) (((unsigned long long) ms * cf->clock) / 1000)
		);

		return (0);
	}

	if (cf->pcm == 0) {
		return (0);
	}

	while (ms > 0) {
		cf->acc += 4 * cf->srate;

		while (cf->acc >= 4000) {
			fputc (0, cf->fp);
			cf->acc -= 4000;
		}

		ms -= 1;
	}

	return (0);
}

static
int cas_write_bit_cas (cas_file_t *cf, int val)
{
	cf->buf = (cf->buf << 1) | (val != 0);
	cf->cnt += 1;

	if (cf->cnt >= 8) {
		fputc (cf->buf & 0xff, cf->fp);

		cf->ofs += 1;

		cf->buf = 0;
		cf->cnt = 0;
	}

	return (0);
}

static
int cas_write_bit_pcm (cas_file_t *cf, int val)
{
	unsigned      i;
	unsigned char smp;

	smp = 0xc0;

	for (i = 0; i < 2; i++) {
		cf->acc += val ? (2 * cf->srate) : cf->srate;

		while (cf->acc >= 4000) {
			fputc (smp, cf->fp);
			cf->ofs += 1;
			cf->acc -= 4000;
		}

		smp ^= 0x80;
	}

	return (0);
}

static
int cas_write_bit_txt (cas_file_t *cf, int val)
{
	unsigned long frq, clk;

	frq = val ? 1000 : 2000;
	clk = cf->clock / frq;

	fprintf (cf->fp, "-%lu +%lu\n", clk / 2, clk / 2);

	return (0);
}

int cas_write_bit (cas_file_t *cf, int val)
{
	if (cas_write_header_check (cf)) {
		return (1);
	}

	if (cf->txt) {
		return (cas_write_bit_txt (cf, val));
	}
	else if (cf->pcm) {
		return (cas_write_bit_pcm (cf, val));
	}
	else {
		return (cas_write_bit_cas (cf, val));
	}
}

int cas_write_byte (cas_file_t *cf, unsigned char val)
{
	unsigned i;

	for (i = 0; i < 8; i++) {
		if (cas_write_bit (cf, val & 0x80)) {
			return (1);
		}

		val <<= 1;
	}

	return (0);
}

int cas_write_leader (cas_file_t *cf, unsigned cnt, unsigned long delay)
{
	unsigned i;

	if (delay > 0) {
		if (cas_write_silence (cf, delay)) {
			return (1);
		}
	}

	for (i = 2; i < cnt; i++) {
		if (cas_write_byte (cf, 0xff)) {
			return (1);
		}
	}

	if (cas_write_byte (cf, 0xfe)) {
		return (1);
	}

	if (cas_write_byte (cf, 0x16)) {
		return (1);
	}

	return (0);
}

int cas_write_trailer (cas_file_t *cf)
{
	unsigned i;

	for (i = 0; i < 32; i++) {
		if (cas_write_bit (cf, 1)) {
			return (1);
		}
	}

	return (0);
}

int cas_write_block (cas_file_t *cf, const void *buf, unsigned cnt)
{
	unsigned      i;
	unsigned      crc;
	unsigned char blk[258];

	if (cnt > 256) {
		cnt = 256;
	}

	memcpy (blk, buf, cnt);

	for (i = cnt; i < 256; i++) {
		blk[i] = 0;
	}

	crc = cas_crc (0, blk, 256);

	blk[256] = (crc >> 8) & 0xff;
	blk[257] = crc & 0xff;

	for (i = 0; i < 258; i++) {
		if (cas_write_byte (cf, blk[i])) {
			return (1);
		}
	}

	return (0);
}

int cas_write_header (cas_file_t *cf, const char *name,
	unsigned type, unsigned size, unsigned seg, unsigned ofs)
{
	unsigned      i, n;
	unsigned char blk[256];

	memset (blk, 0, 256);

	if (name != NULL) {
		n = strlen (name);
	}
	else {
		n = 0;
	}

	blk[0] = 0xa5;

	for (i = 0; i < 8; i++) {
		blk[i + 1] = (i < n) ? name[i] : 0x20;
	}

	blk[9] = type & 0xff;

	blk[10] = size & 0xff;
	blk[11] = (size >> 8) & 0xff;

	blk[12] = seg & 0xff;
	blk[13] = (seg >> 8) & 0xff;

	blk[14] = ofs & 0xff;
	blk[15] = (ofs >> 8) & 0xff;

	blk[16] = 0;

	return (cas_write_block (cf, blk, 256));
}
