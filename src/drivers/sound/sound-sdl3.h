/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   src/drivers/sound/sound-sdl3.h                               *
 * Created:     2026-06-10 Egon Rath <egon.rath@gmail.com>                   *
 * Copyright:   (C) 2010 Hampa Hug <hampa@hampa.ch>                          *
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


#ifndef PCE_DRIVERS_SOUND_SDL3_H
#define PCE_DRIVERS_SOUND_SDL3_H 1

#include <drivers/sound/sound.h>

#include <SDL.h>


typedef struct sound_sdl3_buf_t {
	struct sound_sdl3_buf_t *next;
	unsigned               idx;
	unsigned               cnt;
	unsigned               max;
	unsigned char          *data;
} sound_sdl3_buf_t;

typedef struct sound_sdl3_t {
	sound_drv_t sdrv;

	char        is_open;
	char        is_paused;

	int         sign;
	int         big_endian;

	unsigned    buf_cnt;

	SDL_AudioDeviceID    dev;	
	SDL_AudioStream      *audio_stream;

	sound_sdl3_buf_t *head;
	sound_sdl3_buf_t *tail;
	sound_sdl3_buf_t *free;
} sound_sdl3_t;


#endif
