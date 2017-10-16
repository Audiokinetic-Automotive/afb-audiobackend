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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
//	
//#include <alsa/asoundlib.h>

#include "audiod-binding.h"
#include "audiod-apidef.h" // Generated from JSON OpenAPI
#include "wrap-json.h"

AudioDCtxT g_audioCtx;

static void AudioDBindingTerm()
{



    AFB_INFO("Audio backend Binding succesTermination");
}

// Binding initialization
PUBLIC int AudioDBindingInit()
{

	atexit(AudioDBindingTerm);

    int errcount = 0;
	g_audioCtx.AudioEvent = afb_daemon_make_event("audio_events");
    int err = !afb_event_is_valid(g_audioCtx.AudioEvent);
    if (err) {
        AFB_ERROR("Could not create Audio event");
        err++;
    }

	g_audioCtx.SystemEvent = afb_daemon_make_event("system_events");
    err = !afb_event_is_valid(g_audioCtx.SystemEvent);
    if (err) {
        AFB_ERROR("Could not create Audio event");
        err++;
    }

    AFB_DEBUG("Audio Backend Binding success errcount=%d", errcount);
    return errcount;
}



PUBLIC void AudioDOnEvent(const char *evtname, json_object *eventJ)
{
    // TODO: Implement handling events
    AFB_DEBUG("Audio Backend received event %s", evtname);
}

static int alsa_error_recovery(void *in_puserdata, snd_pcm_t *in_phandle, int in_err)
{
	AudioDClientCtxT *  pCtx = (AudioDClientCtxT *)in_puserdata;
	int returnVal=0;

	if (in_err == -EPIPE) //Underrun
	{
		returnVal = snd_pcm_prepare(in_phandle);
		if (returnVal < 0)
		{
			AFB_ERROR("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(returnVal));

		}

		return returnVal;
	}
	else if (in_err == -ESTRPIPE) //Suspend
	{
		AFB_ERROR("Stream is suspended");
		while ((returnVal = snd_pcm_resume(in_phandle)) == -EAGAIN)
		{
			sleep(1);       /* wait until the suspend flag is released */
		}

		if (returnVal < 0)
		{
			returnVal = snd_pcm_prepare(in_phandle);
			if (returnVal < 0)
			{
				AFB_ERROR("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(returnVal));
			}

		}

		return returnVal;
	}

	return in_err;
}

int stopAlsaStream(AudioDClientCtxT *pCtx)
{
	if (pCtx->pPcmHandle)
	{

		AFB_DEBUG("Stopping Alsa Stream");
		snd_pcm_drain(pCtx->pPcmHandle);
		snd_pcm_close(pCtx->pPcmHandle);
		pCtx->pPcmHandle = NULL;		
	}

	if(pCtx->pSample)	
	{
		free(pCtx->pSample);
		pCtx->pSample = NULL;
		pCtx->iSampleOffset = 0;
		pCtx->evtsrc = NULL;		
	}

	if(pCtx->pWaveHeader)
	{
		free(pCtx->pWaveHeader);
		pCtx->pWaveHeader = NULL;
	}
	
	pCtx->iPlayingID =0;
	pCtx->loopEnable = false;
	pCtx->isPauseAvailable = false;
	pCtx->isPause = false;

}


//Callback to check audio avaibility and wake up Wwise audio thread
int asyncAudioCb(sd_event_source* source, uint64_t timer, void* handle)
{
	

	if(handle!=NULL)
	{
		AudioDClientCtxT *pCtx = (AudioDClientCtxT*)handle;

		
		uint64_t usec;
		snd_pcm_t *pPcmHandle = pCtx->pPcmHandle;
		snd_pcm_sframes_t frameAvail;
		int returnVal;

		switch(pCtx->audioState)
		{
			case AUDIO_STOP:
				stopAlsaStream(pCtx);
				return 0;
				break;
			case AUDIO_PAUSE:
				if(!pCtx->isPause)
				{
					if(pCtx->isPauseAvailable)
					{
						returnVal = snd_pcm_pause(pCtx->pPcmHandle, 1);
						if(returnVal!=0)
						{
							AFB_ERROR("Cannot pause");						
						}

					}
					else
					{
						returnVal = snd_pcm_drain(pCtx->pPcmHandle);
						if(returnVal!=0)
						{
							AFB_ERROR("Cannot pause");
						}

					}
					pCtx->isPause = true;
				}
				

				break;
			case AUDIO_RESUME:
				if(pCtx->isPauseAvailable)
				{
					returnVal = snd_pcm_pause(pCtx->pPcmHandle, 0);
					if(returnVal!=0)
					{
						AFB_ERROR("Cannot prepare");						
					}
				}
				else
				{
					returnVal = snd_pcm_prepare(pCtx->pPcmHandle);
					if(returnVal!=0)
					{
						AFB_ERROR("Cannot prepare");
					}

				}
				pCtx->isPause = false;
				pCtx->audioState = AUDIO_PLAY;

				break;				
			case AUDIO_PLAY:

				//Check how much buffer space we have
				frameAvail = snd_pcm_avail_update(pPcmHandle);
				if(frameAvail< 0)
				{
					returnVal = alsa_error_recovery(pCtx, pPcmHandle, (int)frameAvail);
					if(returnVal < 0)
					{
						AFB_WARNING("Alsa unable to recover from error");
						frameAvail = 0;
					}
					else
					{
						//Check again how much space is available
						frameAvail = snd_pcm_avail_update(pPcmHandle);
						if(frameAvail<0)
						{
							//After prepare not supposed to underrun again
							AFB_WARNING("Alsa stream multiple underrun, problem detected");
							frameAvail = 0;
						}
						
					}
				}
				else
				{	

					int16_t *pCurrentSample = pCtx->pSample+pCtx->iSampleOffset*pCtx->pWaveHeader->num_channels;
					snd_pcm_sframes_t NbFrameToWrite;

					if((pCtx->iSampleOffset+(frameAvail)) <pCtx->iTotalNbFrame)
					{				
						NbFrameToWrite = frameAvail;
					}
					else
					{
						NbFrameToWrite = pCtx->iTotalNbFrame - pCtx->iSampleOffset; 

					}

					snd_pcm_sframes_t numberFrameWritten = snd_pcm_writei(pPcmHandle,pCurrentSample, NbFrameToWrite);
					if(numberFrameWritten < 0)
					{
						returnVal = alsa_error_recovery(pCtx, pPcmHandle, (int)frameAvail);				
						if(returnVal<0)
						{
							AFB_ERROR("Cannot recover from error");
						}
					}
					else
					{
						pCtx->iSampleOffset += (numberFrameWritten);
						if(pCtx->iSampleOffset ==pCtx->iTotalNbFrame)
						{	
							if(pCtx->loopEnable == TRUE)				
							{
								pCtx->iSampleOffset = 0;
							}
							else
							{
								pCtx->audioState = AUDIO_STOP;
								json_object *jresp = json_object_new_object();    							
								json_object_object_add(jresp, "EventName", json_object_new_string("endofstream"));
    							json_object_object_add(jresp, "PlayingID", json_object_new_int(pCtx->iPlayingID));
								afb_event_push(g_audioCtx.AudioEvent,jresp);
							}
							
						}

					}
					
					
				}

				break;
			default:
				break;
		}

		sd_event_now(afb_daemon_get_event_loop(), CLOCK_MONOTONIC, &usec);
		sd_event_source_set_enabled(source, SD_EVENT_ONESHOT);
		sd_event_source_set_time(source, usec + pCtx->iMsPerFrame*1000);	
		//pthread_mutex_unlock( &pCtx->mutex );
	}

	return 0;
}


static int alsa_stream_connect(AudioDClientCtxT *pCtx, char* pDeviceName)
{
	int retValue;

	// Open PCM. The last parameter of this function is the mode.
	// If this is set to 0, the standard mode is used. Possible
	// other values are SND_PCM_NONBLOCK and SND_PCM_ASYNC.
	// If SND_PCM_NONBLOCK is used, read / write access to the
	// PCM device will return immediately. If SND_PCM_ASYNC is
	// specified, SIGIO will be emitted whenever a period has
	// been completely processed by the soundcard.	
	retValue = snd_pcm_open(&pCtx->pPcmHandle, pDeviceName, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (retValue  < 0)
	{
	  AFB_ERROR("Error opening PCM device %s", pDeviceName);
	  return retValue;
	}
	
	

	snd_pcm_hw_params_t *pHwParams;

    // Init hwparams with full configuration space
	snd_pcm_hw_params_alloca(&pHwParams);

	retValue = snd_pcm_hw_params_any(pCtx->pPcmHandle, pHwParams);
    if (retValue < 0)
    {
      AFB_ERROR("Can not configure this PCM device");
      return retValue;
    }

    //Get Wwise sample rate
    unsigned int uRate = pCtx->pWaveHeader->sample_rate;

    //Variable to stored exact rate return by ALSA
    unsigned int uSampleRate;

    //Set access type
    //Possible value are SND_PCM_ACCESS_RW_INTERLEAVED or SND_PCM_ACCESS_RW_NONINTERLEAVED
    retValue = snd_pcm_hw_params_set_access(pCtx->pPcmHandle, pHwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (retValue < 0)
	{
	  AFB_ERROR("Error setting access");
	  return retValue;
	}

	// Set sample format
	retValue = snd_pcm_hw_params_set_format(pCtx->pPcmHandle, pHwParams, SND_PCM_FORMAT_S16_LE);
	if(retValue < 0)
	{
	  AFB_ERROR("int16 not support by hardware");
	  return retValue;
	}

	// Set sample rate. If the exact rate is not supported
	// by the hardware, use nearest possible rate.
	// Use near to be able to see hardware closest supported rate even if we only accept the exact rate
	uSampleRate = uRate;
	retValue = snd_pcm_hw_params_set_rate_near(pCtx->pPcmHandle, pHwParams, &uSampleRate, 0);
	if (retValue  < 0)
	{
	  AFB_ERROR("Error setting rate");
	  return retValue;
	}

	if (uRate != uSampleRate)
	{
		AFB_ERROR("The rate %d Hz is not supported by your hardware.\n ==> Please use %d Hz instead.", uRate, uSampleRate);
		return retValue;
	}

	snd_pcm_chmap_query_t **ppChMaps;
	snd_pcm_chmap_t *FoundChMap;

	ppChMaps = snd_pcm_query_chmaps(pCtx->pPcmHandle);
	if (ppChMaps == NULL)
	{
		AFB_WARNING("Unable to detect number of audio channels supported by the Device. Stereo mode is selected by default");
	}
	else
	{
		int i=0;
		if(pCtx->pWaveHeader->num_channels > 1) //mono
		{
			FoundChMap = &ppChMaps[i]->map;

			//Search for request nb of channels and report error if no config match with number of channel.
			bool bChannelMapConfigFound = false;

			snd_pcm_chmap_query_t *pChMap = ppChMaps[i];

			//ppChMaps is always terminated with NULL
			while(pChMap!=NULL)
			{
				if(pChMap->map.channels == pCtx->pWaveHeader->num_channels)
				{
					bChannelMapConfigFound = true;
					FoundChMap = &ppChMaps[i]->map;
					pChMap = NULL;
				}
				else
				{
					i++;
					pChMap = ppChMaps[i];
				}
			}

			if(bChannelMapConfigFound == false)
			{
				AFB_ERROR("Audio device does not support %i audio channels", pCtx->pWaveHeader->num_channels);
				return -1;
			}		
		}
			
	}

	// Set number of channels
	retValue = snd_pcm_hw_params_set_channels(pCtx->pPcmHandle,pHwParams,pCtx->pWaveHeader->num_channels );
	if (retValue < 0)
	{
	  AFB_ERROR("Unable to set request number of channels :%i", pCtx->pWaveHeader->num_channels);
	  return retValue;
	}

	unsigned int period_min;
	unsigned int period_max;
	snd_pcm_uframes_t period_size_min;
	snd_pcm_uframes_t period_size_max;

	snd_pcm_uframes_t buffer_size_min;
	snd_pcm_uframes_t buffer_size_max;

	//Get usefull HW parameters
	retValue = snd_pcm_hw_params_get_periods_min(pHwParams, &period_min,0);
	if (retValue  < 0)
	{
	  AFB_WARNING("Unable to get the minimum periods size");
	  period_min = 0;
	}

	retValue = snd_pcm_hw_params_get_periods_max(pHwParams, &period_max,0);
	if (retValue < 0)
	{
	  AFB_WARNING("Error getting the minimum periods size");
	  period_max = 0;
	}

	retValue = snd_pcm_hw_params_get_period_size_min(pHwParams, &period_size_min,0);
	if (retValue < 0)
	{
	  AFB_WARNING("Error getting the minimum periods size");
	  period_size_min = 0;
	}

	retValue = snd_pcm_hw_params_get_period_size_max(pHwParams, &period_size_max,0);
	if (retValue < 0)
	{
	  AFB_WARNING("Error getting the minimum periods size");
	  period_size_max = 0;
	}

	retValue = snd_pcm_hw_params_get_buffer_size_min(pHwParams, &buffer_size_min);
	if (retValue < 0)
	{
	  AFB_WARNING("Error getting the minimum periods size");
	  buffer_size_min = 0;
	}

	retValue = snd_pcm_hw_params_get_buffer_size_max(pHwParams, &buffer_size_max);
	if (retValue < 0) {
	  AFB_WARNING("Error getting the minimum periods size");
	  buffer_size_max = 0;
	}

	if(snd_pcm_hw_params_can_pause(pHwParams))
	{
		pCtx->isPauseAvailable = true;
	}
	else
	{	
		pCtx->isPauseAvailable = false;
	}

	

	//buffer_size = periodsize * periods
	//Latency important parameters
	//The latency is calculated as follow:
	//latency = periodsize*period / (rate * bytes_per_frame)
	//Typical example is as follow:
	//period=2; periodsize=8192 bytes; rate=48000Hz; 16 bits stereo data (bytes_per_frame=4bytes)
	//latency = 8192*2/(48000*4) = 85.3 ms of latency	
	snd_pcm_uframes_t buffer_size = (snd_pcm_uframes_t)(pCtx->iFrameSize)*NB_BUFFER; //hardcode value for now

	//Set the buffer size
	retValue = snd_pcm_hw_params_set_buffer_size_near (pCtx->pPcmHandle, pHwParams, &buffer_size);
	if (retValue  < 0) {
	  AFB_ERROR("Error setting buffer size");
	  return retValue;
	}

	unsigned int period = (unsigned int)buffer_size/pCtx->iFrameSize;
	if(period == 0)
	{
		period = 1;
	}

	//Periods
	retValue = snd_pcm_hw_params_test_periods(pCtx->pPcmHandle, pHwParams, period, 0);
	if(retValue < 0)
	{
		AFB_WARNING("Request %i period not supported by hardware, will set to the nearest possible value, the latency will be affected",period);

	}

	retValue = snd_pcm_hw_params_set_periods_near(pCtx->pPcmHandle, pHwParams, &period, 0);
	if (retValue < 0)
	{
		AFB_ERROR("Error setting period size");
		return retValue;
	}

	// Apply HW parameter and prepare device
	retValue = snd_pcm_hw_params(pCtx->pPcmHandle, pHwParams);
    if (retValue < 0)
    {
    	AFB_ERROR("Error setting PCM HW PARAMS");
    	return retValue;
    }

    retValue = snd_pcm_hw_params_get_period_size_min(pHwParams, &period_size_min,0);
	if (retValue < 0)
	{
	  AFB_WARNING("Error getting the minimum periods size");
	}

	retValue = snd_pcm_hw_params_get_period_size_max(pHwParams, &period_size_max,0);
	if (retValue < 0)
	{
	  AFB_WARNING("Error getting the minimum periods size");
	}

	if(ppChMaps!= NULL)
    {
    	//Wwise standard mapping L-R-C-LFE-RL-RR-RC-SL-SR-HL-HR-HC-HRL-HRR-HRC-T
		//In order to support up to 16 channels
		//Always open the stream with Wwise Standard channel mapping, since it does not change based on channel number
		//The we use m_pChannelMap to remap into run time mapping.
		//Since the remapping does not cost an extra memcpy this solution should keep the same performance.
		//Another solution will be to directly map into Wwise runtime mapping but this has a lot of case to handle.
		if(pCtx->pWaveHeader->num_channels > 1) //Channel map only for stereo and up.
		{

			for(unsigned int y=0; y<pCtx->pWaveHeader->num_channels; y++)
			{
				switch(y)
				{
					case 0:
							FoundChMap->pos[y] = SND_CHMAP_FL;
							break;
					case 1:
							FoundChMap->pos[y]  = SND_CHMAP_FR;
							break;
					case 2:
							FoundChMap->pos[y]  = SND_CHMAP_FC;
							break;
					case 3:
							FoundChMap->pos[y]  = SND_CHMAP_LFE;
							break;
					case 4:
							FoundChMap->pos[y]  = SND_CHMAP_RL;
							break;
					case 5:
							FoundChMap->pos[y]  = SND_CHMAP_RR;
							break;
					case 6:
							FoundChMap->pos[y]  = SND_CHMAP_RC;
							break;
					case 7:
							FoundChMap->pos[y]  = SND_CHMAP_SL;
							break;
					case 8:
							FoundChMap->pos[y]  = SND_CHMAP_SR;
							break;
					case 9:
							FoundChMap->pos[y]  = SND_CHMAP_TFL;
							break;
					case 10:
							FoundChMap->pos[y]  = SND_CHMAP_TFR;
							break;
					case 11:
							FoundChMap->pos[y]  = SND_CHMAP_TFC;
							break;
					case 12:
							FoundChMap->pos[y]  = SND_CHMAP_TRL;
							break;
					case 13:
							FoundChMap->pos[y]  = SND_CHMAP_TRR;
							break;
					case 14:
							FoundChMap->pos[y]  = SND_CHMAP_TRC;
							break;
					case 15:
							FoundChMap->pos[y]  = SND_CHMAP_TC;
							break;
				}
			}

			retValue =  snd_pcm_set_chmap(pCtx->pPcmHandle, FoundChMap);
			if(retValue  <0)
			{
				AFB_WARNING("Unable to set channel mapping, your channel mapping is not set. Please check your device name.");
			}

		}

		//Free channel maps
		snd_pcm_free_chmaps	(ppChMaps);
    }


    //SW params setting
	snd_pcm_sw_params_t *pSwparams;
	retValue = snd_pcm_sw_params_malloc(&pSwparams);
	if (retValue < 0)
	{
		AFB_ERROR("Could not allocate ALSA SW parameters");
		return retValue;
	}

    retValue = snd_pcm_sw_params_current(pCtx->pPcmHandle, pSwparams);
	if(retValue < 0)
	{
		AFB_ERROR("Error gettting the current sw params");
		return retValue;
	}

	//Set start value, minimum is 2 frames
	retValue = snd_pcm_sw_params_set_start_threshold(pCtx->pPcmHandle, pSwparams, pCtx->iFrameSize*2);
	if( retValue < 0)
	{
		AFB_ERROR("Error setting the start threashold");
		return retValue;
	}

	retValue = snd_pcm_sw_params_set_avail_min(pCtx->pPcmHandle, pSwparams, pCtx->iFrameSize);
	if(retValue < 0)
	{
		AFB_ERROR("Error setting avail minimum");
		return retValue;
	}

	retValue = snd_pcm_sw_params(pCtx->pPcmHandle, pSwparams);
	if(retValue < 0)
	{
		AFB_ERROR("Error writing sw params");
		return retValue;
	}

	//Init stream
	retValue = snd_pcm_prepare (pCtx->pPcmHandle);
	if (retValue  < 0)
	{
		AFB_ERROR("cannot prepare audio interface for use");
		return retValue;
	}

    return 0;

}


static AudioDClientCtxT * AllocateClientContext()
{
    AudioDClientCtxT * pClientCtx = malloc(sizeof(AudioDClientCtxT));
	pClientCtx->pSample = NULL;
	pClientCtx->iSampleOffset = 0;
	pClientCtx->pPcmHandle = NULL;
	pClientCtx->pWaveHeader = NULL;		
	pthread_mutex_init ( &pClientCtx->mutex, NULL);
	pClientCtx->iFrameSize = FRAME_SIZE;
	pClientCtx->iSampleOffset = 0;
	pClientCtx->audioState = AUDIO_STOP;
	pClientCtx->loopEnable = false;
	pClientCtx->iPlayingID = 0;
	pClientCtx->isPauseAvailable = false;
	pClientCtx->isPause = false;
    return pClientCtx;
}

static void TerminateClientContext(void * ptr)
{

	AudioDClientCtxT * pClientCtx = (AudioDClientCtxT *) ptr;

	if(pClientCtx->pPcmHandle != NULL)
	{
		stopAlsaStream(pClientCtx);
	}
	
    free(pClientCtx);
}




int wavread(AudioDClientCtxT *pCtx, char *pFileName)
{
	
	
	char filePath[MAX_PATH_LENGTH];
	sprintf(filePath, "%s/%s", getenv("MEDIA_LOCATION"), pFileName);


	//Open WaveFile
	FILE * pFileHanle = fopen(filePath, "rb");
	if(pFileHanle == NULL)
	{
		AFB_ERROR("Filename : %s can't be opened", filePath);
		return -1;
	}

	//Allocate memory
	pCtx->pWaveHeader = (WavHeaderT*)malloc(sizeof(WavHeaderT));
	if(pCtx->pWaveHeader== NULL)
	{
		AFB_ERROR("Can't allocate memory for waveHeader");
		return -1;

	}

	fseek(pFileHanle, 0, SEEK_END); // seek to end of file
	long int iFileSize = ftell(pFileHanle); // get current file pointer
	fseek(pFileHanle, 0, SEEK_SET);

	if(iFileSize< sizeof(WavHeaderT))
	{
		AFB_ERROR("WaveFile is too small");
		return -1;
	}

	//Read Wave Header
	size_t iNumberByte = fread(pCtx->pWaveHeader, 1, sizeof(WavHeaderT), pFileHanle);

	if( iNumberByte < (sizeof(WavHeaderT)))
	{
		AFB_ERROR("Corrupt WaveFile Header");
		return -1;

	}

	if(strncmp(pCtx->pWaveHeader->chunk_id, "RIFF", 4) || strncmp(pCtx->pWaveHeader->format, "WAVE", 4))
	{
		AFB_ERROR("File is not a wave file");
		return -1;
	}

	if(pCtx->pWaveHeader->audio_format != 1)
	{
		AFB_ERROR("Only PCM encoding is supported");
		return -1;
	}

	if (pCtx->pPcmHandle)
	{
		free(pCtx->pPcmHandle);
	}

	int DESIRED_FPS=0;
	switch (pCtx->iFrameSize)
	{
		case 2048:
			DESIRED_FPS = 25;
			break;
		case 1024:
			DESIRED_FPS = 50;
			break;
		case 512:
		default:
			DESIRED_FPS = 100;
			break;
		case 256:
			DESIRED_FPS = 200;
			break;
	}
	pCtx->iMsPerFrame = 1000/DESIRED_FPS;

	pCtx->iTotalNbFrame = pCtx->pWaveHeader->datachunk_size/(sizeof(int16_t)*pCtx->pWaveHeader->num_channels);
	
    pCtx->pSample = (int16_t*)malloc((pCtx->pWaveHeader->datachunk_size/pCtx->pWaveHeader->num_channels) *sizeof(int16_t));
	if(pCtx->pSample == NULL)
	{
		AFB_ERROR("Unable to allocate memory for PCM buffer");
		return -1;
	}

	int iNumberSample = fread(pCtx->pSample, sizeof(int16_t), pCtx->iTotalNbFrame*pCtx->pWaveHeader->num_channels, pFileHanle);
	if(iNumberSample < pCtx->iTotalNbFrame*pCtx->pWaveHeader->num_channels)
	{
		AFB_ERROR("Wave file read data failed");
		return -1;
	}

	pCtx->iSampleOffset = 0;

    fclose(pFileHanle);
	
	return 0;

}

PUBLIC void audiod_play(struct afb_req req)
{

    json_object *queryJ = NULL;
    char * deviceName = NULL;
    char * mediaFilePath = NULL;
	int loopValue=0;

    queryJ = afb_req_json(req);
    int err = wrap_json_unpack(queryJ, "{s:s,s:s,s:i}", "device_name", &deviceName, "filepath", &mediaFilePath,"loop", &loopValue);
    if (err) {
        afb_req_fail_f(req, "Invalid arguments", "Args not a valid json object query=%s", json_object_get_string(queryJ));
        return;
    }


    // Check if there is already an existing context for this client
    AudioDClientCtxT * pClientCtx = afb_req_context_get(req); // Retrieve client-specific data structure
    if (pClientCtx == NULL)
    {
        pClientCtx = AllocateClientContext();
        afb_req_context_set(req, pClientCtx, TerminateClientContext);
    }
	

	if(loopValue==1)
	{
		pClientCtx->loopEnable = true;
	}
	else
	{
		pClientCtx->loopEnable = false;

	}

	switch(pClientCtx->audioState)
	{
		case AUDIO_STOP:
			err = wavread(pClientCtx, mediaFilePath);
			if(err)
			{
        		afb_req_fail_f(req, "wavread failed", "wavread failed with return=%i", err);
        		return;
			}

			err= alsa_stream_connect(pClientCtx,deviceName);
			if(err)
			{
				afb_req_fail_f(req, "alsa_stream_connect failed", "alsa_stream_connect failed with return=%i", err);
        		return;
			}

			uint64_t usec;
			// set a timer with ~250us accuracy
			sd_event_now(afb_daemon_get_event_loop(), CLOCK_MONOTONIC, &usec);
			sd_event_add_time(afb_daemon_get_event_loop(), &pClientCtx->evtsrc, CLOCK_MONOTONIC, usec, 250, asyncAudioCb, pClientCtx);
			pClientCtx->audioState = AUDIO_PLAY;
			pClientCtx->iPlayingID = g_audioCtx.iGlobalPlayingID;
			g_audioCtx.iGlobalPlayingID += 1;
			break;

		case AUDIO_PAUSE:
			pClientCtx->audioState = AUDIO_RESUME;
			break;
		case AUDIO_PLAY:
			break;
		default:
			break;
	}


    AFB_DEBUG("Request Playing Song: %s on Device: %s", mediaFilePath,deviceName);

    json_object *jresp = json_object_new_object();

    json_object_object_add(jresp, "PlayingID", json_object_new_int((int)pClientCtx->iPlayingID));

    afb_req_success(req, jresp, "audiod_play");
}

PUBLIC void audiod_stop(struct afb_req req)
{
    json_object *queryJ = NULL;      
    AFB_DEBUG("Request Playing Stop");
	
	// Check if there is already an existing context for this client
    AudioDClientCtxT * pClientCtx = afb_req_context_get(req); // Retrieve client-specific data structure
    if (pClientCtx == NULL)
    {
        pClientCtx = AllocateClientContext();
        afb_req_context_set(req, pClientCtx, TerminateClientContext);
    }

	pClientCtx->audioState = AUDIO_STOP;
    afb_req_success(req, NULL, "audiod_stop");
}


PUBLIC void audiod_pause(struct afb_req req)
{
    json_object *queryJ = NULL;
   
    AFB_DEBUG("Request Playing Pause");
	
	// Check if there is already an existing context for this client
    AudioDClientCtxT * pClientCtx = afb_req_context_get(req); // Retrieve client-specific data structure
    if (pClientCtx == NULL)
    {
        pClientCtx = AllocateClientContext();
        afb_req_context_set(req, pClientCtx, TerminateClientContext);
    }

	pClientCtx->audioState = AUDIO_PAUSE;
    afb_req_success(req, NULL, "audiod_pause");
}


PUBLIC void audiod_post_event(struct afb_req req)
{
    json_object *queryJ = NULL;
	char *eventName = NULL;
	json_object *event_parameter = NULL;

    queryJ = afb_req_json(req);
    int err = wrap_json_unpack(queryJ, "{s:s,s:o}", "event_name", &eventName, "event_parameter", &event_parameter);
    if (err) {        
		afb_req_fail_f(req, "Invalid arguments", "Args not a valid json object query=%s", json_object_get_string(queryJ));
        return;
    }

    AFB_DEBUG("Audiod post event");

	afb_event_push(g_audioCtx.SystemEvent,queryJ);

    afb_req_success(req, NULL, "audiod_post_event");
}


PUBLIC void audiod_subscribe(struct afb_req req)
{
    json_object *queryJ = NULL;
	json_object * eventArrayJ = NULL;

	queryJ = afb_req_json(req);
    int err = wrap_json_unpack(queryJ, "{s:o}", "events", &eventArrayJ);
    if (err) {
        afb_req_fail_f(req, "Invalid arguments", "Args not a valid json object query=%s", json_object_get_string(queryJ));
        return;
    }
    
    int iNumEvents = json_object_array_length(eventArrayJ);
    for (int i = 0; i < iNumEvents; i++)
    {
        char * pEventName = NULL;
        json_object * jEvent = json_object_array_get_idx(eventArrayJ,i);
        pEventName = (char *)json_object_get_string(jEvent);
        if(!strcasecmp(pEventName, AUDIOD_AUDIO_EVENT)) {
			afb_req_subscribe(req, g_audioCtx.AudioEvent);
			AFB_DEBUG("Client subscribed to audio events");
		}
        else if(!strcasecmp(pEventName, AUDIOD_SYSTEM_EVENT)) {
			afb_req_subscribe(req, g_audioCtx.SystemEvent);
			AFB_DEBUG("Client subscribed to system events");
		}
        else {
			afb_req_fail(req, "failed", "Invalid event");
			return;
		}
    }




    afb_req_success(req, NULL, "Subscribe to audio event");
}

PUBLIC void audiod_unsubscribe(struct afb_req req)
{
    json_object *queryJ = NULL;
	json_object * eventArrayJ = NULL;

	queryJ = afb_req_json(req);
    int err = wrap_json_unpack(queryJ, "{s:o}", "events", &eventArrayJ);
    if (err) {
        afb_req_fail_f(req, "Invalid arguments", "Args not a valid json object query=%s", json_object_get_string(queryJ));
        return;
    }
    
    int iNumEvents = json_object_array_length(eventArrayJ);
    for (int i = 0; i < iNumEvents; i++)
    {
        char * pEventName = NULL;
        json_object * jEvent = json_object_array_get_idx(eventArrayJ,i);
        pEventName = (char *)json_object_get_string(jEvent);
        if(!strcasecmp(pEventName, AUDIOD_AUDIO_EVENT)) {
			afb_req_unsubscribe(req, g_audioCtx.AudioEvent);
			AFB_DEBUG("Client unsubscribed to audio events");
		}
        else if(!strcasecmp(pEventName, AUDIOD_SYSTEM_EVENT)) {
			afb_req_unsubscribe(req, g_audioCtx.SystemEvent);
			AFB_DEBUG("Client unsubscribed to system events");
		}
        else {
			afb_req_fail(req, "failed", "Invalid event");
			return;
		}
    }


    afb_req_success(req, NULL, "Unsubscribe to audio events");
}