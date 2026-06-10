/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   src/drivers/sound/sound-sdl3.c                               *
 * Created:     2026-06-10 Egon Rath <egon.rath@gmail.com>                   *
 * Copyright:   (C) 2010-2017 Hampa Hug <hampa@hampa.ch>                     *
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
#include <limits.h>

#include <drivers/options.h>
#include <drivers/sound/sound.h>
#include <drivers/sound/sound-sdl3.h>

#include <SDL.h>

#ifndef DEBUG_SND_SDL
#define DEBUG_SND_SDL 0
#endif

static
sound_sdl3_buf_t *snd_sdl3_buf_new (sound_sdl3_t *drv, unsigned size)
{
	unsigned char   *tmp;
	sound_sdl3_buf_t *buf;
	
	if (drv->free != NULL) {
		buf = drv->free;
		drv->free = drv->free->next;
	}
	else {
		if (drv->buf_cnt > 32) {
			return (NULL);
		}

		buf = malloc (sizeof (sound_sdl3_buf_t));

		if (buf == NULL) {
			return (NULL);
		}

		buf->max = 0;
		buf->data = NULL;

		drv->buf_cnt += 1;
	}

	buf->next = NULL;

	buf->idx = 0;
	buf->cnt = 0;

	if (buf->max < size) {
		tmp = realloc (buf->data, size);

		if (tmp == NULL) {
			free (buf->data);
			free (buf);

			drv->buf_cnt -= 1;

			return (NULL);
		}

		buf->max = size;
		buf->data = tmp;
	}

	return (buf);
}

static
void snd_sdl3_buf_free_list (sound_sdl3_buf_t *buf)
{
	sound_sdl3_buf_t *tmp;
	
	while (buf != NULL) {
		tmp = buf;
		buf = buf->next;

		free (tmp->data);
		free (tmp);
	}
}

static
void snd_sdl3_close (sound_drv_t *sdrv)
{
	sound_sdl3_t *drv;
	
	drv = sdrv->ext;

	if (drv->is_open) {
		SDL_CloseAudioDevice (drv->dev);
	}

	snd_sdl3_buf_free_list (drv->head);
	snd_sdl3_buf_free_list (drv->free);

	snd_free (sdrv);

	free (drv);
}

static
int snd_sdl3_write (sound_drv_t *sdrv, const uint16_t *buf, unsigned cnt)
{
	int             sign;
	unsigned long   bcnt, scnt;
	sound_sdl3_buf_t *bbuf;
	sound_sdl3_t     *drv;
	
	drv = sdrv->ext;

	scnt = (unsigned long) sdrv->channels * (unsigned long) cnt;
	bcnt = 2 * scnt;

	bbuf = snd_sdl3_buf_new (drv, bcnt);

	if (bbuf == NULL) {
#if DEBUG_SND_SDL >= 1
		fprintf (stderr, "snd-sdl3: buffer overrun\n");
#endif
		return (1);
	}

	sign = (sdrv->sample_sign != drv->sign);

	snd_set_buf (bbuf->data, buf, scnt, sign, drv->big_endian);

	bbuf->idx = 0;
	bbuf->cnt = bcnt;

	if (drv->tail == NULL) {
		drv->head = bbuf;
	}
	else {
		drv->tail->next = bbuf;
	}

	drv->tail = bbuf;

	if (drv->is_paused) {
		SDL_ResumeAudioStreamDevice(drv->audio_stream);
		drv->is_paused = 0;
	}

	return (0);
}

static
void snd_sdl3_callback (void *user, Uint8 *buf, int cnt)
{
	int             n;
	sound_sdl3_t     *drv;
	sound_sdl3_buf_t *src;
	
	drv = user;

	if (drv->head == NULL) {
		SDL_PauseAudioStreamDevice(drv->audio_stream);
		drv->is_paused = 1;
		return;
	}

	while (cnt > 0) {
		if (drv->head == NULL) {
#if DEBUG_SND_SDL >= 1
			fprintf (stderr, "snd-sdl3: buffer underrun\n");
#endif
			memset (buf, 0, cnt);
			return;
		}

		src = drv->head;

		n = src->cnt - src->idx;

		if (n <= cnt) {
			memcpy (buf, src->data + src->idx, n);

			buf += n;
			cnt -= n;

			drv->head = src->next;

			if (drv->head == NULL) {
				drv->tail = NULL;
			}

			src->next = drv->free;
			drv->free = src;
		}
		else {
			memcpy (buf, src->data + src->idx, cnt);
			src->idx += cnt;
			cnt = 0;
		}
	}
}

static 
void SDLCALL snd_sdl3_new_callback (void *userdata,SDL_AudioStream *stream, int additional, int total)
{
	if (additional>0) {
		Uint8 *data = SDL_stack_alloc(Uint8, additional);
		if (data) {
			snd_sdl3_callback (userdata,data,additional);
			SDL_PutAudioStreamData (stream,data,additional);
			SDL_stack_free (data);
		}
	}
}

static
int snd_sdl3_set_params (sound_drv_t *sdrv, unsigned chn, unsigned long srate, int sign)
{
	sound_sdl3_t   *drv;
	SDL_AudioSpec   req;
	
	drv = sdrv->ext;

	if (SDL_InitSubSystem (SDL_INIT_AUDIO) == 0) {
		fprintf (stderr,
			"snd-sdl3: error initializing audio subsystem (%s)\n",
			SDL_GetError()
		);
		return (1);
	}

	if (drv->is_open) {
		SDL_CloseAudioDevice (drv->dev);
		drv->is_open = 0;
	}

	SDL_memset (&req, 0, sizeof (req));
	req.freq     = srate;
	req.format   = SDL_AUDIO_S16LE;
	req.channels = chn;

	/* open the audio device itself for playback */
	drv->dev = SDL_OpenAudioDevice (SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &req);
	if (drv->dev == 0) {
		fprintf (stderr, "snd-sdl3: error opening output device (%s)\n",
			SDL_GetError()
		);
		return (1);
	}
	else
		fprintf (stdout, "snd-sdl3: opened audio output (%s)\n", SDL_GetAudioDeviceName (drv->dev));
		
	/* open a audio stream for this device for playing back something */
	drv->audio_stream = SDL_OpenAudioDeviceStream (drv->dev,&req,snd_sdl3_new_callback,drv);
	if (drv->audio_stream == NULL) {
		fprintf (stderr, "snd-sdl3: error opening output audio stream (%s)\n", SDL_GetError());
		return (1);
	}
	else
		fprintf(stdout,"snd-sdl3: opened audio output stream\n");

	/* note: the audio device opened by SDL_OpenAudioDevice starts unpause, however, the
	         audio stream starts paused. We never pause the audio device itself, as it results
		 in small clicks in the output, we just pause/resume the stream */

	drv->is_open = 1;
	drv->is_paused = 1;

	drv->sign = 1;
	drv->big_endian = 0;

	return (0);
}

static
int snd_sdl3_init (sound_sdl3_t *drv, const char *name)
{
	snd_init (&drv->sdrv, drv);

	drv->sdrv.close = snd_sdl3_close;
	drv->sdrv.write = snd_sdl3_write;
	drv->sdrv.set_params = snd_sdl3_set_params;

	drv->dev = 0;
	drv->is_open = 0;
	drv->is_paused = 1;

	drv->buf_cnt = 0;

	drv->head = NULL;
	drv->tail = NULL;
	drv->free = NULL;

	return (0);
}

sound_drv_t *snd_sdl3_open (const char *name)
{
	sound_sdl3_t *drv;
	
	drv = malloc (sizeof (sound_sdl3_t));

	if (drv == NULL) {
		return (NULL);
	}

	if (snd_sdl3_init (drv, name)) {
		snd_sdl3_close (&drv->sdrv);
		return (NULL);
	}

	return (&drv->sdrv);
}
