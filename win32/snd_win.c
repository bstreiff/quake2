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
#include <float.h>

#include "../client/client.h"
#include "../client/snd_loc.h"
#include "winquake.h"
#include "SDL.h"

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

static qboolean	snd_firsttime = true;
static qboolean snd_init;

// starts at 0 for disabled
static int	sample16;
static int	snd_sent, snd_completed;
static byte done[WAV_BUFFERS];

static SDL_AudioDeviceID audio_device = 0;
static byte* sound_secondary_buffer = NULL;

qboolean SNDDMA_InitSDL(void);

void FreeSound( void );

struct CircularBuffer
{
	byte* data;
	size_t begin_index;
	size_t end_index;
	size_t size;
	size_t capacity;
};
struct CircularBuffer sound_buffer;

/*
==================
FreeSound
==================
*/
void FreeSound (void)
{
	int		i;

	Com_DPrintf( "Shutting down sound system\n" );

	if (audio_device)
	{
		SDL_CloseAudioDevice(audio_device);
	}

	if (sound_buffer.data)
	{
		free(sound_buffer.data);
	}

	if (sound_secondary_buffer)
	{
		free(sound_secondary_buffer);
	}

	snd_init = false;
}

static size_t SpaceAvailableInSoundBuffer()
{
	return sound_buffer.capacity - sound_buffer.size;
}

static size_t CopyIntoSoundBuffer(const byte* data, const size_t length)
{
	const size_t bytes_to_write = min(length, SpaceAvailableInSoundBuffer());

	if (!data || length == 0)
		return 0;

	if (bytes_to_write <= sound_buffer.capacity - sound_buffer.end_index)
	{
		memcpy(sound_buffer.data + sound_buffer.end_index, data, bytes_to_write);
		sound_buffer.end_index += bytes_to_write;
		if (sound_buffer.end_index == sound_buffer.capacity)
			sound_buffer.end_index = 0;
	}
	else
	{
		const size_t size_a = sound_buffer.capacity - sound_buffer.end_index;
		const size_t size_b = bytes_to_write - size_a;
		memcpy(sound_buffer.data + sound_buffer.end_index, data, size_a);
		memcpy(sound_buffer.data, data + size_a, size_b);
		sound_buffer.end_index = size_b;
	}

	sound_buffer.size += bytes_to_write;
	return bytes_to_write;
}

static size_t CopyFromSoundBuffer(byte* data, size_t requested_size)
{
	const size_t bytes_to_read = min(requested_size, sound_buffer.size);

	if (!data || requested_size == 0)
		return 0;

	if (bytes_to_read <= sound_buffer.capacity - sound_buffer.begin_index)
	{
		memcpy(data, sound_buffer.data + sound_buffer.begin_index, bytes_to_read);
		sound_buffer.begin_index += bytes_to_read;
		if (sound_buffer.begin_index == sound_buffer.capacity)
			sound_buffer.begin_index = 0;
	}
	else
	{
		const size_t size_a = sound_buffer.capacity - sound_buffer.begin_index;
		const size_t size_b = bytes_to_read - size_a;
		memcpy(data, sound_buffer.data + sound_buffer.begin_index, size_a);
		memcpy(data + size_a, sound_buffer.data, size_b);
		sound_buffer.begin_index = size_b;
	}

	sound_buffer.size -= bytes_to_read;
	return bytes_to_read;
}

void SNDDMA_Callback(void* userdata, Uint8* data, int length)
{
	static int d = 0;

	for (int i = 0; i < length / WAV_BUFFER_SIZE; ++i)
	{
		size_t read_length = CopyFromSoundBuffer(data, WAV_BUFFER_SIZE);
		data += WAV_BUFFER_SIZE;

		if (read_length != WAV_BUFFER_SIZE)
		{
			// Ran out of data.
			memset(data + read_length, 0, length - read_length);
			SDL_PauseAudioDevice(audio_device, 1);
			break;
		}

		done[d&WAV_MASK] = true;
		++d;
	}
}

qboolean SNDDMA_InitSDL(void)
{
	SDL_AudioSpec desired = {0};
	SDL_AudioSpec obtained = {0};

	Com_Printf("Initializing SDL audio\n");

	if (s_khz->value == 44)
		desired.freq = 44100;
	else if (s_khz->value == 22)
		desired.freq = 22050;
	else
		desired.freq = 11025;

	desired.channels = 2;
	desired.format = AUDIO_S16;
	desired.samples = WAV_BUFFER_SIZE;
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
		return false;
	}
	else
	{
		Com_Printf("... %d Hz, %d channels\n", obtained.freq, obtained.channels);
	}

	// This is the main circular buffer.
	sound_buffer.data = (byte*)malloc(SECONDARY_BUFFER_SIZE);
	sound_buffer.capacity = SECONDARY_BUFFER_SIZE;
	sound_buffer.size = 0;
	sound_buffer.begin_index = 0;
	sound_buffer.end_index = 0;
	memset(sound_buffer.data, 0, sound_buffer.capacity);

	// This is the secondary buffer that everything dumps into.
	sound_secondary_buffer = (byte*)malloc(WAV_BUFFERS*WAV_BUFFER_SIZE);
	memset(sound_secondary_buffer, 0, WAV_BUFFERS*WAV_BUFFER_SIZE);

	dma.buffer = sound_secondary_buffer;
	dma.channels = obtained.channels;
	dma.samplebits = 16;
	dma.samplepos = 0;
	dma.samples = (WAV_BUFFERS*WAV_BUFFER_SIZE) / (dma.samplebits / 8);
	dma.speed = obtained.freq;
	dma.submission_chunk = 512;
	sample16 = (dma.samplebits / 8) - 1;

	snd_sent = 0;
	snd_completed = 0;

	return true;
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
	memset ((void *)&dma, 0, sizeof (dma));

	snd_init = SNDDMA_InitSDL();
	snd_firsttime = false;

	if (!snd_init)
	{
		if (snd_firsttime)
			Com_Printf ("*** No sound device initialized ***\n");

		return 0;
	}

	return 1;
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
	int		s = 0;

	if (snd_init)
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}

	s >>= sample16;
	s &= (dma.samples-1);

	return s;
}

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
void SNDDMA_BeginPainting (void)
{
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
	if (!dma.buffer)
		return;

	for (;;)
	{
		if (snd_completed == snd_sent)
			break;

		if (!done[snd_completed&WAV_MASK])
			break;

		snd_completed++;
	}

	while (((snd_sent - snd_completed) >> sample16) < 8)
	{
		if (paintedtime / 256 <= snd_sent)
			break;

		snd_sent++;

		SDL_LockAudioDevice(audio_device);
		done[snd_sent&WAV_MASK] = false;
		if (CopyIntoSoundBuffer(dma.buffer + (snd_sent&WAV_MASK)*WAV_BUFFER_SIZE, WAV_BUFFER_SIZE) != WAV_BUFFER_SIZE)
			Com_Printf("sound overflow\n");
		SDL_PauseAudioDevice(audio_device, 0);
		SDL_UnlockAudioDevice(audio_device);
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	FreeSound ();
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

