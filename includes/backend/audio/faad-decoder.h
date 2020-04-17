#
/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	This file will be included in mp4processor
 */
#
#ifndef __FAAD_DECODER__
#define __FAAD_DECODER__

#include "dab-api.h"
#include "neaacdec.h"
#include "ringbuffer.h"

typedef struct {
  int rfa;
  int dacRate;
  int sbrFlag;
  int psFlag;
  int aacChannelMode;
  int mpegSurround;
} stream_parms;

class faadDecoder {
 private:
  bool processorOK;
  bool aacInitialized;
  uint32_t aacCap;
  NeAACDecHandle aacHandle;
  NeAACDecConfigurationPtr aacConf;
  NeAACDecFrameInfo hInfo;
  bool isStereo;
  int32_t baudRate;
  void *userData;
  audioOut_t soundOut;

  void output(int16_t *buffer, int size, bool isStereo, int rate) {
    if (soundOut == NULL) return;
    (soundOut)(buffer, size, rate, isStereo, userData);
  }
  //
 public:
  faadDecoder(audioOut_t soundOut, void *userData) {
    aacCap = NeAACDecGetCapabilities();
    aacHandle = NeAACDecOpen();
    aacConf = NeAACDecGetCurrentConfiguration(aacHandle);
    aacInitialized = false;
    baudRate = 48000;
    this->soundOut = soundOut;
    this->userData = userData;
  }

  ~faadDecoder(void) { NeAACDecClose(aacHandle); }

  int get_aac_channel_configuration(int16_t m_mpeg_surround_config,
                                    uint8_t aacChannelMode) {
    switch (m_mpeg_surround_config) {
      case 0:  // no surround
        return aacChannelMode ? 2 : 1;
      case 1:  // 5.1
        return 6;
      case 2:  // 7.1
        return 7;
      default:
        return -1;
    }
  }

  int16_t MP42PCM(stream_parms *sp, uint8_t buffer[], int16_t bufferLength,
                  decodeErrorReport_t errorReportHandler) {
    int16_t samples;
    uint8_t channels;
    long unsigned int sample_rate;
    int16_t *outBuffer;
    NeAACDecFrameInfo hInfo;

    if (!aacInitialized) {
      /* AudioSpecificConfig structure (the only way to select 960 transform
       * here!)
       *
       *  00010 = AudioObjectType 2 (AAC LC)
       *  xxxx  = (core) sample rate index
       *  xxxx  = (core) channel config
       *  100   = GASpecificConfig with 960 transform
       *
       * SBR: implicit signaling sufficient - libfaad2
       * automatically assumes SBR on sample rates <= 24 kHz
       * => explicit signaling works, too, but is not necessary here
       *
       * PS:  implicit signaling sufficient - libfaad2
       * therefore always uses stereo output (if PS support was enabled)
       * => explicit signaling not possible, as libfaad2 does not
       * support AudioObjectType 29 (PS)
       */
      int core_sr_index = sp->dacRate
                              ? (sp->sbrFlag ? 6 : 3)
                              : (sp->sbrFlag ? 8 : 5);  // 24/48/16/32 kHz
      int core_ch_config =
          get_aac_channel_configuration(sp->mpegSurround, sp->aacChannelMode);
      if (core_ch_config == -1) {
        fprintf(stderr, "Unrecognized mpeg surround config (ignored): %d\n",
                sp->mpegSurround);
        return false;
      }

      uint8_t asc[2];
      asc[0] = 0b00010 << 3 | core_sr_index >> 1;
      asc[1] = (core_sr_index & 0x01) << 7 | core_ch_config << 3 | 0b100;
      long int init_result =
          NeAACDecInit2(aacHandle, asc, sizeof(asc), &sample_rate, &channels);
      if (init_result != 0) {
        //	If some error initializing occurred, skip the file */
        fprintf(stderr, "Error initializing decoder library: %s\n",
                NeAACDecGetErrorMessage(-init_result));
        NeAACDecClose(aacHandle);
        return false;
      }
      aacInitialized = true;
    }

    outBuffer =
        (int16_t *)NeAACDecDecode(aacHandle, &hInfo, buffer, bufferLength);
    sample_rate = hInfo.samplerate;
    samples = hInfo.samples;
    if ((sample_rate == 24000) || (sample_rate == 32000) ||
        (sample_rate == 48000) || (sample_rate != (long unsigned)baudRate))
      baudRate = sample_rate;

    //	fprintf (stderr, "bytes consumed %d\n", (int)(hInfo. bytesconsumed));
    //	fprintf (stderr, "samplerate = %d, samples = %d, channels = %d, error =
    //%d, sbr = %d\n", sample_rate, samples, 	         hInfo. channels, 	         hInfo. error,
    //	         hInfo. sbr);
    //	fprintf (stderr, "header = %d\n", hInfo. header_type);
    channels = hInfo.channels;
    if (hInfo.error != 0) {
      if (errorReportHandler) errorReportHandler(3, 1, 0, userData);
      fprintf(stderr, "Warning: %s\n", faacDecGetErrorMessage(hInfo.error));
      return 0;
    }

    if (channels == 2) {
      output(outBuffer, samples, sp->aacChannelMode, sample_rate);
    } else if (channels == 1) {
      int16_t *buffer = (int16_t *)alloca(2 * samples);
      int16_t i;
      for (i = 0; i < samples; i++) {
        buffer[2 * i] = ((int16_t *)outBuffer)[i];
        buffer[2 * i + 1] = buffer[2 * i];
      }
      output(buffer, 2 * samples, sp->aacChannelMode, sample_rate);
    } else {
      if (errorReportHandler) errorReportHandler(3, 1, 0, userData);
      fprintf(stderr, "Cannot handle these channels\n");
    }

    return samples / 2;
  }
};
#endif
