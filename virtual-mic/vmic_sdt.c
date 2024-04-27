/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <unistd.h>
#include <fcntl.h>    /* For O_RDWR */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "vmic_sdt_private.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <alsa/asoundlib.h>

#define PCM_DEVICE "hw:0,1,4"

static unsigned int rec_size = 0;
static unsigned int pcm, tmp, dir;
static unsigned int ptime;
static unsigned int rate = 16000;
static unsigned int channels = 1;
static snd_pcm_t *pcm_handle = NULL;
static snd_pcm_hw_params_t *params;
static snd_pcm_sw_params_t *sw_params;
static snd_pcm_uframes_t frames;
static snd_pcm_uframes_t period_size = 1870;
char audio_buffer[3640];
#define VMIC_SDT_IDENTIFIER (0xC11FB9C2)

static bool     vmic_sdt_object_is_valid(vmic_sdt_obj_t *obj);
static uint64_t vmic_sdt_time_get(void);
static void vmic_sdt_handler_session_begin(void *data, const uuid_t uuid, xrsr_src_t src, uint32_t dst_index, xrsr_keyword_detector_result_t *detector_result, xrsr_session_config_out_t *config_out, xrsr_session_config_in_t *config_in, rdkx_timestamp_t *timestamp, const char *transcription_in);
static void vmic_sdt_handler_session_end(void *data, const uuid_t uuid, xrsr_session_stats_t *stats, rdkx_timestamp_t *timestamp);
static void vmic_sdt_handler_stream_begin(void *data, const uuid_t uuid, xrsr_src_t src, rdkx_timestamp_t *timestamp);
static void vmic_sdt_handler_stream_kwd(void *data, const uuid_t uuid, rdkx_timestamp_t *timestamp);
static void vmic_sdt_handler_stream_end(void *data, const uuid_t uuid, xrsr_stream_stats_t *stats, rdkx_timestamp_t *timestamp);
static bool vmic_sdt_handler_connected(void *data, const uuid_t uuid, xrsr_handler_send_t send, void *param, rdkx_timestamp_t *timestamp);
static void vmic_sdt_handler_disconnected(void *data, const uuid_t uuid, xrsr_session_end_reason_t reason, bool retry, bool *detect_resume, rdkx_timestamp_t *timestamp);
static int vmic_recv_audiodata(unsigned char* frame,uint32_t sample_qty);
static int vmic_alsa_buffer_playback(unsigned char* audiodata);
static void vmic_init();
static void vmic_close();



bool vmic_sdt_object_is_valid(vmic_sdt_obj_t *obj) {
   if(obj != NULL && obj->identifier == VMIC_SDT_IDENTIFIER) {
      return(true);
   }
   return(false);
}


vmic_sdt_object_t vmic_sdt_create(const vmic_sdt_params_t *params)
{
   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)malloc(sizeof(vmic_sdt_obj_t));

   if(obj == NULL) {
      XLOGD_ERROR("Out of memory.");
      return(NULL);
   }

   memset(obj, 0, sizeof(*obj));

   obj->identifier = VMIC_SDT_IDENTIFIER;
   obj->mask_pii   = params->mask_pii;
   obj->user_data  = params->user_data;

  return(obj) ;
}

bool vmic_sdt_update_mask_pii(vmic_sdt_object_t object, bool enable) {
   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)object;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return(false);
   }
   obj->mask_pii = enable;
   return(true);
}

bool vmic_sdt_handlers(vmic_sdt_object_t object, const vmic_sdt_handlers_t *handlers_in, xrsr_handlers_t *handlers_out) {
  
   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)object;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return(false);
   }
   bool ret = true;
   handlers_out->data           = obj;
   handlers_out->session_begin  = vmic_sdt_handler_session_begin;
   handlers_out->session_config = NULL;
   handlers_out->session_end    = vmic_sdt_handler_session_end;
   handlers_out->stream_begin   = vmic_sdt_handler_stream_begin;
   handlers_out->stream_kwd     = vmic_sdt_handler_stream_kwd;
   handlers_out->stream_end     = vmic_sdt_handler_stream_end;
   handlers_out->connected      = vmic_sdt_handler_connected;
   handlers_out->disconnected   = vmic_sdt_handler_disconnected;
   handlers_out->stream_audio   = vmic_recv_audiodata;

   obj->handlers = *handlers_in;

   return(ret);
}

int vmic_recv_audiodata(unsigned char* data, uint32_t size)
{
  XLOGD_DEBUG("Received Buffer Size:%d",size);
  memcpy(audio_buffer,data,size);
  vmic_alsa_buffer_playback(audio_buffer);
}
int vmic_alsa_buffer_playback(unsigned char* audio_stream)
{
  if (pcm = snd_pcm_writei(pcm_handle,audio_stream, frames) == -EPIPE) {
       XLOGD_ERROR("ERROR: snd_pcm_writei:%s",snd_strerror(pcm));
       snd_pcm_prepare(pcm_handle);
  } else if (pcm < 0) {
      XLOGD_ERROR("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
  } else if ( pcm == -EAGAIN )
  {
       XLOGD_ERROR("ERROR. EAGAIN raised by  PCM device. %s\n", snd_strerror(pcm));
       snd_pcm_prepare(pcm_handle);
  }

}

void vmic_sdt_destroy(vmic_sdt_object_t object) {
   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)object;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }
   XLOGD_INFO("");
   obj->identifier                     = 0;
   free(obj);
}

void vmic_sdt_handler_session_begin(void *data, const uuid_t uuid, xrsr_src_t src, uint32_t dst_index, xrsr_keyword_detector_result_t *detector_result, xrsr_session_config_out_t *config_out, xrsr_session_config_in_t *config_in, rdkx_timestamp_t *timestamp, const char *transcription_in) {

   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }

   char uuid_str[37] = {'\0'};
   int rc = 0;
   vmic_sdt_stream_params_t stream_params;
   stream_params.keyword_sample_begin               = (detector_result != NULL ? detector_result->offset_kwd_begin - detector_result->offset_buf_begin : 0);
   stream_params.keyword_sample_end                 = (detector_result != NULL ? detector_result->offset_kwd_end   - detector_result->offset_buf_begin : 0);
   stream_params.keyword_doa                        = (detector_result != NULL ? detector_result->doa : 0);
   stream_params.keyword_sensitivity                = 0;
   stream_params.keyword_sensitivity_triggered      = false;
   stream_params.keyword_sensitivity_high           = 0;
   stream_params.keyword_sensitivity_high_support   = false;
   stream_params.keyword_sensitivity_high_triggered = false;
   stream_params.dynamic_gain                       = 0.0;
   stream_params.linear_confidence                  = 0.0;
   stream_params.nonlinear_confidence               = 0;
   stream_params.signal_noise_ratio                 = 255.0; // Invalid;
   stream_params.push_to_talk                       = false;

   if(obj->handlers.session_begin != NULL) {
      (*obj->handlers.session_begin)(uuid, src, dst_index, config_out, &stream_params, timestamp,obj->user_data);
   }
}

void vmic_sdt_handler_session_end(void *data, const uuid_t uuid, xrsr_session_stats_t *stats, rdkx_timestamp_t *timestamp) {
   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }
   if(obj->handlers.session_end != NULL) {
      (*obj->handlers.session_end)(uuid, stats, timestamp,obj->user_data);
   }

}

void vmic_sdt_handler_stream_begin(void *data, const uuid_t uuid, xrsr_src_t src, rdkx_timestamp_t *timestamp) {
   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }

   vmic_init();

   if(obj->handlers.stream_begin != NULL) {
      (*obj->handlers.stream_begin)(uuid, src, timestamp, obj->user_data);
   }
}

void vmic_sdt_handler_stream_kwd(void *data, const uuid_t uuid, rdkx_timestamp_t *timestamp) {

   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }

   if(obj->handlers.stream_kwd != NULL) {
      (*obj->handlers.stream_kwd)(uuid, timestamp,obj->user_data);
   }
}

void vmic_sdt_handler_stream_end(void *data, const uuid_t uuid, xrsr_stream_stats_t *stats, rdkx_timestamp_t *timestamp) {

   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }

   if(obj->handlers.stream_end != NULL) {
      (*obj->handlers.stream_end)(uuid, stats, timestamp, obj->user_data);
   }
}

bool vmic_sdt_handler_connected(void *data, const uuid_t uuid, xrsr_handler_send_t send, void *param, rdkx_timestamp_t *timestamp) {

   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return(false);
   }
   if(obj->handlers.connected != NULL) {
      (*obj->handlers.connected)(uuid, timestamp,obj->user_data);
   }
   return(true);
}

void vmic_sdt_handler_disconnected(void *data, const uuid_t uuid, xrsr_session_end_reason_t reason, bool retry, bool *detect_resume, rdkx_timestamp_t *timestamp) {
   
   vmic_close();

   vmic_sdt_obj_t *obj = (vmic_sdt_obj_t *)data;
   if(!vmic_sdt_object_is_valid(obj)) {
      XLOGD_ERROR("invalid object");
      return;
   }

   if(obj->handlers.disconnected != NULL) {
      (*obj->handlers.disconnected)(uuid, retry, timestamp, obj->user_data);
   }
}

uint64_t vmic_sdt_time_get(void) {
    struct timespec ts;
    errno = 0;
    if(clock_gettime(CLOCK_REALTIME, &ts)) {
       int errsv = errno;
       XLOGD_ERROR("unable to get clock <%s>", strerror(errsv));
       return(0);
    }
    // Return the time in milliseconds since epoch
    return(((uint64_t)ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

void vmic_init()
{
    do
    {
    /* Open the PCM device in playback mode */
    if (pcm = snd_pcm_open(&pcm_handle,PCM_DEVICE,SND_PCM_STREAM_PLAYBACK,0) < 0)
    {
        XLOGD_ERROR("ERROR: Can't open \"%s\" PCM device. %s\n",PCM_DEVICE, snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm_handle, params);

    /* Set parameters */
    if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
                                    SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
    {
        XLOGD_ERROR("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
                                            SND_PCM_FORMAT_S16_LE) < 0)
    {
        XLOGD_ERROR("ERROR: Can't set format. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
    {
        XLOGD_ERROR("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0)
    {
        XLOGD_ERROR("ERROR: Can't set rate. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    if ( pcm = snd_pcm_hw_params_set_period_size_near (pcm_handle, params, &period_size, 0 ) < 0)
    {
        XLOGD_ERROR("ERROR: Can't set period Size. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }
 
    /* Write parameters */
    if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
    {
        XLOGD_ERROR("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }


    /* Allocate buffer to hold single period */
    if (pcm = snd_pcm_hw_params_get_period_size(params, &frames, 0) < 0)
    {
        XLOGD_ERROR("ERROR: Can't get period size. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    /* Allocate buffer to hold single period */
    if (pcm = snd_pcm_hw_params_get_period_time(params, &ptime, NULL) < 0)
    {
        XLOGD_ERROR("ERROR: Can't get period size. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        break;
    }

    if ((pcm = snd_pcm_sw_params_malloc (&sw_params)) < 0) 
    {
         XLOGD_ERROR("cannot allocate software parameters structure (%s)\n",snd_strerror (pcm));
	 snd_pcm_close(pcm_handle);
	 break;
    }

     if ((pcm = snd_pcm_sw_params_current (pcm_handle, sw_params)) < 0) 
     {
          XLOGD_ERROR("cannot initialize software parameters structure (%s)\n",snd_strerror (pcm));
	  snd_pcm_close(pcm_handle);
	  break;
     }
     if ((pcm = snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, ( 20 * period_size))) < 0) 
     {
         XLOGD_ERROR("cannot set start mode (%s)\n",snd_strerror (pcm));
	 snd_pcm_close(pcm_handle);
	 break;
     }

     if ((pcm = snd_pcm_sw_params (pcm_handle, sw_params)) < 0) 
     {
          XLOGD_ERROR("cannot set software parameters (%s)\n",snd_strerror (pcm));
	  snd_pcm_close(pcm_handle);
	  break;
     }
     if ((pcm = snd_pcm_prepare (pcm_handle)) < 0) 
     {
         XLOGD_ERROR("cannot prepare audio interface for use (%s)\n",snd_strerror (pcm));
	 snd_pcm_close(pcm_handle);
	 break;
     }
     if (( pcm =  snd_pcm_start(pcm_handle))<0 )
     {
	 XLOGD_ERROR("cannot start pcm  (%s)\n",snd_strerror (pcm));
	 snd_pcm_close(pcm_handle);
	 break;
     }

    }while(0);

}

void vmic_close()
{
   if ( NULL != pcm_handle)
   {
      snd_pcm_drain(pcm_handle);
      snd_pcm_close(pcm_handle);
   }
}

