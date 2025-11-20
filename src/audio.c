#include "audio.h"

#include <mpg123.h>
#include <portaudio.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AudioEngine {
  PaStream *stream;
  float *samples;      // interleaved float32 samples
  size_t sample_count; // total float samples (frames * channels)
  size_t play_cursor;  // current sample index
  long sample_rate;
  int channels;

  float volume;
  bool playing;

  double duration; // seconds
};

static bool g_audio_libs_initialized = false;

static int pa_callback(const void *input, void *output,
                       unsigned long frameCount,
                       const PaStreamCallbackTimeInfo *timeInfo,
                       PaStreamCallbackFlags statusFlags, void *userData) {
  (void)input;
  (void)timeInfo;
  (void)statusFlags;

  AudioEngine *engine = (AudioEngine *)userData;
  float *out = (float *)output;

  unsigned long samples_requested =
      frameCount * (unsigned long)engine->channels;

  for (unsigned long i = 0; i < samples_requested; ++i) {
    if (engine->playing && engine->play_cursor < engine->sample_count) {
      out[i] = engine->samples[engine->play_cursor++] * engine->volume;
    } else {
      out[i] = 0.0f;
    }
  }

  // When we reach the end of buffer, mark as done
  if (engine->play_cursor >= engine->sample_count) {
    engine->playing = false;
    return paComplete;
  }

  return paContinue;
}

AudioEngine *audio_init(void) {
  if (!g_audio_libs_initialized) {
    if (Pa_Initialize() != paNoError) {
      fprintf(stderr, "PortAudio init failed\n");
      return NULL;
    }
    if (mpg123_init() != MPG123_OK) {
      fprintf(stderr, "mpg123 init failed\n");
      Pa_Terminate();
      return NULL;
    }
    g_audio_libs_initialized = true;
  }

  AudioEngine *engine = calloc(1, sizeof(AudioEngine));
  if (!engine) {
    fprintf(stderr, "Failed to allocate AudioEngine\n");
    return NULL;
  }

  engine->volume = 0.7f;
  return engine;
}

void audio_cleanup(AudioEngine *engine) {
  if (!engine)
    return;

  if (engine->stream) {
    Pa_StopStream(engine->stream);
    Pa_CloseStream(engine->stream);
    engine->stream = NULL;
  }

  free(engine->samples);
  free(engine);

  // For a small CLI app, we can skip Pa_Terminate/mpg123_exit here,
  // OS will clean on exit. If you want to be fancy, you can track
  // engine count and call Pa_Terminate/mpg123_exit when last is freed.
}

bool audio_load_file(AudioEngine *engine, const char *filename) {
  if (!engine)
    return false;

  // Stop current playback
  if (engine->stream) {
    Pa_StopStream(engine->stream);
    Pa_CloseStream(engine->stream);
    engine->stream = NULL;
  }
  free(engine->samples);
  engine->samples = NULL;
  engine->sample_count = 0;
  engine->play_cursor = 0;
  engine->duration = 0.0;
  engine->playing = false;

  // ---- Open and configure mpg123 ----
  int err = 0;
  mpg123_handle *mh = mpg123_new(NULL, &err);
  if (!mh) {
    fprintf(stderr, "[audio] mpg123_new failed: %s\n",
            mpg123_plain_strerror(err));
    return false;
  }

  if (mpg123_open(mh, filename) != MPG123_OK) {
    fprintf(stderr, "[audio] mpg123_open failed for %s\n", filename);
    mpg123_delete(mh);
    return false;
  }

  long rate;
  int channels, encoding;
  if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
    fprintf(stderr, "[audio] mpg123_getformat failed\n");
    mpg123_close(mh);
    mpg123_delete(mh);
    return false;
  }

  // Ask for 16-bit signed output, keep the same rate/channels
  mpg123_format_none(mh);
  if (mpg123_format(mh, rate, channels, MPG123_ENC_SIGNED_16) != MPG123_OK) {
    fprintf(stderr, "[audio] mpg123_format failed\n");
    mpg123_close(mh);
    mpg123_delete(mh);
    return false;
  }

  // Re-query actual output format (mpg123 can adjust it)
  if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
    fprintf(stderr, "[audio] mpg123_getformat (after format) failed\n");
    mpg123_close(mh);
    mpg123_delete(mh);
    return false;
  }

  engine->sample_rate = rate;
  engine->channels = channels;

  // Duration from mpg123_length (frames per channel)
  off_t len_per_channel = mpg123_length(mh);
  if (len_per_channel > 0) {
    engine->duration = (double)len_per_channel / (double)rate;
    fprintf(stderr, "[audio] mpg123_length: %lld frames/ch, duration=%.2f s\n",
            (long long)len_per_channel, engine->duration);
  }

  // ---- Decode whole file to 16-bit buffer ----
  unsigned char *buffer = NULL;
  size_t buffer_size = 0;
  size_t capacity = 0;
  size_t done = 0;
  const size_t chunk = 4096;

  while (1) {
    if (capacity - buffer_size < chunk) {
      size_t new_capacity = capacity ? capacity * 2 : 65536;
      unsigned char *new_buf = realloc(buffer, new_capacity);
      if (!new_buf) {
        fprintf(stderr, "[audio] Out of memory while decoding\n");
        free(buffer);
        mpg123_close(mh);
        mpg123_delete(mh);
        return false;
      }
      buffer = new_buf;
      capacity = new_capacity;
    }

    err = mpg123_read(mh, buffer + buffer_size, chunk, &done);
    buffer_size += done;

    if (err == MPG123_DONE)
      break;
    if (err != MPG123_OK && err != MPG123_NEW_FORMAT) {
      fprintf(stderr, "[audio] mpg123_read error: %s\n", mpg123_strerror(mh));
      free(buffer);
      mpg123_close(mh);
      mpg123_delete(mh);
      return false;
    }
  }

  mpg123_close(mh);
  mpg123_delete(mh);

  // buffer now has 16-bit signed samples
  size_t sample_count_int16 =
      buffer_size / sizeof(short); // total ints, all channels
  short *sbuf = (short *)buffer;

  // Convert to float in [-1, 1]
  engine->samples = malloc(sample_count_int16 * sizeof(float));
  if (!engine->samples) {
    fprintf(stderr, "[audio] Out of memory for float samples\n");
    free(buffer);
    return false;
  }

  for (size_t i = 0; i < sample_count_int16; ++i) {
    engine->samples[i] = (float)sbuf[i] / 32768.0f;
  }
  free(buffer);

  engine->sample_count =
      sample_count_int16; // number of float samples (frames * channels)
  engine->play_cursor = 0;

  // Sanity: compute duration from buffer
  size_t frames = sample_count_int16 / (size_t)channels;
  double dur_from_buffer = (double)frames / (double)rate;
  fprintf(stderr,
          "[audio] rate=%ld, channels=%d, total_samples=%zu, frames=%zu, "
          "dur_from_buffer=%.2f s\n",
          rate, channels, sample_count_int16, frames, dur_from_buffer);

  // If mpg123_length failed, use fallback
  if (engine->duration <= 0.0) {
    engine->duration = dur_from_buffer;
  }

  // ---- Setup PortAudio stream as before ----
  PaStreamParameters outParams;
  memset(&outParams, 0, sizeof(outParams));
  outParams.device = Pa_GetDefaultOutputDevice();
  if (outParams.device == paNoDevice) {
    fprintf(stderr, "[audio] No default output device.\n");
    return false;
  }

  const PaDeviceInfo *info = Pa_GetDeviceInfo(outParams.device);
  fprintf(stderr, "[audio] using device: %s\n", info ? info->name : "(null)");

  outParams.channelCount = channels;
  outParams.sampleFormat = paFloat32;
  outParams.suggestedLatency = info->defaultLowOutputLatency;
  outParams.hostApiSpecificStreamInfo = NULL;

  PaError paErr = Pa_OpenStream(&engine->stream, NULL, &outParams, (double)rate,
                                paFramesPerBufferUnspecified, paClipOff,
                                pa_callback, engine);
  if (paErr != paNoError) {
    fprintf(stderr, "[audio] Pa_OpenStream failed: %s\n",
            Pa_GetErrorText(paErr));
    engine->stream = NULL;
    return false;
  }

  paErr = Pa_StartStream(engine->stream);
  if (paErr != paNoError) {
    fprintf(stderr, "[audio] Pa_StartStream failed: %s\n",
            Pa_GetErrorText(paErr));
    Pa_CloseStream(engine->stream);
    engine->stream = NULL;
    return false;
  }

  return true;
}

void audio_play(AudioEngine *engine) {
  if (!engine || !engine->stream)
    return;
  engine->playing = true;
}

void audio_pause(AudioEngine *engine) {
  if (!engine || !engine->stream)
    return;
  engine->playing = false;
}

void audio_stop(AudioEngine *engine) {
  if (!engine)
    return;
  engine->playing = false;
  engine->play_cursor = 0;
}

void audio_set_volume(AudioEngine *engine, float volume) {
  if (!engine)
    return;
  if (volume < 0.0f)
    volume = 0.0f;
  if (volume > 1.0f)
    volume = 1.0f;
  engine->volume = volume;
}

double audio_get_position(AudioEngine *engine) {
  if (!engine || engine->sample_rate == 0 || engine->channels == 0)
    return 0.0;

  size_t frames_played = engine->play_cursor / (size_t)engine->channels;
  return (double)frames_played / (double)engine->sample_rate;
}

double audio_get_duration(AudioEngine *engine) {
  if (!engine)
    return 0.0;
  return engine->duration;
}

bool audio_is_playing(AudioEngine *engine) {
  if (!engine)
    return false;
  return engine->playing;
}
