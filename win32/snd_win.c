/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "../client/client.h"
#include "../client/snd_loc.h"
#include "SDL.h"

static qboolean snd_init;

static SDL_AudioDeviceID audio_device = 0;

void SNDDMA_Callback(void* userdata, Uint8* data, int length)
{
	dma.buffer = data;
	dma.samplepos += length / (dma.samplebits / 4);
	S_PaintChannels(dma.samplepos);
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
int SNDDMA_Init(void)
{
	SDL_AudioSpec desired = { 0 };
	SDL_AudioSpec obtained = { 0 };

	S_StopAllSounds();

	Com_Printf("Initializing SDL audio\n");

	if (s_khz->value == 44)
		desired.freq = 44100;
	else if (s_khz->value == 22)
		desired.freq = 22050;
	else
		desired.freq = 11025;

	desired.channels = 2;
	desired.samples = 2048;
	desired.format = AUDIO_S16;
	desired.callback = SNDDMA_Callback;
	desired.userdata = &dma;

	audio_device = SDL_OpenAudioDevice(
		NULL,  /* default sound device */
		false, /* playback */
		&desired,
		&obtained,
		SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

	if (audio_device == 0)
	{
		Com_Printf("... unable to open.\n");
		snd_init = false;
		return false;
	}

	memset((void *)&dma, 0, sizeof (dma));
	dma.buffer = NULL;
	dma.channels = obtained.channels;
	dma.samplebits = 16;
	dma.samplepos = 0;
	dma.samples = obtained.samples * obtained.channels;
	dma.speed = obtained.freq;
	dma.submission_chunk = 512;

	snd_init = true;
	return true;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	return dma.samplepos;
}

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
void SNDDMA_BeginPainting (void)
{
	SDL_PauseAudioDevice(audio_device, 0);
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the dsound buffer
===============
*/
void SNDDMA_Submit(void)
{
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	Com_DPrintf("Shutting down sound system\n");

	if (audio_device)
	{
		SDL_CloseAudioDevice(audio_device);
	}

	snd_init = false;
}

/*
===========
S_Activate

Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void S_Activate (qboolean active)
{
	if (snd_init)
	{
		if ( active )
		{
			SDL_PauseAudioDevice(audio_device, 0);
		}
		else
		{
			SDL_PauseAudioDevice(audio_device, 1);
		}
	}
}

