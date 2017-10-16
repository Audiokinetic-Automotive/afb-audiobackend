/*
 * Copyright (C) 2017 "Audiokinetic Inc"
 * Author Francois Thibault <fthibault@audiokinetic.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIOD_BINDING_INCLUDE
#define AUDIOD_BINDING_INCLUDE

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>
#include <json-c/json.h>
#include <glib.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <systemd/sd-event.h>

#ifndef PUBLIC
  #define PUBLIC
#endif

#define FRAME_SIZE 1024
#define NB_BUFFER 4
#define MAX_PATH_LENGTH 2048

#define AUDIOD_AUDIO_EVENT "audiod_audio_event"
#define AUDIOD_SYSTEM_EVENT "audiod_system_event"


typedef enum AudioState {
    AUDIO_PLAY = 0,          // Automatic endpoint selection based on config priority
    AUDIO_PAUSE,             // Explicit endpoint selection
    AUDIO_RESUME,
    AUDIO_STOP,             // Explicit endpoint selection
    AUDIO_MAX,            // Enum count, keep at the end
} AudioStateT;

typedef struct WavHeader{
    char     chunk_id[4];
    uint32_t chunk_size;
    char     format[4];
    char     fmtchunk_id[4];
    uint32_t fmtchunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bps;
    char     datachunk_id[4];
    uint32_t datachunk_size;
} WavHeaderT;


// Parts of the context that are visible to the policy (for state based decisions)
typedef struct AudioDClientCtx {
  WavHeaderT *pWaveHeader;
  int16_t *pSample;  
  int long iSampleOffset;  
  snd_pcm_t *pPcmHandle;
  uint16_t iFrameSize;
  uint32_t iTotalNbFrame;
  uint16_t iMsPerFrame;
  bool loopEnable;
  bool isPauseAvailable;
  bool isPause;
  AudioStateT audioState;
  pthread_mutex_t mutex;
  sd_event_source *evtsrc;
  uint32_t iPlayingID;
} AudioDClientCtxT;

typedef struct AudioDCtx {
  uint32_t iGlobalPlayingID;

	struct afb_event AudioEvent;
  struct afb_event SystemEvent;
} AudioDCtxT;




PUBLIC int AudioDBindingInit();
PUBLIC void AudioDOnEvent(const char *evtname, json_object *eventJ);

#endif // AUDIOD_BINDING_INCLUDE
