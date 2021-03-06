#
/*
 *    Copyright (C) 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the DAB-library program
 *    DAB-library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB-library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB-library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#
#ifndef __DAB_PROCESSOR__
#define __DAB_PROCESSOR__
/*
 *
 */
#include <stdint.h>
#include <atomic>
#include <thread>
#include <vector>
#include "dab-api.h"
#include "dab-constants.h"
#include "dab-params.h"
#include "fic-handler.h"
#include "msc-handler.h"
#include "ofdm-decoder.h"
#include "phasereference.h"
#include "ringbuffer.h"
#include "sample-reader.h"
#include "tii_detector.h"
//
class deviceHandler;

class dabProcessor {
 public:
  dabProcessor(deviceHandler *,
               uint8_t,  // Mode
               syncsignal_t, systemdata_t, ensemblename_t, programname_t,
               fib_quality_t, audioOut_t, bytesOut_t, dataOut_t, programdata_t,
               programQuality_t, motdata_t, RingBuffer<std::complex<float>> *,
               RingBuffer<std::complex<float>> *, void *);
  virtual ~dabProcessor();
  void reset();
  void stop();
  void setOffset(int32_t);
  void start();
  bool signalSeemsGood();
  void show_Corrector(int);
  //      inheriting from our delegates
  void setSelectedService(std::string);
  uint8_t kindofService(std::string);
  uint8_t kindofService(int SId);
  void dataforAudioService(std::string, audiodata *);
  void dataforAudioService(std::string, audiodata *, int16_t);
  void dataforAudioService(int SId, audiodata *, int16_t);
  void dataforDataService(std::string, packetdata *);
  void dataforDataService(std::string, packetdata *, int16_t);
  void dataforDataService(int SId, packetdata *, int16_t);
  int32_t get_SId(std::string s);
  std::string get_serviceName(int32_t);
  void printAll_metaInfo(FILE *out);
  void set_audioChannel(audiodata *);
  void set_dataChannel(packetdata *);
  std::string get_ensembleName();
  void clearEnsemble();
  void reset_msc();

  void setTII_handler(tii_t tii_Handler, tii_ex_t tii_ExHandler,
                      int tii_framedelay, float alfa, int resetFrameCount);
  void setEId_handler(ensembleid_t EId_Handler);
  void setError_handler(decodeErrorReport_t err_Handler);
  void setFIB_handler(fibdata_t fib_Handler);

  std::complex<float> get_coordinates(int16_t, int16_t, bool *);
  std::complex<float> get_coordinates(int16_t, int16_t, bool *,
                                      int16_t *pMainId, int16_t *pSubId,
                                      int16_t *pTD);
  uint8_t getECC(bool *);
  uint8_t getInterTabId(bool *);

 private:
  int tii_framedelay;
  int tii_counter;
  tii_t my_tiiHandler;
  tii_ex_t my_tiiExHandler;
  float tii_alfa;
  int tii_resetFrameCount;
  unsigned tii_num;

  deviceHandler *inputDevice;
  dabParams params;
  sampleReader myReader;
  phaseReference phaseSynchronizer;
  TII_Detector my_TII_Detector;
  ofdmDecoder my_ofdmDecoder;
  ficHandler my_ficHandler;
  mscHandler my_mscHandler;
  syncsignal_t syncsignalHandler;
  decodeErrorReport_t errorReportHandler;
  systemdata_t systemdataHandler;
  void call_systemData(bool, int16_t, int32_t);
  std::thread threadHandle;
  void *userData;
  std::atomic<bool> running;
  bool isSynced;
  int32_t T_null;
  int32_t T_u;
  int32_t T_s;
  int32_t T_g;
  int32_t T_F;
  int32_t nrBlocks;
  int32_t carriers;
  int32_t carrierDiff;

  bool wasSecond(int16_t, dabParams *);

  virtual void run();
};
#endif
