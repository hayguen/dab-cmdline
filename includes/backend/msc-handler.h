#
/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the DAB-library
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
 *    along with DAB-library-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#
/*
 * 	MSC data
 */
#
#ifndef __MSC_HANDLER__
#define __MSC_HANDLER__

#include <stdint.h>
#include <stdio.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include "dab-api.h"
#include "dab-constants.h"
#include "dab-params.h"
#include "fft_handler.h"
#include "freq-interleaver.h"
#include "semaphore.h"

class virtualBackend;

using namespace std;
class mscHandler {
 public:
  mscHandler(uint8_t, audioOut_t, dataOut_t, bytesOut_t, programQuality_t,
             motdata_t, void *);
  ~mscHandler(void);
  void setError_handler(decodeErrorReport_t err_Handler);
  void process_mscBlock(std::complex<float> *, int16_t);
  void set_audioChannel(audiodata *);
  void set_dataChannel(packetdata *);
  void reset(void);
  void stop(void);
  void start(void);

 private:
  virtual void run(void);
  void process_mscBlock(std::vector<int16_t>, int16_t);
  dabParams params;
  fft_handler my_fftHandler;
  interLeaver myMapper;
  audioOut_t soundOut;
  dataOut_t dataOut;
  bytesOut_t bytesOut;
  programQuality_t programQuality;
  decodeErrorReport_t errorReportHandler;
  motdata_t motdata_Handler;
  void *userData;
  Semaphore freeSlots;
  Semaphore usedSlots;
  std::complex<float> **theData;
  std::atomic<bool> running;

  std::thread threadHandle;
  std::vector<complex<float> > phaseReference;
  bool audioService;
  std::mutex mutexer;
  std::vector<virtualBackend *> theBackends;
  //	std::vector<int16_t> cifVector;
  int16_t cifCount;
  int16_t blkCount;
  std::atomic<bool> work_to_do;
  int16_t BitsperBlock;
  int16_t numberofblocksperCIF;
  int16_t blockCount;
};

#endif
