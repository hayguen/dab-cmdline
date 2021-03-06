#
/*
 *    Copyright (C) 2015, 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    Copyright (C) 2018
 *    Hayati Ayguen (h_ayguen@web.de)
 *
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
 *
 *	E X A M P L E  P R O G R A M
 *	This program might (or might not) be used to mould the interface to
 *	your wishes. Do not take it as a definitive and "ready" program
 *	for the DAB-library
 */
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <utility>
#include "band-handler.h"
#include "dab-api.h"
#ifdef HAVE_SDRPLAY
#include "sdrplay-handler.h"
#elif HAVE_AIRSPY
#include "airspy-handler.h"
#elif defined(HAVE_RTLSDR)
#include "rtlsdr-handler.h"
#elif (HAVE_WAVFILES || HAVE_RAWFILES)
#if HAVE_WAVFILES
#include "wavfiles.h"
#endif
#if HAVE_RAWFILES
#include "rawfiles.h"
#endif
#elif HAVE_RTL_TCP
#include "rtl_tcp-client.h"
#endif

#include <atomic>
#ifdef DATA_STREAMER
#include "tcp-server.h"
#endif

#include "../convenience.h"
#include "dab_tables.h"

using std::cerr;
using std::endl;

#include <assert.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include "fibbits.h"
#include "proc_fig0ext6.h"

#define LOG_EX_TII_SPECTRUM 0
#define MAX_EX_TII_BUFFER_SIZE 16
#define FIB_PROCESSING_IN_MAIN 0
#define PRINT_DBG_ALL_SERVICES 1
#define CSV_PRINT_PROTECTION_COLS 0


std::string prepCsvStr(const std::string &s) {
  std::string r = "\"";
  std::string c = s;
  std::replace(c.begin(), c.end(), '"', '\'');
  std::replace(c.begin(), c.end(), ',', ';');
  r += c + "\"";
  return r;
}

/* where to output program/scan infos, when using option '-E' */
static FILE *infoStrm = stderr;

/* where to output audio stream */
static FILE *audioSink = stdout;
static const char *outWaveFilename = nullptr;
static int32_t outFrequency = 0;
static double recDuration = -1.0;
static uint32_t recDurationSmp = 0;

static int32_t timeOut = 0;
static int32_t nextOut = 0;

struct MyServiceData {
  MyServiceData(void) {
    SId = 0;
    gotAudio = false;
    gotAudioPacket[0] = false;
    gotAudioPacket[1] = false;
    gotAudioPacket[2] = false;
    gotAudioPacket[3] = false;
    gotPacket = false;
  }

  int SId;
  std::string programName;
  std::string programAbbr;
  bool gotAudio;
  audiodata audio;
  bool gotAudioPacket[4];  // 1 .. 4 --> 0 .. 3
  packetdata audiopacket[4];

  bool gotPacket;
  packetdata packet;
};

struct MyGlobals {
  std::unordered_map<int, MyServiceData *> channels;
};

MyGlobals globals;

struct ExTiiInfo {
  ExTiiInfo()
      : tii(100),
        numOccurences(0),
        maxAvgSNR(-200.0F),
        maxMinSNR(-200.0F),
        maxNxtSNR(-200.0F) {}

  int tii;
  int numOccurences;
  float maxAvgSNR;
  float maxMinSNR;
  float maxNxtSNR;

  bool operator<(const ExTiiInfo &b) const {
    const ExTiiInfo &a = *this;
    const int minSNR_a =
        int((a.maxMinSNR < 0.0F ? -0.5F : 0.5F) + a.maxMinSNR * 10.0F);
    const int minSNR_b =
        int((b.maxMinSNR < 0.0F ? -0.5F : 0.5F) + b.maxMinSNR * 10.0F);
    if (minSNR_a != minSNR_b) return (minSNR_a > minSNR_b);
    const int avgSNR_a =
        int((a.maxAvgSNR < 0.0F ? -0.5F : 0.5F) + a.maxAvgSNR * 10.0F);
    const int avgSNR_b =
        int((b.maxAvgSNR < 0.0F ? -0.5F : 0.5F) + b.maxAvgSNR * 10.0F);
    return (avgSNR_a > avgSNR_b);
  }
};

std::map<int, int> tiiMap;
std::map<int, ExTiiInfo> tiiExMap;
int numAllTii = 0;

#define PRINT_COLLECTED_STAT_AND_TIME 0
#define PRINT_LOOPS 0
#define PRINT_DURATION 1

#define T_UNITS "ms"
#define T_UNIT_MUL 1000
#define T_GRANULARITY 10

#define ENABLE_FAST_EXIT 1

#if ENABLE_FAST_EXIT
#define FAST_EXIT(N) exit(N)
#else
#define FAST_EXIT(N) \
  do {               \
  } while (0)
#endif

#if PRINT_DURATION
#define FMT_DURATION "%5ld: "
#define SINCE_START , sinceStart()
#else
#define FMT_DURATION ""
#define SINCE_START
#endif

void printCollectedCallbackStat(const char *txt,
                                int out = PRINT_COLLECTED_STAT_AND_TIME);

void printCollectedErrorStat(const char *txt);

inline void sleepMillis(unsigned ms) { usleep(ms * 1000); }

inline uint64_t currentMSecsSinceEpoch() {
  struct timeval te;

  gettimeofday(&te, 0);  // get current time
  uint64_t milliseconds =
      te.tv_sec * 1000LL + te.tv_usec / 1000;  // calculate milliseconds
  //	printf("milliseconds: %lld\n", milliseconds);
  return milliseconds;
}

uint64_t msecs_progStart = 0;

inline long sinceStart(void) {
  uint64_t n = currentMSecsSinceEpoch();

  return long(n - msecs_progStart);
}

void printOptions(void);  // forward declaration
//	we deal with callbacks from different threads. So, if you extend
//	the functions, take care and add locking whenever needed
static std::atomic<bool> run;

static void *theRadio = NULL;

static std::atomic<bool> timeSynced;

static std::atomic<bool> timesyncSet;

static std::atomic<bool> ensembleRecognized;

#ifdef DATA_STREAMER
tcpServer tdcServer(8888);
#endif

static std::string programName = "Sky";
static int32_t serviceIdentifier = -1;
static int recRate = 0;
static bool recStereo = false;
static bool haveStereo = false;
static bool useFirstProgramName = true;
static bool haveProgramNameForSID = false;
static bool gotSampleData = false;

static std::string programNameForSID;

static uint64_t msecs_smp_start;
static uint64_t msecs_smp_curr;
static uint64_t num_samples_since_start;
static uint64_t num_samples_per_check;
static uint64_t num_samples_next_check;
static int print_mismatch_counter;
static int recTolerance = -1;

static std::string ensembleName;
static std::string ensembleAbbr;
static uint32_t ensembleIdentifier = -1;

static bool scanOnly = false;
static int16_t minSNRtoExit = -32768;
static deviceHandler *theDevice = nullptr;

static void sighandler(int signum) {
  fprintf(stderr, "Signal caught, terminating!\n");
  run.store(false);
}

static void syncsignalHandler(bool b, void *userData) {
  timeSynced.store(b);
  timesyncSet.store(true);

  (void)userData;
}

static long numSyncErr = 0, numFeErr = 0, numRsErr = 0, numAacErr = 0;
static long numFicSyncErr = 0, numMp4CrcErr = 0;
static int32_t totalDABframeCount = 0;

static void decodeErrorReportHandler(int16_t errorType, int16_t numErr,
                                     int32_t totalFrameCount, void *userData) {
  if (!gotSampleData) return;  // ignore error until getting initial sample data

  //	1: DAB _frame error
  //	2: Reed Solom correction failed
  //	3: AAC frame error
  //	4: OFDM time/phase sync error
  //	5: FIC CRC error
  //	6: MP4/DAB+ CRC error
  switch (errorType) {
    case 1:
      numFeErr += numErr;
      totalDABframeCount = totalFrameCount;
      break;
    case 2:
      numRsErr += numErr;
      break;
    case 3:
      numAacErr += numErr;
      break;
    case 4:
      numSyncErr += numErr;
      break;
    case 5:
      numFicSyncErr += numErr;
      break;
    case 6:
      numMp4CrcErr += numErr;
      break;
    default:;
  }
}

void printCollectedErrorStat(const char *txt) {
  fprintf(infoStrm, "  decodeErrors:\n");
  fprintf(infoStrm, "      OFDM frame sync errors:    %ld\n", numSyncErr);
  fprintf(infoStrm, "      DAB frame errors (Fe):     %ld @ %ld (total)\n",
          numFeErr, (long)totalDABframeCount);
  fprintf(infoStrm, "      Reed Solomon errors (Rs):  %ld\n", numRsErr);
  fprintf(infoStrm, "      AAC decode errors (Aac):   %ld\n", numAacErr);
  fprintf(infoStrm, "      FIC CRC errors:            %ld\n", numFicSyncErr);
  fprintf(infoStrm, "      MP4/DAB+ CRC errors:       %ld\n", numMp4CrcErr);
  fprintf(infoStrm, "\n");
}

//
//	This function is called whenever the dab engine has taken
//	some time to gather information from the FIC bloks
//	the Boolean b tells whether or not an ensemble has been
//	recognized, the names of the programs are in the
//	ensemble
static void ensemblenameHandler(std::string name, std::string abbr, int EId, void *userData) {
  if (ensembleIdentifier != (uint32_t)EId || ensembleRecognized.load()) return;
  fprintf(stderr,
          "\n" FMT_DURATION
          "ensemblenameHandler: '%s' / '%s' ensemble (EId %X) is "
          "recognized\n\n" SINCE_START,
          name.c_str(), abbr.c_str(), (uint32_t)EId);
  ensembleName = name;
  ensembleAbbr = abbr;
  ensembleRecognized.store(true);
}

static void ensembleIdHandler(int EId, void *userData) {
  fprintf(stderr,
          "\n" FMT_DURATION
          "ensembleIdHandler: ensemble (EId %X) is recognized\n\n" SINCE_START,
          (uint32_t)EId);
  ensembleIdentifier = (uint32_t)EId;
}

static void programnameHandler(std::string s, std::string abbr, int SId, void *userdata) {
  fprintf(stderr, "programnameHandler: '%s' / '%s' (SId %X) is part of the ensemble\n",
          s.c_str(), abbr.c_str(), SId);
  MyServiceData *d = new MyServiceData();
  d->SId = SId;
  d->programName = s;
  d->programAbbr = abbr;
  globals.channels[SId] = d;

  if ((SId == serviceIdentifier || useFirstProgramName) &&
      !haveProgramNameForSID) {
    programNameForSID = s;
    haveProgramNameForSID = true;
    serviceIdentifier = SId;
    useFirstProgramName = false;
  }
}

static void programdataHandler(audiodata *d, void *ctx) {
  auto p = globals.channels.find(serviceIdentifier);
  (void)ctx;

  if (p != globals.channels.end() && d != NULL) {
    p->second->audio = *d;
    p->second->gotAudio = true;
    fprintf(stderr, "programdataHandler for SID %X called. stored audiodata\n",
            serviceIdentifier);
  } else {
    fprintf(stderr,
            "programdataHandler for SID %X called. cannot save audiodata\n",
            serviceIdentifier);
  }
}

//
//	The function is called from within the library with
//	a string, the so-called dynamic label
static void dataOut_Handler(std::string dynamicLabel, void *ctx) {
  (void)ctx;
  static std::string lastLabel;
  if (lastLabel != dynamicLabel) {
    fprintf(stderr, "dataOut: dynamicLabel = '%s'\n", dynamicLabel.c_str());
    lastLabel = dynamicLabel;
  }
}
//
//	The function is called from the MOT handler, with
//	as parameters the filename where the picture is stored
//	d denotes the subtype of the picture
//	typedef void (*motdata_t)(std::string, int, void *);
void motdataHandler(std::string s, int d, void *ctx) {
  (void)s;
  (void)d;
  (void)ctx;
  fprintf(stderr, "motdataHandler: %s\n", s.c_str());
}

//
//	Note: the function is called from the tdcHandler with a
//	frame, either frame 0 or frame 1.
//	The frames are packed bytes, here an additional header
//	is added, a header of 8 bytes:
//	the first 4 bytes for a pattern 0xFF 0x00 0xFF 0x00 0xFF
//	the length of the contents, i.e. framelength without header
//	is stored in bytes 5 (high byte) and byte 6.
//	byte 7 contains 0x00, byte 8 contains 0x00 for frametype 0
//	and 0xFF for frametype 1
//	Note that the callback function is executed in the thread
//	that executes the tdcHandler code.
static void bytesOut_Handler(uint8_t *data, int16_t amount, uint8_t type,
                             void *ctx) {
#ifdef DATA_STREAMER
  uint8_t localBuf[amount + 8];
  int16_t i;
  localBuf[0] = 0xFF;
  localBuf[1] = 0x00;
  localBuf[2] = 0xFF;
  localBuf[3] = 0x00;
  localBuf[4] = (amount >> 8) & 0xFF;
  localBuf[5] = amount & 0xFF;
  localBuf[6] = 0x00;
  localBuf[7] = type == 0 ? 0 : 0xFF;
  for (i = 0; i < amount; i++) localBuf[8 + i] = data;
  tdcServer.sendData(localBuf, amount + 8);
#else
  (void)data;
  (void)amount;
#endif
  (void)ctx;
}

//
//	This function is overloaded. In the normal form it
//	handles a buffer full of PCM samples. We pass them on to the
//	audiohandler, based on portaudio. Feel free to modify this
//	and send the samples elsewhere
//
static void pcmHandler(int16_t *buffer, int size, int rate, bool isStereo,
                       void *ctx) {
  if (scanOnly) return;

  const int smpFrames = size / 2;

  if (outWaveFilename) {
    audioSink = fopen(outWaveFilename, "wb");
    if (!audioSink) {
      fprintf(stderr, "Failed to open %s\n", outWaveFilename);
      exit(1);
    }
    fprintf(stderr, "Open %s for write with samplerate %d in %s\n",
            outWaveFilename, rate, (isStereo ? "stereo" : "mono"));
    // the data buffer[] is always delivered as stereo signal; mono with right
    // == left channel
    waveWriteHeader(rate, outFrequency, 16, 2 /*(isStereo ? 2:1)*/, audioSink);
    outWaveFilename = nullptr;
    gotSampleData = true;

    recStereo = isStereo;
    haveStereo = true;
    recRate = rate;

    msecs_smp_start = currentMSecsSinceEpoch();
    num_samples_since_start = 0;
    num_samples_next_check = num_samples_per_check = rate / 2;  // every 0.5 sec
    print_mismatch_counter = 0;
  }
  if (recDuration > 0) {
    recDurationSmp = recDuration * rate;
    recDuration = -1.0;
  }

  if (recRate != rate && outWaveFilename) {
    fprintf(stderr,
            "abort recording, because samplerate changed from %d to %d. "
            "terminating!\n",
            recRate, rate);
    run.store(false);
    return;
  } else if (recStereo != isStereo && outWaveFilename) {
    fprintf(stderr,
            "abort recording, because stereo flag changed from %s to %s. "
            "terminating!\n",
            recStereo ? "true" : "false", isStereo ? "true" : "false");
    run.store(false);
    return;
  }

  static uint64_t lastRecSeconds = 0;
  uint64_t recSeconds = num_samples_since_start / uint64_t(rate);
  if (recSeconds != lastRecSeconds) {
    double recSecs = (double)(num_samples_since_start / uint64_t(rate));
    fprintf(stderr, "time: %.1f sec\n", recSecs);
    lastRecSeconds = recSeconds;
  }

  static long lastSumDecErrs = 0;
  if (num_samples_since_start >= num_samples_next_check && outWaveFilename) {
    msecs_smp_curr = currentMSecsSinceEpoch();
    uint64_t msecs_delta = msecs_smp_curr - msecs_smp_start;
    uint64_t msecs_num_smp =
        (num_samples_since_start * uint64_t(1000)) / uint64_t(rate);
    long msecs_mismatch = (long)(msecs_num_smp - msecs_delta);
    ++print_mismatch_counter;
    if (print_mismatch_counter >= 20 || msecs_mismatch >= 100 ||
        msecs_mismatch <= -100) {
      // print once in 10 secs (== 20 iterations with 500 msec) .. or if
      // |mismatch| >= 100 ms
      fprintf(stderr,
              "mismatch from system time to #samples @ rate == %ld ms\n",
              msecs_mismatch);
      print_mismatch_counter = 0;
    }
    // if ( recTolerance > 0 && (msecs_mismatch >= recTolerance ||
    // msecs_mismatch <= -recTolerance) ) { 	fprintf (stderr, "abort
    // recording,
    // because mismatch is too big. terminating!\n"); 	run. store (false);
    //}
    num_samples_next_check += num_samples_per_check;

    long sumDecErrs = numSyncErr + numFeErr + numRsErr + numAacErr +
                      numFicSyncErr + numMp4CrcErr;
    if (lastSumDecErrs != sumDecErrs) {
      fprintf(infoStrm,
              "decodeErrors:\tSync %ld\tFe %ld @ %ld\tRs %ld\tAac %ld\tFicCRC "
              "%ld\tMp4CRC %ld\n",
              numSyncErr, numFeErr, (long)totalDABframeCount, numRsErr,
              numAacErr, numFicSyncErr, numMp4CrcErr);
      lastSumDecErrs = sumDecErrs;

      if (recTolerance > 0) {
        if (numSyncErr) {
          fprintf(stderr,
                  "abort recording, because of OFDM frame sync error. "
                  "terminating!\n");
          nextOut = timeOut;
          printCollectedCallbackStat("OFDM frame sync error", 1);
          printCollectedErrorStat("OFDM frame sync error");
          run.store(false);
        } else if (numFeErr &&
                   (long)numFeErr * 100L >= (long)totalDABframeCount) {
          fprintf(stderr,
                  "abort recording, because DAB frame error count >= "
                  "1%%. terminating!\n");
          nextOut = timeOut;
          printCollectedCallbackStat("DAB frame error count >= 1%", 1);
          printCollectedErrorStat("DAB frame error count >= 1%");
          run.store(false);
        }
      }
    }
  }
  num_samples_since_start += smpFrames;  //( size / (isStereo ? 2 : 1) );

  // output rate, isStereo once
  fwrite((void *)buffer, size, 2, audioSink);
  waveDataSize += size * 2;

  if (recDurationSmp) {
    // int sz = isStereo ? (size /2) : size;
    if (recDurationSmp > (uint32_t)smpFrames)
      recDurationSmp -= smpFrames;
    else {
      fprintf(stderr, "recording duration reached, terminating!\n");
      nextOut = timeOut;
      printCollectedCallbackStat("recording duration reached", 1);
      printCollectedErrorStat("recording duration reached");
      run.store(false);
    }
  }
}

static bool stat_gotSysData = false, stat_gotFic = false, stat_gotMsc = false;
static bool stat_everSynced = false;
static long numSnr = 0, sumSnr = 0, avgSnr = -32768;
static long numFic = 0, sumFic = 0, avgFic = 0;
static long numMsc = 0;
static int16_t stat_minSnr = 0, stat_maxSnr = 0;
static int16_t stat_minFic = 0, stat_maxFic = 0;
static int16_t stat_minFe = 0, stat_maxFe = 0;
static int16_t stat_minRsE = 0, stat_maxRsE = 0;
static int16_t stat_minAacE = 0, stat_maxAacE = 0;

static void systemData(bool flag, int16_t snr, int32_t freqOff, void *ctx) {
  if (stat_gotSysData) {
    stat_everSynced = stat_everSynced || flag;
    stat_minSnr = snr < stat_minSnr ? snr : stat_minSnr;
    stat_maxSnr = snr > stat_maxSnr ? snr : stat_maxSnr;
  } else {
    stat_everSynced = flag;
    stat_minSnr = stat_maxSnr = snr;
  }

  ++numSnr;
  sumSnr += snr;
  avgSnr = sumSnr / numSnr;
  stat_gotSysData = true;

  //	fprintf (stderr, "synced = %s, snr = %d, offset = %d\n",
  //	                    flag? "on":"off", snr, freqOff);
}

static void tii(int16_t mainId, int16_t subId, unsigned tii_num, void *ctx) {
  ++numAllTii;
  if (mainId >= 0) {
    int combinedId = mainId * 100 + subId;
    fprintf(stderr, "tii, %d, %u\n", combinedId, tii_num);
    if (scanOnly) tiiMap[combinedId]++;
  }
}

#if LOG_EX_TII_SPECTRUM
static FILE *powerFile = nullptr;
static float bufferedP[MAX_EX_TII_BUFFER_SIZE][2048];
static int numBufferedP = 0;
static int gPavg_T_u = 0;
#endif

static void tiiEx(int numOut, int *outTii, float *outAvgSNR, float *outMinSNR,
                  float *outNxtSNR, unsigned numAvg, const float *Pavg,
                  int Pavg_T_u, void *ctx) {
  int i;
  if (!numOut) return;
  for (i = 0; i < numOut; ++i) {
    ++numAllTii;
    fprintf(stderr, "%s no %d: %d (avg %.1f dB, min %.1f, next %.1f dB, # %u)",
            (i == 0 ? "tii:" : ","), i + 1, outTii[i], outAvgSNR[i],
            outMinSNR[i], outNxtSNR[i], numAvg);
  }
  if (scanOnly) {
    fprintf(stderr, "  =>  ");
    for (i = 0; i < numOut; ++i) {
      ExTiiInfo &ei = tiiExMap[outTii[i]];
      ei.tii = outTii[i];
      ++ei.numOccurences;
      if (outAvgSNR[i] > ei.maxAvgSNR) ei.maxAvgSNR = outAvgSNR[i];
      if (outMinSNR[i] > ei.maxMinSNR) ei.maxMinSNR = outMinSNR[i];
      if (outNxtSNR[i] > ei.maxNxtSNR) ei.maxNxtSNR = outNxtSNR[i];
      fprintf(stderr, "# %d (%d)%s", ei.numOccurences, outTii[i],
              (i == (numOut - 1) ? "\n" : ", "));
    }
#if LOG_EX_TII_SPECTRUM
    if (Pavg && numBufferedP < MAX_EX_TII_BUFFER_SIZE) {
      for (int i = 0; i < Pavg_T_u; ++i) bufferedP[numBufferedP][i] = Pavg[i];
      gPavg_T_u = Pavg_T_u;
      ++numBufferedP;
    }
#endif
  } else {
    fprintf(stderr, "\n");
  }
}

#if LOG_EX_TII_SPECTRUM

static void writeTiiExBuffer() {
  if (!powerFile) {
    powerFile = fopen("power.csv", "w");
  }
  if (powerFile) {
    if (!numBufferedP) {
      fprintf(powerFile, "#0\n");
    } else {
      for (int i = 0; i < gPavg_T_u; ++i) {
        int j = (i + gPavg_T_u / 2) % gPavg_T_u;  // 1024 .. 2048, 0 .. 1023
        int k = (j < gPavg_T_u / 2) ? j : (j - gPavg_T_u);  // -1024 .. +1024
        fprintf(powerFile, "%d, ", k);
        for (int col = 0; col < numBufferedP; ++col) {
          float level = 10.0 * log10(bufferedP[col][j]);
          fprintf(powerFile, "%f,", level);
        }
        fprintf(powerFile, "\n");
      }
    }
    fflush(powerFile);
    fclose(powerFile);
  }
}

#endif

static void fibQuality(int16_t q, void *ctx) {
  if (stat_gotFic) {
    stat_minFic = q < stat_minFic ? q : stat_minFic;
    stat_maxFic = q > stat_maxFic ? q : stat_maxFic;
  } else {
    stat_minFic = stat_maxFic = q;
    stat_gotFic = true;
  }

  ++numFic;
  sumFic += q;
  avgFic = sumFic / numFic;
  //	fprintf (stderr, "fic quality = %d\n", q);
}

static void mscQuality(int16_t fe, int16_t rsE, int16_t aacE, void *ctx) {
  ++numMsc;
  if (stat_gotMsc) {
    stat_minFe = fe < stat_minFe ? fe : stat_minFe;
    stat_maxFe = fe > stat_maxFe ? fe : stat_maxFe;
    stat_minRsE = rsE < stat_minRsE ? rsE : stat_minRsE;
    stat_maxRsE = rsE > stat_maxRsE ? rsE : stat_maxRsE;
    stat_minAacE = aacE < stat_minAacE ? aacE : stat_minAacE;
    stat_maxAacE = aacE > stat_maxAacE ? aacE : stat_maxAacE;
  } else {
    stat_minFe = stat_maxFe = fe;
    stat_minRsE = stat_maxRsE = rsE;
    stat_minAacE = stat_maxAacE = aacE;
    stat_gotMsc = true;
  }
}

void printCollectedCallbackStat(const char *txt, int out) {
  if (!out) return;

  if (timeOut >= nextOut) {   // force output with nextOut = timeOut
    nextOut = timeOut + 500;  // output every 500 ms
    fprintf(stderr, "\n" FMT_DURATION "%s:\n" SINCE_START, txt);
    fprintf(infoStrm,
            "  systemData(): %s: everSynced %s, snr min/max: %d/%d. # %ld avg "
            "%ld\n",
            stat_gotSysData ? "yes" : "no", stat_everSynced ? "yes" : "no",
            int(stat_minSnr), int(stat_maxSnr), numSnr, avgSnr);
    fprintf(infoStrm, "  fibQuality(): %s: q min/max: %d/%d. # %ld avg %ld\n",
            stat_gotFic ? "yes" : "no", int(stat_minFic), int(stat_maxFic),
            numFic, avgFic);

    fprintf(infoStrm,
            "  mscQuality(): %s: fe min/max: %d/%d, rsE min/max: %d/%d, aacE "
            "min/max: %d/%d\n",
            stat_gotMsc ? "yes" : "no", int(stat_minFe), int(stat_maxFe),
            int(stat_minRsE), int(stat_maxRsE), int(stat_minAacE),
            int(stat_maxAacE));

    fprintf(infoStrm,
            "  mscQuality(): # %ld, #errors fe %ld  rsE %ld  aacE %ld\n",
            numMsc, numFeErr, numRsErr, numAacErr);

    fprintf(infoStrm, "\n");
  }
}

void flush_fig_processings() {
#if FIB_PROCESSING_IN_MAIN
  flush_fig0_ext6();
#endif
}

static bool repeater = false;

void device_eof_callback(void *userData) {
  (void)userData;
  if (!repeater) {
    fprintf(stderr, "\nEnd-of-File reached, triggering termination!\n");
    if (gotSampleData) {
      nextOut = timeOut;
      printCollectedCallbackStat("End-of-File reached", 1);
      printCollectedErrorStat("End-of-File reached");
    }

    flush_fig_processings();
    run.store(false);
    exit(30);
  }
}

static FILE *ficFile = NULL;
static unsigned fibCallbackNo = 0;

static void fib_dataHandler(const uint8_t *fib, int crc_ok, void *ud) {
  (void)crc_ok;
  if (ficFile) fwrite(fib, 32, 1, ficFile);
  ++fibCallbackNo;

#if FIB_PROCESSING_IN_MAIN
  if (!crc_ok) return;
  const uint8_t *d = fib;
  int processed = 0;
  while (processed < 30) {
    if (d[0] == 0xFF)  // end marker?
      break;
    const int FIGtype = getMax8Bits(3, 0, d);     // getBits_3 (d, 0);
    const int FIGlen = getMax8Bits(5, 3, d) + 1;  // getBits_5 (d, 3) + 1;
    switch (FIGtype) {
      case 0: {
        int extension = getMax8Bits(5, 8 + 3, d);  // getBits_5 (d, 8 + 3);  //
                                                   // FIG0
        if (extension == 6) proc_fig0_ext6(d + 1, FIGlen - 1, fibCallbackNo);
        break;
      }
      case 1:
      default:
        break;
    }
    processed += FIGlen;
    d = fib + processed;
  }
#endif
}

void allocateDevice(bool openDevice = false, int32_t frequency = 0,
                    int16_t ppmCorrection = 0, int theGain = 100,
                    bool autogain = true, uint16_t deviceIndex = -1,
                    const char *deviceSerial = NULL, const char *rtlOpts = NULL,
                    std::string *fileName = NULL, double fileOffset = 0.0,
                    const char *hostname = NULL, int32_t basePort = 1234) {
  try {
#ifdef HAVE_SDRPLAY
    theDevice =
        new sdrplayHandler(frequency, ppmCorrection, theGain, autogain, 0, 0);
#elif HAVE_AIRSPY
    theDevice = new airspyHandler(frequency, ppmCorrection, theGain);
#elif defined(HAVE_RTLSDR)
    theDevice = new rtlsdrHandler(openDevice, frequency, ppmCorrection, theGain,
                                  autogain, (uint16_t)deviceIndex, deviceSerial,
                                  rtlOpts);
#elif (HAVE_WAVFILES || HAVE_RAWFILES)
#if HAVE_WAVFILES
    if (fileName && strcmp(fileName->c_str(), "-") ) {
      fprintf(stderr, "try to open '%s' as wavFile ..\n", fileName->c_str());
      try {
        theDevice =
            new wavFiles(*fileName, fileOffset, device_eof_callback, nullptr);
      } catch (int e) {
        // allow retry as RAW file (HAVE_RAWFILES)
        fprintf(stderr, "allocating wave device failed (%d), fatal\n", e);
        theDevice = NULL;
      }
    }
#endif
#if HAVE_RAWFILES

    if (!theDevice && fileName) {
      fprintf(stderr, "try to open '%s' as rawFile ..\n", fileName->c_str());
      theDevice =
          new rawFiles(*fileName, fileOffset, device_eof_callback, nullptr);
    }
#endif
#elif HAVE_RTL_TCP
    theDevice = new rtl_tcp_client(hostname, basePort, frequency, theGain,
                                   autogain, ppmCorrection);
#endif

  } catch (int e) {
    fprintf(stderr, "allocating device failed (%d), fatal\n", e);
  }
}


static inline
int32_t timeOutIncGranularity() {
  double off = ( theDevice ) ? theDevice->currentOffset() : -1.0;
  if ( off < 0 )
    timeOut += T_GRANULARITY;
  else
    timeOut = (int32_t)( off * T_UNIT_MUL );
  return timeOut;
}

int main(int argc, char **argv) {
  msecs_progStart = currentMSecsSinceEpoch();  // for  long sinceStart()
  // Default values
  uint8_t theMode = 1;
  std::string theChannel = "11C";
  uint8_t theBand = BAND_III;
  int16_t ppmCorrection = 0;
  int theGain = 35;  // scale = 0 .. 100

  int32_t waitingTime = 10 * T_UNIT_MUL;
  int32_t waitingTimeInit = 10 * T_UNIT_MUL;
  int32_t waitAfterEnsemble = -1;

  bool autogain = false;
  bool printAsCSV = false;
  int tii_framedelay = 10;
  float tii_alfa = 0.9F;
  int tii_resetFrames = 10;
  bool useExTii = false;
  const char *rtlOpts = NULL;

  int opt;
  struct sigaction sigact;
  bandHandler dabBand;
#if defined(HAVE_WAVFILES) || defined(HAVE_RAWFILES)
  std::string fileName;
  double fileOffset = 0.0;
#elif HAVE_RTL_TCP
  std::string hostname = "127.0.0.1";  // default
  int32_t basePort = 1234;             // default
#endif
#if defined(HAVE_RTLSDR)
  int deviceIndex = 0;
  const char *deviceSerial = nullptr;
#endif

  fprintf(stderr,
          "dab_cmdline example-10 V 1.0alfa,\n \
	                  Copyright 2018 Hayati Ayguen/Jan van Katwijk\n");
  timeSynced.store(false);
  timesyncSet.store(false);
  run.store(false);

  if (argc == 1) {
    printOptions();
    exit(1);
  }

  audioSink = stdout;

//	For file input we do not need options like Q, G and C,
//	We do need an option to specify the filename
#if defined(HAVE_WAVFILES) || defined(HAVE_RAWFILES)
#define FILE_OPTS "F:Ro:"
#define NON_FILE_OPTS
#define RTL_TCP_OPTS
#define RTLSDR_OPTS
#else
#define FILE_OPTS
#define NON_FILE_OPTS "C:G:Q"
#if defined(HAVE_RTLSDR)
#define RTLSDR_OPTS "d:s:"
#else
#define RTLSDR_OPTS
#endif
#ifdef HAVE_RTL_TCP
#define RTL_TCP_OPTS "H:I:"
#else
#define RTL_TCP_OPTS
#endif
#endif

#if 0
    {
        bool ok = testGetBits();
        if (!ok)
            return 1;
    }
#endif

  while (
      (opt = getopt(argc, argv,
                    "W:A:M:B:P:p:T:S:E:cft:a:r:xO:w:n:" FILE_OPTS NON_FILE_OPTS
                        RTLSDR_OPTS RTL_TCP_OPTS)) != -1) {
    fprintf(stderr, "opt = %c\n", opt);
    switch (opt) {
      case 'W':
        waitingTimeInit = waitingTime = int32_t(atoi(optarg));
        fprintf(stderr, "read option -W : max time to aquire sync is %d %s\n",
                int(waitingTime), T_UNITS);
        break;

      case 'A':
        waitAfterEnsemble = int32_t(atoi(optarg));
        fprintf(
            stderr,
            "read option -A : additional time after sync to aquire ensemble "
            "is %d %s\n",
            int(waitAfterEnsemble), T_UNITS);
        break;

      case 'E':
        scanOnly = true;
        infoStrm = stdout;
        minSNRtoExit = int16_t(atoi(optarg));
        fprintf(stderr, "read option -E : scanOnly .. with minSNR %d\n",
                minSNRtoExit);
        break;

      case 'c':
        printAsCSV = true;
        break;

      case 'f':
        ficFile = fopen("ficdata.fic", "wb");
        break;

      case 't':
        tii_framedelay = atoi(optarg);
        fprintf(stderr, "read option -t : tii framedelay %d\n", tii_framedelay);
        break;

      case 'a':
        tii_alfa = atof(optarg);
        fprintf(stderr, "read option -a : tii alfa %f\n", tii_alfa);
        break;

      case 'r':
        tii_resetFrames = atoi(optarg);
        fprintf(stderr, "read option -r : tii resetFrames %d\n",
                tii_resetFrames);
        break;

      case 'x':
        useExTii = !useExTii;
        fprintf(stderr, "read option -x : using %s Tii algorithm\n",
                (useExTii ? "extended" : "Jan's"));
        break;

      case 'M':
        theMode = atoi(optarg);
        if (!((theMode == 1) || (theMode == 2) || (theMode == 4))) theMode = 1;
        break;

      case 'B':
        theBand =
            std::string(optarg) == std::string("L_BAND") ? L_BAND : BAND_III;
        break;

      case 'P':
        programName = optarg;
        serviceIdentifier = -1;
        useFirstProgramName = false;
        fprintf(stderr, "read option -P : tune to program '%s'\n", optarg);
        break;

      case 'p':
        ppmCorrection = atoi(optarg);
        break;

      case 'T':
        recTolerance = atoi(optarg);
        break;

      case 'O':
        rtlOpts = optarg;
        break;

#if defined(HAVE_WAVFILES) || defined(HAVE_RAWFILES)
      case 'F':
        fileName = std::string(optarg);
        break;
      case 'R':
        repeater = false;
        break;
      case 'o':
        fileOffset = atof(optarg);
        break;
#else
      case 'C':
        theChannel = std::string(optarg);
        break;

      case 'G':
        theGain = atoi(optarg);
        break;

      case 'Q':
        autogain = true;
        break;
#if defined(HAVE_RTLSDR)
      case 'd':
        deviceIndex = atoi(optarg);
        break;

      case 's':
        deviceSerial = optarg;
        break;
#endif
#ifdef HAVE_RTL_TCP
      case 'H':
        hostname = std::string(optarg);
        break;

      case 'I':
        basePort = atoi(optarg);
        break;
#endif
#endif
      case 'S': {
        std::stringstream ss;
        ss << std::hex << optarg;
        ss >> serviceIdentifier;
        if (ss) programName.clear();
        useFirstProgramName = false;
        fprintf(stderr, "read option -S : tune to program SId '%X'\n",
                serviceIdentifier);
        break;
      }

      case 'w': {
        outWaveFilename = optarg;
        break;
      }

      case 'n': {
        recDuration = atof(optarg);
        break;
      }

      default:
        printOptions();
        exit(1);
    }
  }
  //
  sigact.sa_handler = sighandler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction(SIGINT, &sigact, nullptr);

  int32_t frequency = dabBand.Frequency(theBand, theChannel);
  outFrequency = frequency;

  allocateDevice(true, frequency, ppmCorrection, theGain, autogain,
#if defined(HAVE_RTLSDR)
                 deviceIndex, deviceSerial,
#else
                 0, NULL,
#endif
                 rtlOpts,
#if defined(HAVE_WAVFILES) || defined(HAVE_RAWFILES)
                 &fileName, fileOffset,
#else
                 NULL, 0.0,
#endif
#ifdef HAVE_RTL_TCP
                 hostname, basePort
#else
                 NULL, 0
#endif
  );

  if (!theDevice) {
#if PRINT_DURATION
    fprintf(stderr,
            "\n" FMT_DURATION
            "exiting main() for failed device allocation\n" SINCE_START);
#endif
    exit(32);
  }

  //
  //	and with a sound device we now can create a "backend"
  theRadio =
      dabInit(theDevice, theMode, syncsignalHandler, systemData,
              ensemblenameHandler, programnameHandler, fibQuality, pcmHandler,
              dataOut_Handler, bytesOut_Handler, programdataHandler, mscQuality,
              motdataHandler,  // MOT in PAD
              NULL,            // no spectrum shown
              NULL,            // no constellations
              NULL             // Ctx
      );
  if (theRadio == NULL) {
    fprintf(stderr, "sorry, no radio available, fatal\n");
    nextOut = timeOut;
    printCollectedCallbackStat("A: no radio");
#if PRINT_DURATION
    fprintf(stderr, "\n" FMT_DURATION "exiting main()\n" SINCE_START);
#endif
    exit(4);
  }

  if (useExTii)
    dab_setTII_handler(theRadio, nullptr, tiiEx, tii_framedelay, tii_alfa,
                       tii_resetFrames);
  else
    dab_setTII_handler(theRadio, tii, nullptr, tii_framedelay, tii_alfa,
                       tii_resetFrames);

  dab_setEId_handler(theRadio, ensembleIdHandler);
  dab_setError_handler(theRadio, decodeErrorReportHandler);
  dab_setFIB_handler(theRadio, fib_dataHandler);

  theDevice->setGain(theGain);
  if (autogain) theDevice->set_autogain(autogain);
  theDevice->restartReader(frequency);
  //
  //	The device should be working right now

  timesyncSet.store(false);
  ensembleRecognized.store(false);
  fprintf(stderr, "\n" FMT_DURATION "starting DAB processing ..\n" SINCE_START);
  dabStartProcessing(theRadio);

  bool abortForSnr = false;
  bool continueForFullEnsemble = false;

#if PRINT_LOOPS
  fprintf(
      stderr,
      "\n" FMT_DURATION
      "before while1: cont=%s, abort=%s, timeout=%d, waitTime=%d\n" SINCE_START,
      continueForFullEnsemble ? "true" : "false",
      abortForSnr ? "true" : "false", int(timeOut), int(waitingTime));
#endif
  while (timeOutIncGranularity() < waitingTime && !abortForSnr) {
    if ((!ensembleRecognized.load() || continueForFullEnsemble) &&
        timeOut > T_GRANULARITY)
      sleepMillis(T_GRANULARITY);  // sleep (1);  // skip 1st sleep if possible
    printCollectedCallbackStat("wait for timeSync ..");
    if (scanOnly && numSnr >= 5 && avgSnr < minSNRtoExit) {
      fprintf(
          stderr,
          FMT_DURATION
          "abort because minSNR %d is not met. # is %ld avg %ld\n" SINCE_START,
          int(minSNRtoExit), numSnr, avgSnr);
      abortForSnr = true;
      break;
    }

    if (waitingTime >= waitingTimeInit && ensembleRecognized.load()) {
      const bool prevContinueForFullEnsemble = continueForFullEnsemble;
      continueForFullEnsemble = true;
      if (waitAfterEnsemble > 0) {
        if (!prevContinueForFullEnsemble) {  // increase waitingTime only once
          fprintf(stderr,
                  "t=%d: abort later because already got ensemble data.\n",
                  timeOut);
          waitingTime = timeOut + waitAfterEnsemble;
          fprintf(stderr,
                  FMT_DURATION
                  "waitAfterEnsemble = %d > 0  ==> waitingTime = timeOut + "
                  "waitAfterEnsemble = %d + %d = %d\n" SINCE_START,
                  waitAfterEnsemble, timeOut, waitAfterEnsemble, waitingTime);
        }
      } else if (waitAfterEnsemble == 0) {
        fprintf(stderr,
                "t=%d: abort directly because already got ensemble data.\n",
                timeOut);
        fprintf(stderr,
                FMT_DURATION "waitAfterEnsemble == 0  ==> break\n" SINCE_START);
        break;
      }
    }

#if PRINT_LOOPS
    fprintf(
        stderr,
        FMT_DURATION
        "in while1: cont=%s, abort=%s, timeout=%d, waitTime=%d\n" SINCE_START,
        continueForFullEnsemble ? "true" : "false",
        abortForSnr ? "true" : "false", int(timeOut), int(waitingTime));
#endif
  }

#if PRINT_LOOPS
  fprintf(
      stderr,
      "\n" FMT_DURATION
      "before while2: cont=%s, abort=%s, timeout=%d, waitTime=%d\n" SINCE_START,
      continueForFullEnsemble ? "true" : "false",
      abortForSnr ? "true" : "false", int(timeOut), int(waitingTime));
#endif

  while (continueForFullEnsemble && !abortForSnr &&
         timeOutIncGranularity() < waitingTime) {
    sleepMillis(T_GRANULARITY);
    printCollectedCallbackStat("wait for full ensemble info..");
    if (scanOnly && numSnr >= 5 && avgSnr < minSNRtoExit) {
      fprintf(
          stderr,
          FMT_DURATION
          "abort because minSNR %d is not met. # is %ld avg %ld\n" SINCE_START,
          int(minSNRtoExit), numSnr, avgSnr);
      abortForSnr = true;
      break;
    }
#if PRINT_LOOPS
    fprintf(
        stderr,
        FMT_DURATION
        "in while2: cont=%s, abort=%s, timeout=%d, waitTime=%d\n" SINCE_START,
        continueForFullEnsemble ? "true" : "false",
        abortForSnr ? "true" : "false", int(timeOut), int(waitingTime));
#endif
  }

  if (!timeSynced.load()) {
    fprintf(stderr, FMT_DURATION
            "There does not seem to be a DAB signal here\n" SINCE_START);
    FAST_EXIT(22);
    theDevice->stopReader();
    dabStop(theRadio);
    nextOut = timeOut;
    printCollectedCallbackStat("B: no DAB signal");
    dabExit(theRadio);
    delete theDevice;
#if PRINT_DURATION
    fprintf(stderr, "\n" FMT_DURATION "exiting main()\n" SINCE_START);
#endif
    exit(22);
  }

#if PRINT_LOOPS
  fprintf(
      stderr,
      "\n" FMT_DURATION
      "before while3: cont=%s, abort=%s, timeout=%d, waitTime=%d\n" SINCE_START,
      continueForFullEnsemble ? "true" : "false",
      abortForSnr ? "true" : "false", int(timeOut), int(waitingTime));
#endif
  while (!ensembleRecognized.load() &&
         (timeOutIncGranularity() < waitingTime) && !abortForSnr) {
    sleepMillis(T_GRANULARITY);
    printCollectedCallbackStat("C: collecting ensembleData ..");
    if (scanOnly && numSnr >= 5 && avgSnr < minSNRtoExit) {
      fprintf(
          stderr,
          FMT_DURATION
          "abort because minSNR %d is not met. # is %ld avg %ld\n" SINCE_START,
          int(minSNRtoExit), numSnr, avgSnr);
      abortForSnr = true;
      break;
    }
#if PRINT_LOOPS
    fprintf(
        stderr,
        FMT_DURATION
        "in while3: cont=%s, abort=%s, timeout=%d, waitTime=%d\n" SINCE_START,
        continueForFullEnsemble ? "true" : "false",
        abortForSnr ? "true" : "false", int(timeOut), int(waitingTime));
#endif
  }
  fprintf(stderr, "\n");
  if (abortForSnr) exit(24);
  static int count = 10;

  while (!scanOnly && !ensembleRecognized.load() && (--count > 0)) sleep(1);

  if (!ensembleRecognized.load()) {
    fprintf(stderr, FMT_DURATION "no ensemble data found, fatal\n" SINCE_START);
    FAST_EXIT(22);
    theDevice->stopReader();
    dabStop(theRadio);
    nextOut = timeOut;
    printCollectedCallbackStat("D: no ensembleData");
    dabExit(theRadio);
    delete theDevice;
#if PRINT_DURATION
    fprintf(stderr, "\n" FMT_DURATION "exiting main()\n" SINCE_START);
#endif
    exit(22);
  }

  if (ensembleRecognized.load()) {
    nextOut = timeOut;
    if (!printAsCSV)
      printCollectedCallbackStat("summmary for found ensemble", 1);
  }

  if (!scanOnly) {
    audiodata ad;
    packetdata pd;
    run.store(true);
    const std::string *pPlayProgName = &programName;
    if (serviceIdentifier != -1) {
      if (haveProgramNameForSID)
        pPlayProgName = &programNameForSID;
      else {
        std::cerr << "sorry  we could not find program for given SID "
                  << std::hex << serviceIdentifier << "\n";
        run.store(false);
      }
    }

    std::cerr << "we try to start service '" << *pPlayProgName << "'\n";
    if (is_audioService(theRadio, pPlayProgName->c_str())) {
      dataforAudioService(theRadio, pPlayProgName->c_str(), &ad, 0);
      if (!ad.defined) {
        std::cerr << "sorry  we cannot handle service '" << *pPlayProgName
                  << "'\n";
        run.store(false);
      }
    } else if (is_dataService(theRadio, pPlayProgName->c_str())) {
      dataforDataService(theRadio, pPlayProgName->c_str(), &pd, 0);
      if (!pd.defined) {
        std::cerr << "sorry  we cannot handle service '" << *pPlayProgName
                  << "'\n";
        run.store(false);
      }
    } else {
      std::cerr << "sorry, we cannot handle service '" << *pPlayProgName
                << "'\n";
      run.store(false);
    }

    if (run.load()) {
      dabReset_msc(theRadio);
      if (is_audioService(theRadio, pPlayProgName->c_str()))
        set_audioChannel(theRadio, &ad);
      else
        set_dataChannel(theRadio, &pd);

      int countGran = 0;
      while (run.load()) {
        timeOutIncGranularity();
        sleepMillis(T_GRANULARITY);
        printCollectedCallbackStat("E: loading ..");
        // TODO: check for audio samples after some timeout!
        if (countGran < 3000) {
          countGran += T_GRANULARITY;
          if (countGran >= 3000 && !gotSampleData && outWaveFilename) {
            std::cerr << "abort after 3 sec without receiving sample data!\n";
            run.store(false);
          }
        }
      }

#if LOG_EX_TII_SPECTRUM
      if (useExTii) writeTiiExBuffer();
#endif
    }
    flush_fig_processings();
  } else {  // scan only
    uint64_t secsEpoch = msecs_progStart / 1000;
    bool gotECC = false;
    bool gotInterTabId = false;
    const uint8_t eccCode = dab_getExtendedCountryCode(theRadio, &gotECC);
    const uint8_t interTabId =
        dab_getInternationalTabId(theRadio, &gotInterTabId);
    const std::string comma = ",";

#if LOG_EX_TII_SPECTRUM
    if (useExTii) writeTiiExBuffer();
#endif

    std::string ensembleCols;
    std::string ensembleComm;
    {
      std::stringstream sidStrStream;
      sidStrStream << std::hex << ensembleIdentifier;
      ensembleCols = comma + "0x" + sidStrStream.str();
      ensembleComm = comma + "SID";
      if (ensembleRecognized.load()) {
        ensembleCols += comma + prepCsvStr(ensembleName);
      } else {
        ensembleCols += comma + prepCsvStr("unknown ensemble");
      }
      ensembleComm = comma + "ensembleName";
    }

    int mostTii = -1;
    int numMostTii = 0;
    for (auto const &x : tiiMap) {
      if (x.second > numMostTii && x.first > 0) {
        numMostTii = x.second;
        mostTii = x.first;
      }
    }

    if (printAsCSV) {
      std::string outLine = std::to_string(secsEpoch);
      std::string outComm = std::to_string(secsEpoch);
      outLine += comma + "CSV_ENSEMBLE";
      outComm += comma + "#CSV_ENSEMBLE";
      outLine += comma + prepCsvStr(theChannel);
      outComm += comma + "channel";
      outLine += ensembleCols;
      outComm += ensembleComm;

      outLine += comma + prepCsvStr("snr");
      outComm += comma + "snr";
      outLine += comma + std::to_string(int(stat_minSnr));
      outComm += comma + "min_snr";
      outLine += comma + std::to_string(int(stat_maxSnr));
      outComm += comma + "max_snr";
      outLine += comma + std::to_string(int(numSnr));
      outComm += comma + "num_snr";
      outLine += comma + std::to_string(int(avgSnr));
      outComm += comma + "avg_snr";

      outLine += comma + prepCsvStr("fic");
      outComm += comma + "fic";
      outLine += comma + std::to_string(int(stat_minFic));
      outComm += comma + "min_fic";
      outLine += comma + std::to_string(int(stat_maxFic));
      outComm += comma + "max_fic";
      outLine += comma + std::to_string(int(numFic));
      outComm += comma + "num_fic";
      outLine += comma + std::to_string(int(avgFic));
      outComm += comma + "avg_fic";

      outLine += comma + prepCsvStr("tii");
      outComm += comma + "tii";
      if (useExTii) {
        std::vector<ExTiiInfo> tiiExVec;
        tiiExVec.reserve(tiiExMap.size());
        for (auto const &x : tiiExMap) {
          tiiExVec.push_back(x.second);
        }
        std::sort(tiiExVec.begin(), tiiExVec.end());
        for (auto const &x : tiiExVec) {
          outLine += comma + std::to_string(x.tii);
          outComm += comma + "tii_id";
          outLine += comma + std::to_string(x.numOccurences);
          outComm += comma + "num";
          outLine +=
              comma + std::to_string(int((x.maxAvgSNR < 0.0F ? -0.5F : 0.5F) +
                                         10.0F * x.maxAvgSNR));
          outComm += comma + "max(avg_snr)";
          outLine +=
              comma + std::to_string(int((x.maxMinSNR < 0.0F ? -0.5F : 0.5F) +
                                         10.0F * x.maxMinSNR));
          outComm += comma + "max(min_snr)";
          outLine +=
              comma + std::to_string(int((x.maxNxtSNR < 0.0F ? -0.5F : 0.5F) +
                                         10.0F * x.maxNxtSNR));
          outComm += comma + "max(next_snr)";
          outLine += comma;
          outComm += comma;
        }
      } else {
        outLine += comma + std::to_string(int(numMostTii));
        outComm += comma + "tii_id";
        outLine += comma + std::to_string(int(numAllTii));
        outComm += comma + "num_all";
        outLine += comma + std::to_string(int(mostTii));
        outComm += comma + "num_id";
      }
      
      if (ensembleRecognized.load()) {
        outLine += comma + comma + prepCsvStr("shortLabel") + comma + prepCsvStr(ensembleAbbr);
      } else {
        outLine += comma + comma + prepCsvStr("shortLabel") + comma + prepCsvStr("unknown");
      }
      outComm += comma + comma + "shortLabel" + comma + "label";

      fprintf(infoStrm, "%s\n", outComm.c_str());
      fprintf(infoStrm, "%s\n", outLine.c_str());
    }

    int16_t mainId, subId, TD;
    float gps_latitude, gps_longitude;
    bool gps_success = false;
    dab_getCoordinates(theRadio, -1, -1, &gps_latitude, &gps_longitude,
                       &gps_success, &mainId, &subId);
    if (gps_success) {
      for (subId = 1; subId <= 23; ++subId) {
        dab_getCoordinates(theRadio, mainId, subId, &gps_latitude,
                           &gps_longitude, &gps_success, nullptr, nullptr, &TD);
        if (gps_success) {
          if (!printAsCSV) {
            fprintf(infoStrm,
                    "\ttransmitter gps coordinate (latitude / longitude) "
                    "for\ttii %04d\t=%f / %f\tTD=%d us\n",
                    mainId * 100 + subId, gps_latitude, gps_longitude, int(TD));
          } else {
            std::string outLine = std::to_string(secsEpoch);
            outLine += comma + "CSV_GPSCOOR";
            outLine += comma + prepCsvStr(theChannel);
            outLine += ensembleCols;

            outLine += comma + std::to_string(mainId * 100 + subId);
            outLine += comma + std::to_string(gps_latitude);
            outLine += comma + std::to_string(gps_longitude);
            outLine += comma + std::to_string(int(TD));
            fprintf(infoStrm, "%s\n", outLine.c_str());
          }
        }
      }  // end for
    }    // end if (gps_success)

    std::string outAudioBeg = std::to_string(secsEpoch);
    outAudioBeg += comma + "CSV_AUDIO";
    outAudioBeg += comma + prepCsvStr(theChannel);
    outAudioBeg += ensembleCols;

    std::string outPacketBeg = std::to_string(secsEpoch);
    outPacketBeg += comma + "CSV_PACKET";
    outPacketBeg += comma + prepCsvStr(theChannel);
    outPacketBeg += ensembleCols;

#if PRINT_DBG_ALL_SERVICES
        fprintf(stderr, "\n\n");
#endif

    for (auto &it : globals.channels) {
      serviceIdentifier = it.first;
      int numAudioInSvc = 0;
      int numPacketInSvc = 0;
      char typeOfSvc = '?';
#if PRINT_DBG_ALL_SERVICES
      //fprintf (stderr, "going to check %s\n", it.second->programName.c_str());
#endif
      if (is_audioService_by_id(theRadio, serviceIdentifier))
        typeOfSvc = 'A';
      else if ( is_dataService_by_id(theRadio, serviceIdentifier) )
        typeOfSvc = 'P';

      if ( typeOfSvc != '?' ) {
        if (!printAsCSV) {
          fprintf(infoStrm,
                  "\n" FMT_DURATION
                  "checked program '%s' with SId %X\n" SINCE_START,
                  it.second->programName.c_str(), serviceIdentifier);
        }
        // numberofComponents = getBits_4() in fib-decoder.cpp => 0 .. 15
        for (int i = 0; i < 16; i++) {
          audiodata ad;
          packetdata pd;
          dataforAudioService_by_id(theRadio, serviceIdentifier, &ad, i);

          if (ad.defined) {
            ++numAudioInSvc;
            uint8_t countryId = (serviceIdentifier >> 12) & 0xF;  // audio
            assert( i == ad.componentNr );
            if (!printAsCSV) {
              fprintf(infoStrm, "\taudioData:\n");
              fprintf(infoStrm, "\t\tsubchId\t\t= %d\n", int(ad.subchId));
              fprintf(infoStrm, "\t\tstartAddr\t= %d\n", int(ad.startAddr));
              fprintf(infoStrm, "\t\tshortForm\t= %s\n",
                      ad.shortForm ? "true" : "false");
              fprintf(infoStrm, "\t\tprotLevel\t= %d: '%s'\n",
                      int(ad.protLevel),
                      getProtectionLevel(ad.shortForm, ad.protLevel));
              fprintf(infoStrm, "\t\tcodeRate\t= %d: '%s'\n", int(ad.protLevel),
                      getCodeRate(ad.shortForm, ad.protLevel));
              fprintf(infoStrm, "\t\tlength\t\t= %d\n", int(ad.length));
              fprintf(infoStrm, "\t\tbitRate\t\t= %d\n", int(ad.bitRate));
              fprintf(infoStrm, "\t\tASCTy\t\t= %d: '%s'\n", int(ad.ASCTy),
                      getASCTy(ad.ASCTy));
              if (gotECC)
                fprintf(infoStrm, "\t\tcountry\tECC %X, Id %X: '%s'\n",
                        int(eccCode), int(countryId),
                        getCountry(eccCode, countryId));
              fprintf(infoStrm, "\t\tlanguage\t= %d: '%s'\n", int(ad.language),
                      getLanguage(ad.language));
              fprintf(
                  infoStrm, "\t\tprogramType\t= %d: '%s'\n",
                  int(ad.programType),
                  getProgramType(gotInterTabId, interTabId, ad.programType));

            } else {
              std::string outLine = outAudioBeg;
              std::stringstream sidStrStream;
              sidStrStream << std::hex << serviceIdentifier;
              outLine += comma + "0x" + sidStrStream.str();
              outLine += comma + prepCsvStr(it.second->programName.c_str());
              outLine += comma + std::to_string(int(ad.componentNr));
              outLine += comma + std::to_string(countryId);
              outLine += comma + prepCsvStr(getProtectionLevel(ad.shortForm,
                                                               ad.protLevel));
              outLine += comma + std::to_string(int(ad.protLevel));
              outLine +=
                  comma + prepCsvStr(getCodeRate(ad.shortForm, ad.protLevel));
              outLine += comma + std::to_string(int(ad.bitRate));
              outLine += comma + std::to_string(int(ad.ASCTy));
              outLine += comma + prepCsvStr(getASCTy(ad.ASCTy));
              if (gotECC) {
                std::stringstream eccStrStream;
                std::stringstream countryStrStream;
                eccStrStream << std::hex << int(eccCode);
                countryStrStream << std::hex << int(countryId);
                outLine += comma + "0x" + eccStrStream.str();
                outLine += comma + "0x" + countryStrStream.str();
                const char *countryStr = getCountry(eccCode, countryId);
                outLine += comma + (countryStr ? prepCsvStr(countryStr)
                                               : prepCsvStr("unknown country"));
              } else {
                outLine += comma + comma + comma;
              }
              outLine += comma + std::to_string(int(ad.language));
              outLine += comma + prepCsvStr(getLanguage(ad.language));
              outLine += comma + std::to_string(int(ad.programType));
              outLine +=
                  comma + prepCsvStr(getProgramType(gotInterTabId, interTabId,
                                                    ad.programType));
              outLine += comma + std::to_string(int(ad.length));
              outLine += comma + std::to_string(int(ad.subchId));
              outLine += comma + prepCsvStr(it.second->programAbbr.c_str());
              outLine += comma + prepCsvStr(ad.componentLabel);
              outLine += comma + prepCsvStr(ad.componentAbbr);
#if CSV_PRINT_PROTECTION_COLS
              outLine += comma + std::to_string(int(ad.protIdxOrCase));
              outLine += comma + std::to_string(int(ad.subChanSize));
#endif

              fprintf(infoStrm, "%s\n", outLine.c_str());
            }
          } else {
            dataforDataService_by_id(theRadio, serviceIdentifier, &pd, i);

            if (pd.defined) {
              ++numPacketInSvc;
              uint8_t countryId = (serviceIdentifier >> (5 * 4)) & 0xF;
              assert( i == pd.componentNr );
              if (!printAsCSV) {
                fprintf(infoStrm, "\tpacket:\n");
                fprintf(infoStrm, "\t\tsubchId\t\t= %d\n", int(pd.subchId));
                fprintf(infoStrm, "\t\tstartAddr\t= %d\n", int(pd.startAddr));
                fprintf(infoStrm, "\t\tshortForm\t= %s\n",
                        pd.shortForm ? "true" : "false");
                fprintf(infoStrm, "\t\tprotLevel\t= %d: '%s'\n",
                        int(pd.protLevel),
                        getProtectionLevel(pd.shortForm, pd.protLevel));
                fprintf(infoStrm, "\t\tcodeRate\t= %d: '%s'\n",
                        int(pd.protLevel),
                        getCodeRate(pd.shortForm, pd.protLevel));
                fprintf(infoStrm, "\t\tDSCTy\t\t= %d: '%s'\n", int(pd.DSCTy),
                        getDSCTy(pd.DSCTy));
                fprintf(infoStrm, "\t\tlength\t\t= %d\n", int(pd.length));
                fprintf(infoStrm, "\t\tbitRate\t\t= %d\n", int(pd.bitRate));
                fprintf(infoStrm, "\t\tFEC_scheme\t= %d: '%s'\n",
                        int(pd.FEC_scheme), getFECscheme(pd.FEC_scheme));
                fprintf(infoStrm, "\t\tDGflag\t= %d\n", int(pd.DGflag));
                fprintf(infoStrm, "\t\tpacketAddress\t= %d\n",
                        int(pd.packetAddress));
                if (gotECC)
                  fprintf(infoStrm, "\t\tcountry\tECC %X, Id %X: '%s'\n",
                          int(eccCode), int(countryId),
                          getCountry(eccCode, countryId));
                fprintf(infoStrm, "\t\tappType\t\t= %d: '%s'\n",
                        int(pd.appType), getUserApplicationType(pd.appType));
                fprintf(infoStrm, "\t\tis_madePublic\t=%s\n",
                        pd.is_madePublic ? "true" : "false");
              } else {
                std::string outLine = outPacketBeg;
                std::stringstream sidStrStream;
                sidStrStream << std::hex << serviceIdentifier;
                outLine += comma + "0x" + sidStrStream.str();
                outLine += comma + prepCsvStr(it.second->programName.c_str());
                outLine += comma + std::to_string(int(pd.componentNr));
                outLine += comma + std::to_string(int(pd.protLevel));
                outLine += comma + prepCsvStr(getProtectionLevel(pd.shortForm,
                                                                 pd.protLevel));
                outLine +=
                    comma + prepCsvStr(getCodeRate(pd.shortForm, pd.protLevel));
                outLine += comma + std::to_string(int(pd.DSCTy));
                outLine += comma + prepCsvStr(getDSCTy(pd.DSCTy));
                outLine += comma + std::to_string(int(pd.bitRate));
                outLine += comma + std::to_string(int(pd.FEC_scheme));
                outLine += comma + prepCsvStr(getFECscheme(pd.FEC_scheme));
                outLine += comma + std::to_string(int(pd.DGflag));
                if (gotECC) {
                  std::stringstream eccStrStream;
                  std::stringstream countryStrStream;
                  eccStrStream << std::hex << int(eccCode);
                  countryStrStream << std::hex << int(countryId);
                  outLine += comma + "0x" + eccStrStream.str();
                  outLine += comma + "0x" + countryStrStream.str();
                  const char *countryStr = getCountry(eccCode, countryId);
                  outLine +=
                      comma + (countryStr ? prepCsvStr(countryStr)
                                          : prepCsvStr("unknown country"));
                } else {
                  outLine += comma + comma + comma;
                }
                outLine += comma + std::to_string(int(pd.appType));
                outLine +=
                    comma + prepCsvStr(getUserApplicationType(pd.appType));
                outLine += comma + std::to_string(int(pd.length));
                outLine += comma + std::to_string(int(pd.subchId));
                outLine += comma + prepCsvStr(it.second->programAbbr.c_str());
                outLine += comma + prepCsvStr(pd.componentLabel);
                outLine += comma + prepCsvStr(pd.componentAbbr);
#if CSV_PRINT_PROTECTION_COLS
                outLine += comma + std::to_string(int(pd.protIdxOrCase));
                outLine += comma + std::to_string(int(pd.subChanSize));
#endif

                fprintf(infoStrm, "%s\n", outLine.c_str());
              }
            }
          }
        }
#if PRINT_DBG_ALL_SERVICES
        fprintf(stderr, "LIST: SID %08X of type %c has %d audio and %d packet components: '%s' / '%s'\n",
          serviceIdentifier, typeOfSvc, numAudioInSvc, numPacketInSvc,
          it.second->programName.c_str(), it.second->programAbbr.c_str() );
#endif
      }
      else {
#if PRINT_DBG_ALL_SERVICES
        fprintf(stderr, "ERROR: SID %08X is neither audio nor data!\n", serviceIdentifier);
#endif
      }
    }

    fprintf(stderr, "\n\n");
    dab_printAll_metaInfo(theRadio, stderr);

    nextOut = timeOut;
    printCollectedCallbackStat("D: quit without loading");
  }

#if PRINT_DURATION
  fprintf(stderr, "\n" FMT_DURATION "at dabStop()\n" SINCE_START);
#endif

  if (audioSink != stdout) {
    waveFinalizeHeader(audioSink);
    fclose(audioSink);
    audioSink = stdout;
  }

  FAST_EXIT(123);
  dabStop(theRadio);
  theDevice->stopReader();
  dabExit(theRadio);
  delete theDevice;

  flush_fig_processings();

#if PRINT_DURATION
  fprintf(stderr, "\n" FMT_DURATION "end of main()\n" SINCE_START);
#endif
}

void printOptions(void) {
  if (!theDevice) allocateDevice();
  const char *devOptHelp = NULL;
  if (theDevice) devOptHelp = theDevice->get_opt_help(1);
  if (!devOptHelp) devOptHelp = "";

  fprintf(
      stderr,
      "	dab-cmdline options are\n\
	-W number   amount of time to look for a time sync in %s\n\
	-A number   amount of time to look for an ensemble in %s\n\
	-E minSNR   activates scan mode: if set, quit after loading scan data\n\
	            also quit, if SNR is below minSNR\n\
	-t number   determine tii every number frames. default is 10\n\
	-a alfa     update tii spectral power with factor alfa in 0 to 1. default: 0.9\n\
	-r number   reset tii spectral power every number frames. default: 10\n\
	            tii statistics is output in scan mode\n\
	-x          switch tii algorithm to extended one\n\
	-c          activates CSV output mode\n\
	-M Mode     Mode is 1, 2 or 4. Default is Mode 1\n\
	-B Band     Band is either L_BAND or BAND_III (default)\n\
	-P name     program to be selected in the ensemble\n\
	-p ppmCorr  ppm correction\n\
	-T tol      tolerate mismatch of up to tol ms from system time before recording is aborted\n\
	              default = -1. negative values deactivate abortion\n"
      "%s"
#if defined(HAVE_RTLSDR)
      "	-d index    set RTLSDR device index\n\
	-s serial   set RTLSDR device serial\n"
#endif
#if defined(HAVE_WAVFILES) || defined(HAVE_RAWFILES)
      "	-F filename in case the input is from file\n"
      "	-o offset   offset in seconds from where to start file playback\n"
      "	-R          deactivates repetition of file playback\n"

#else
      "	-C channel  channel to be used\n\
	-G Gain     gain for device (range 1 .. 100)\n\
	-Q          if set, set autogain for device true\n"
#ifdef HAVE_RTL_TCP
      "	-H hostname  hostname for rtl_tcp\n\
	-I port      port number to rtl_tcp\n"
#endif
#endif
      "	-S hexnumber use hexnumber to identify program\n"
      "	-w fileName  write audio to wave file\n"
      "	-f           write binary FIC data to ficdata.fic\n"
      "	-n runtime   stream/save runtime seconds of audio, then exit\n\n",
      T_UNITS, T_UNITS, devOptHelp);
}
