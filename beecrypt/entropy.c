/*
 * entropy.c
 *
 * entropy gathering routine for pseudo-random generator initialization
 *
 * Copyright (c) 1998-2000 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define BEECRYPT_DLL_EXPORT

#include "entropy.h"
#include "endianness.h"

#if WIN32
# include <mmsystem.h>
#else 
# if HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif
# if HAVE_SYS_STAT_H
#  include <sys/types.h>
#  include <sys/stat.h>
# endif
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# if HAVE_SYS_AUDIOIO_H
#  include <sys/audioio.h>
# endif
# if HAVE_SYS_SOUNDCARD_H
#  include <sys/soundcard.h>
# endif
# if HAVE_TERMIO_H
#  include <termio.h>
# endif
# if HAVE_SYNCH_H
#  include <synch.h>
# elif HAVE_PTHREAD_H
#  include <pthread.h>
# else
#  error need locking mechanism
# endif
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif

#include <stdio.h>

#if WIN32
static HINSTANCE	entropy_instance = (HINSTANCE) 0;

static HANDLE entropy_wavein_lock;
static HANDLE entropy_wavein_event;

int entropy_provider_setup(HINSTANCE hInst)
{
	if (!entropy_instance)
	{
		entropy_instance = hInst;
		if (!(entropy_wavein_lock = CreateMutex(NULL, FALSE, NULL)))
			return -1;
		if (!(entropy_wavein_event = CreateEvent(NULL, FALSE, FALSE, NULL)))
			return -1;
	}
	return 0;
}

int entropy_provider_cleanup()
{
	if (entropy_wavein_lock)
	{
		CloseHandle(entropy_wavein_lock);
		entropy_wavein_lock = 0;
	}
	if (entropy_wavein_event)
	{
		CloseHandle(entropy_wavein_event);
		entropy_wavein_event = 0;
	}
	return 0;
}

static int entropy_noisebits_8(HWAVEIN wavein, uint32 *data, int size)
{
	uint32 randombits = size << 5;
	uint32 temp = 0;

	/* first set up a wave header */
	WAVEHDR header;
	/* use a 1K buffer */
	uint8 sample[1024];

	header.lpData = (LPSTR) sample;
	header.dwBufferLength = 1024 * sizeof(uint8);
	header.dwFlags = 0;

	/* do error handling! */
	waveInStart(wavein);
	
	/* the first event is the due to the opening of the wave */
	ResetEvent(entropy_wavein_event);
	
	while (randombits)
	{
		register int i;
		
		while (1)
		{
			/* pass the buffer to the wavein and wait for the event */
			waveInPrepareHeader(wavein, &header, sizeof(WAVEHDR));
			waveInAddBuffer(wavein, &header, sizeof(WAVEHDR));

			/* in case we have to wait more than 10 seconds, bail out */
			if (WaitForSingleObject(entropy_wavein_event, 10000) == WAIT_OBJECT_0)
			{
				/* only process if we read the whole thing */
				if (header.dwBytesRecorded == header.dwBufferLength)
					break;
			}
			else
			{
				waveInReset(wavein);
				waveInClose(wavein);
				return -1;
			}
		}
		
		/* on windows, no swap of sample data is necessary */
		for (i = 0; randombits && (i < 1024); i += 2)
		{
			if ((sample[i] ^ sample[i+1]) & 0x1)
			{
				temp <<= 1;
				if (sample[i] & 0x1)
					temp |= 0x1;
				randombits--;
				if (!(randombits & 0x1F))
					*(data++) = temp;
			}
		}
	}
	
	waveInReset(wavein);
	waveInClose(wavein);

	ReleaseMutex(entropy_wavein_lock);

	return 0;
}

static int entropy_noisebits_16(HWAVEIN wavein, uint32 *data, int size)
{
	uint32 randombits = size << 5;
	uint32 temp = 0;

	/* first set up a wave header */
	WAVEHDR header;
	/* use a 1K buffer */
	uint16 sample[512];

	header.lpData = (LPSTR) sample;
	header.dwBufferLength = 512 * sizeof(uint16);
	header.dwFlags = 0;

	/* do error handling! */
	waveInStart(wavein);
	
	/* the first event is the due to the opening of the wave */
	ResetEvent(entropy_wavein_event);
	
	while (randombits)
	{
		register int i;
		
		while (1)
		{
			/* pass the buffer to the wavein and wait for the event */
			waveInPrepareHeader(wavein, &header, sizeof(WAVEHDR));
			waveInAddBuffer(wavein, &header, sizeof(WAVEHDR));

			/* in case we have to wait more than 10 seconds, bail out */
			if (WaitForSingleObject(entropy_wavein_event, 10000) == WAIT_OBJECT_0)
			{
				/* only process if we read the whole thing */
				if (header.dwBytesRecorded == header.dwBufferLength)
					break;
			}
			else
			{
				waveInReset(wavein);
				waveInClose(wavein);
				return -1;
			}
		}
		
		/* on windows, no swap of sample data is necessary */
		for (i = 0; randombits && (i < 512); i += 2)
		{
			if ((sample[i] ^ sample[i+1]) & 0x1)
			{
				temp <<= 1;
				if (sample[i] & 0x1)
					temp |= 0x1;
				randombits--;
				if (!(randombits & 0x1F))
					*(data++) = temp;
			}
		}
	}
	
	waveInReset(wavein);
	waveInClose(wavein);

	ReleaseMutex(entropy_wavein_lock);

	return 0;
}

int entropy_wavein(uint32* data, int size)
{
	WAVEINCAPS		waveincaps;
	WAVEFORMATEX	waveformatex;
	HWAVEIN			wavein;
	MMRESULT		numdevs;
	MMRESULT		rc;
	
	numdevs = waveInGetNumDevs();
	if (numdevs <= 0)
		return -1;
	
	rc = waveInGetDevCaps(0, &waveincaps, sizeof(WAVEINCAPS));
	if (rc != MMSYSERR_NOERROR)
		return -1;

	/* first go for the 16 bits samples -> more chance of noise bits */
	switch (waveformatex.nChannels = waveincaps.wChannels)
	{
	case 1:
		/* mono */
		if (waveincaps.dwFormats & WAVE_FORMAT_4M16)
		{
			waveformatex.nSamplesPerSec = 44100;
			waveformatex.wBitsPerSample = 16;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_2M16)
		{
			waveformatex.nSamplesPerSec = 22050;
			waveformatex.wBitsPerSample = 16;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_1M16)
		{
			waveformatex.nSamplesPerSec = 11025;
			waveformatex.wBitsPerSample = 16;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_4M08)
		{
			waveformatex.nSamplesPerSec = 44100;
			waveformatex.wBitsPerSample = 8;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_2M08)
		{
			waveformatex.nSamplesPerSec = 22050;
			waveformatex.wBitsPerSample = 8;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_1M08)
		{
			waveformatex.nSamplesPerSec = 11025;
			waveformatex.wBitsPerSample = 8;
		}
		else
			return -1;
			
		break;
	case 2:
		/* stereo */
		if (waveincaps.dwFormats & WAVE_FORMAT_4S16)
		{
			waveformatex.nSamplesPerSec = 44100;
			waveformatex.wBitsPerSample = 16;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_2S16)
		{
			waveformatex.nSamplesPerSec = 22050;
			waveformatex.wBitsPerSample = 16;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_1S16)
		{
			waveformatex.nSamplesPerSec = 11025;
			waveformatex.wBitsPerSample = 16;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_4S08)
		{
			waveformatex.nSamplesPerSec = 44100;
			waveformatex.wBitsPerSample = 8;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_2S08)
		{
			waveformatex.nSamplesPerSec = 22050;
			waveformatex.wBitsPerSample = 8;
		}
		else if (waveincaps.dwFormats & WAVE_FORMAT_1S08)
		{
			waveformatex.nSamplesPerSec = 11025;
			waveformatex.wBitsPerSample = 8;
		}
		else
			return -1;

		break;
	}
	
	waveformatex.wFormatTag = WAVE_FORMAT_PCM;
	waveformatex.nAvgBytesPerSec = (waveformatex.nSamplesPerSec * waveformatex.nChannels * waveformatex.wBitsPerSample) / 8;
	waveformatex.nBlockAlign = (waveformatex.nChannels * waveformatex.wBitsPerSample) / 8;
	waveformatex.cbSize = 0;

	/* we now have the wavein's capabilities hammered out; from here on we need to lock */

	if (WaitForSingleObject(entropy_wavein_lock, INFINITE) != WAIT_OBJECT_0)
		return -1;
		
	rc = waveInOpen(&wavein, 0, &waveformatex, (DWORD) entropy_wavein_event, (DWORD) 0, CALLBACK_EVENT);
	if (rc != MMSYSERR_NOERROR)
	{
		fprintf(stderr, "waveInOpen returned %d\n", rc);
		ReleaseMutex(entropy_wavein_lock);
		return -1;
	}

	switch (waveformatex.wBitsPerSample)
	{
	case 8:
		return entropy_noisebits_8(wavein, data, size);
	case 16:
		return entropy_noisebits_16(wavein, data, size);
	default:
		waveInClose(wavein);
		ReleaseMutex(entropy_wavein_lock);
		return -1;
	}
}
#else
#if HAVE_DEV_AUDIO
static const char* name_dev_audio = "/dev/audio";
static int dev_audio_fd = -1;
#ifdef _REENTRANT
#if HAVE_SYNCH_H
static mutex_t dev_audio_lock = DEFAULTMUTEX;
#elif HAVE_PTHREAD_H
static pthread_mutex_t dev_audio_lock = PTHREAD_MUTEX_INITIALIZER;
#else
#error Need locking mechanism
#endif
#endif
#endif

#if HAVE_DEV_DSP
static const char* name_dev_dsp = "/dev/dsp";
static int dev_dsp_fd = -1;
#ifdef _REENTRANT
#if HAVE_SYNCH_H
static mutex_t dev_dsp_lock = DEFAULTMUTEX;
#elif HAVE_PTHREAD_H
static pthread_mutex_t dev_dsp_lock = PTHREAD_MUTEX_INITIALIZER;
#else
#error Need locking mechanism
#endif
#endif
#endif

#if HAVE_DEV_RANDOM
static const char* name_dev_random = "/dev/random";
static int dev_random_fd = -1;
#ifdef _REENTRANT
#if HAVE_SYNC_H
static mutex_t dev_random_lock = DEFAULTMUTEX;
#elif HAVE_PTHREAD_H
static pthread_mutex_t dev_random_lock = PTHREAD_MUTEX_INITIALIZER;
#else
#error Need locking mechanism
#endif
#endif
#endif

#if HAVE_DEV_TTY
static const char *dev_tty_name = "/dev/tty";
static int dev_tty_fd = -1;
#ifdef _REENTRANT
#if HAVE_SYNCH_H
static mutex_t dev_tty_lock = DEFAULTMUTEX;
#elif HAVE_PTHREAD_H
static pthread_mutex_t dev_tty_lock = PTHREAD_MUTEX_INITIALIZER;
#else
#error Need locking mechanism
#endif
#endif
#endif

#if HAVE_SYS_STAT_H
static int statdevice(const char *device)
{
	struct stat s;

	if (stat(device, &s) < 0)
	{
		#if HAVE_ERRNO_H && HAVE_STRING_H
		fprintf(stderr, "cannot stat %s: %s\n", device, strerror(errno));
		#endif
		return -1;
	}
	if (!S_ISCHR(s.st_mode))
	{
		fprintf(stderr, "%s is not a device\n", device);
		return -1;
	}
	return 0;
}
#endif

#if HAVE_FCNTL_H
static int opendevice(const char *device)
{
	register int fd, flags, rc;

	if ((fd = open(device, O_RDWR | O_NONBLOCK)) < 0)
	{
		#if HAVE_ERRNO_H && HAVE_STRING_H
		fprintf(stderr, "open of %s failed: %s\n", device, strerror(errno));
		#endif
		return fd;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
	{
		rc = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		if (rc < 0)
		{
			#if HAVE_ERRNO_H
			perror("fcntl F_SETFL failed");
			#endif
			return rc;
		}
	}
	else
	{
		#if HAVE_ERRNO_H
		perror("fcntl F_GETFL failed");
		#endif
		return flags;
	}
	
	return fd;
}
#endif

#if HAVE_DEV_AUDIO || HAVE_DEV_DSP
/* bit deskewing technique: the classical Von Neumann method
	- only use the lsb bit of every sample
	- there is a chance of bias in 0 or 1 bits, so to deskew this:
		- look at two successive sampled bits
		- if they are the same, discard them
		- if they are different, they're either 0-1 or 1-0; use the first bit of the pair as output
*/

static int entropy_be_noisebits(int fd, uint32 *data, int size)
{
	register uint32 randombits = size << 5;
	register uint32 temp;
	uint16 sample[2];

	while (randombits)
	{
		if (read(fd, sample, sizeof(sample)) < 0)
		{
			#if HAVE_ERRNO_H
			perror("read failed");
			#endif
			return -1;
		}

		if ((sample[0] ^ sample[1]) & 0x1)
		{
			temp <<= 1;
			if (sample[0] & 0x1)
				temp |= 0x1;
			randombits--;
			if (!(randombits & 0x1F))
				*(data++) = temp;
		}
	}
	return 0;
}

static int entropy_le_noisebits(int fd, uint32 *data, int size)
{
	register uint32 randombits = size << 5;
	register uint32 temp;
	uint16 sample[2];

	while (randombits)
	{
		if (read(fd, sample, sizeof(sample)) < 0)
		{
			#if HAVE_ERRNO_H
			perror("read failed");
			#endif
			return -1;
		}

		if (swapu16(sample[0] ^ sample[1]) & 0x1)
		{
			temp <<= 1;
			if (sample[0] & 0x1)
				temp |= 0x1;
			randombits--;
			if (!(randombits & 0x1F))
				*(data++) = temp;
		}
	}
	return 0;
}
#endif

#if HAVE_DEV_RANDOM
static int entropy_randombits(int fd, uint32* data, int size)
{
	register uint32 randombytes = size << 2;
	register uint32 temp;
	uint8  ch;

	while (randombytes)
	{
		if (read(fd, &ch, 1) < 0)
		{
			#if HAVE_ERRNO_H
			perror("read failed");
			#endif
			return -1;
		}

		temp <<= 8;
		temp |= ch;

		randombytes--;
		if (!(randombytes & 0x3))
			*(data++) = temp;
	}
	return 0;
}
#endif

#if HAVE_DEV_TTY
static int entropy_ttybits(int fd, uint32* data, int size)
{
	register uint32 randombits = size << 5;
	register uint32 temp;
	byte dummy;
	#if HAVE_TERMIO_H
	struct termio tio_save, tio_set;
	#endif
	#if HAVE_GETHRTIME
	hrtime_t hrtsample;
	#elif HAVE_GETTIMEOFDAY
	struct timeval tvsample;
	#else
	# error Need alternative high-precision timer
	#endif

	printf("please press random keys on your keyboard\n");
	#if HAVE_TERMIO_H
	if (ioctl(fd, TCGETA, &tio_save) < 0)
	{
		#if HAVE_ERRNO_H
		perror("ioctl TCGETA failed");
		#endif
		return -1;
	}

	tio_set = tio_save;
	tio_set.c_cc[VMIN] = 1;				/* read 1 tty character at a time */
	tio_set.c_cc[VTIME] = 0;			/* don't timeout the read */
	tio_set.c_iflag |= IGNBRK;			/* ignore <ctrl>-c */
	tio_set.c_lflag &= ~(ECHO|ICANON);	/* don't echo characters */

	/* change the tty settings, and flush input characters */
	if (ioctl(fd, TCSETAF, &tio_set) < 0)
	{
		#if HAVE_ERRNO_H
		perror("ioctl TCSETAF failed");
		#endif
		return -1;
	}
	#else
	# error Need alternative tty control library
	#endif

	while (randombits)
	{
		if (read(fd, &dummy, 1) < 0)
		{
			#if HAVE_ERRNO_H
			perror("tty read failed");
			#endif
			return -1;
		}
		printf("."); fflush(stdout);
		#if HAVE_GETHRTIME
		hrtsample = gethrtime();
		/* get 16 bits from the sample */
		temp <<= 16;
		/* discard the 10 lowest bits i.e. 1024 nanoseconds */
		temp |= (uint16)(hrtsample >> 10);
		randombits -= 16;
		#elif HAVE_GETTIMEOFDAY
		/* discard the 4 lowest bits i.e. 4 microseconds */
		gettimeofday(&tvsample, 0);
		/* get 8 bits from the sample */
		temp <<= 8;
		temp |= (uint8)(tvsample.tv_usec >> 2);
		randombits -= 8;
		#else
		# error Need alternative high-precision timer sample
		#endif
		if (!(randombits & 0x1f))
			*(data++) = temp;
	}

	printf("\nthanks\n");

	/* give the user 1 second to stop typing */
	sleep(1);

	#if HAVE_TERMIO_H
	/* restore the tty settings, and flush input characters */
	if (ioctl(fd, TCSETAF, &tio_save) < 0)
	{
		#if HAVE_ERRNO_H
		perror("ioctl TCSETAF failed");
		#endif
		return -1;
	}
	#else
	# error Need alternative tty control library
	#endif

	return 0;
}
#endif

#if HAVE_DEV_AUDIO
int entropy_dev_audio(uint32 *data, int size)
{
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	if (mutex_lock(&dev_audio_lock))
		return -1;
	# elif HAVE_PTHREAD_H
	if (pthread_mutex_lock(&dev_audio_lock))
		return -1;
	# else
	#  error need locking mechanism
	# endif
	#endif

	#if HAVE_SYS_STAT_H
	if (statdevice(name_dev_audio) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_audio_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_audio_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}
	#endif
	
	if ((dev_audio_fd = opendevice(name_dev_audio)) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_audio_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_audio_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}

	#if HAVE_SYS_AUDIOIO_H /* i.e. Solaris */
	{
		struct audio_info info;

		AUDIO_INITINFO(&info);

		info.record.sample_rate = 48000;
		info.record.channels = 2;
		info.record.precision = 16;
		info.record.encoding = AUDIO_ENCODING_LINEAR;
		info.record.gain = AUDIO_MAX_GAIN;
		info.record.pause = 0;
		info.record.buffer_size = 4096;
		info.record.samples = 0;

		if (ioctl(dev_audio_fd, AUDIO_SETINFO, &info) < 0)
		{
			if (errno == EINVAL)
			{
				/* use a conservative setting this time */
				info.record.sample_rate = 22050;
				info.record.channels = 1;
				info.record.precision = 8;

				if (ioctl(dev_audio_fd, AUDIO_SETINFO, &info) < 0)
				{
					#if HAVE_ERRNO_H
					perror("ioctl AUDIO_SETINFO failed");
					#endif
					close(dev_audio_fd);
					#ifdef _REENTRANT
					# if HAVE_SYNCH_H
					mutex_unlock(&dev_audio_lock);
					# elif HAVE_PTHREAD_H
					pthread_mutex_unlock(&dev_audio_lock);
					# else
					#  error need locking mechanism
					# endif
					#endif
					return -1;
				}
			}
			else
			{
				#if HAVE_ERRNO_H
				perror("ioctl AUDIO_SETINFO failed");
				#endif
				close(dev_audio_fd);
				#ifdef _REENTRANT
				# if HAVE_SYNCH_H
				mutex_unlock(&dev_audio_lock);
				# elif HAVE_PTHREAD_H
				pthread_mutex_unlock(&dev_audio_lock);
				# else
				#  error need locking mechanism
				# endif
				#endif
				return -1;
			}
		}

		if (entropy_be_noisebits(dev_audio_fd, data, size) < 0)
		{
			close(dev_audio_fd);
			#ifdef _REENTRANT
			# if HAVE_SYNCH_H
			mutex_unlock(&dev_audio_lock);
			# elif HAVE_PTHREAD_H
			pthread_mutex_unlock(&dev_audio_lock);
			# else
			#  error need locking mechanism
			# endif
			#endif
			return -1;
		}
	}
	#else
	# error Unknown type of /dev/audio interface
	#endif

	close(dev_audio_fd);
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	mutex_unlock(&dev_audio_lock);
	# elif HAVE_PTHREAD_H
	pthread_mutex_unlock(&dev_audio_lock);
	# else
	#  error need locking mechanism
	# endif
	#endif
	return 0;
}
#endif

#if HAVE_DEV_DSP
int entropy_dev_dsp(uint32 *data, int size)
{
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	if (mutex_lock(&dev_dsp_lock))
		return -1;
	# elif HAVE_PTHREAD_H
	if (pthread_mutex_lock(&dev_dsp_lock))
		return -1;
	# else
	#  error need locking mechanism
	# endif
	#endif

	#if HAVE_SYS_STAT_H
	if (statdevice(name_dev_dsp) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_dsp_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_dsp_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}
	#endif
	
	if ((dev_dsp_fd = opendevice(name_dev_dsp)) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_dsp_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_dsp_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}

	#if HAVE_SYS_SOUNDCARD_H /* i.e. Linux audio */
	{
		int mask, format, stereo, speed, swap;

		if (ioctl(dev_dsp_fd, SNDCTL_DSP_GETFMTS, &mask) < 0)
		{
			#if HAVE_ERRNO_H
			perror("ioctl SNDCTL_DSP_GETFMTS failed");
			#endif
			close (dev_dsp_fd);
			#ifdef _REENTRANT
			# if HAVE_SYNCH_H
			mutex_unlock(&dev_dsp_lock);
			# elif HAVE_PTHREAD_H
			pthread_mutex_unlock(&dev_dsp_lock);
			# else
			#  error need locking mechanism
			# endif
			#endif
			return -1;
		}

		#if WORDS_BIGENDIAN
		if (mask & AFMT_S16_BE)
		{
			format = AFMT_S16_BE;
			swap = 0;
		}
		else if (mask & AFMT_S16_LE)
		{
			format = AFMT_S16_LE;
			swap = 1;
		}
		#else
		if (mask & AFMT_S16_LE)
		{
			format = AFMT_S16_LE;
			swap = 0;
		}
		else if (mask & AFMT_S16_BE)
		{
			format = AFMT_S16_BE;
			swap = 1;
		}
		#endif
		else if (mask & AFMT_S8)
		{
			format = AFMT_S8;
			swap = 0;
		}
		else
		{
			/* No linear audio format available */
			close(dev_dsp_fd);
			#ifdef _REENTRANT
			# if HAVE_SYNCH_H
			mutex_unlock(&dev_dsp_lock);
			# elif HAVE_PTHREAD_H
			pthread_mutex_unlock(&dev_dsp_lock);
			# else
			#  error need locking mechanism
			# endif
			#endif
			return -1;
		}

		if (ioctl(dev_dsp_fd, SNDCTL_DSP_SETFMT, &format) < 0)
		{
			#if HAVE_ERRNO_H
			perror("ioctl SNDCTL_DSP_SETFMT failed");
			#endif
			close(dev_dsp_fd);
			#ifdef _REENTRANT
			# if HAVE_SYNCH_H
			mutex_unlock(&dev_dsp_lock);
			# elif HAVE_PTHREAD_H
			pthread_mutex_unlock(&dev_dsp_lock);
			# else
			#  error need locking mechanism
			# endif
			#endif
			return -1;
		}

		stereo = 1;
		ioctl(dev_dsp_fd, SNDCTL_DSP_STEREO, &stereo);

		speed = 44100;
		ioctl(dev_dsp_fd, SNDCTL_DSP_SPEED, &speed);

		if (swap)
		{
			if (entropy_le_noisebits(dev_dsp_fd, data, size) < 0)
			{
				close(dev_dsp_fd);
				#ifdef _REENTRANT
				# if HAVE_SYNCH_H
				mutex_unlock(&dev_dsp_lock);
				# elif HAVE_PTHREAD_H
				pthread_mutex_unlock(&dev_dsp_lock);
				# else
				#  error need locking mechanism
				# endif
				#endif
				return -1;
			}
		}
		else
		{
			if (entropy_be_noisebits(dev_dsp_fd, data, size) < 0)
			{
				close(dev_dsp_fd);
				#ifdef _REENTRANT
				# if HAVE_SYNCH_H
				mutex_unlock(&dev_dsp_lock);
				# elif HAVE_PTHREAD_H
				pthread_mutex_unlock(&dev_dsp_lock);
				# else
				#  error need locking mechanism
				# endif
				#endif
				return -1;
			}
		}
	}
	#else
	# error Unknown type of /dev/dsp interface
	#endif

	close(dev_dsp_fd);
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	mutex_unlock(&dev_dsp_lock);
	# elif HAVE_PTHREAD_H
	pthread_mutex_unlock(&dev_dsp_lock);
	# else
	#  error need locking mechanism
	# endif
	#endif
	return 0;
}
#endif

#if HAVE_DEV_RANDOM
int entropy_dev_random(uint32* data, int size)
{
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	if (mutex_lock(&dev_random_lock))
		return -1;
	# elif HAVE_PTHREAD_H
	if (pthread_mutex_lock(&dev_random_lock))
		return -1;
	# else
	#  error need locking mechanism
	# endif
	#endif

	#if HAVE_SYS_STAT_H
	if (statdevice(name_dev_random) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_random_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_random_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}
	#endif
	
	if ((dev_random_fd = opendevice(name_dev_random)) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_random_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_random_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}

	if (entropy_randombits(dev_random_fd, data, size) < 0)
	{
		close(dev_random_fd);
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_random_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_random_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}

	close(dev_random_fd);
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	mutex_unlock(&dev_random_lock);
	# elif HAVE_PTHREAD_H
	pthread_mutex_unlock(&dev_random_lock);
	# else
	#  error need locking mechanism
	# endif
	#endif
	return 0;
}
#endif

#if HAVE_DEV_TTY
int entropy_dev_tty(uint32* data, int size)
{
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	if (mutex_lock(&dev_tty_lock))
		return -1;
	# elif HAVE_PTHREAD_H
	if (pthread_mutex_lock(&dev_tty_lock))
		return -1;
	# else
	#  error need locking mechanism
	# endif
	#endif

	#if HAVE_SYS_STAT_H
	if (statdevice(dev_tty_name) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_tty_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_tty_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}
	#endif
	
	if ((dev_tty_fd = opendevice(dev_tty_name)) < 0)
	{
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_tty_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_tty_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}

	if (entropy_ttybits(dev_tty_fd, data, size) < 0)
	{
		close(dev_tty_fd);
		#ifdef _REENTRANT
		# if HAVE_SYNCH_H
		mutex_unlock(&dev_tty_lock);
		# elif HAVE_PTHREAD_H
		pthread_mutex_unlock(&dev_tty_lock);
		# else
		#  error need locking mechanism
		# endif
		#endif
		return -1;
	}

	close(dev_tty_fd);
	#ifdef _REENTRANT
	# if HAVE_SYNCH_H
	mutex_unlock(&dev_tty_lock);
	# elif HAVE_PTHREAD_H
	pthread_mutex_unlock(&dev_tty_lock);
	# else
	#  error need locking mechanism
	# endif
	#endif
	return 0;
}
#endif
#endif
