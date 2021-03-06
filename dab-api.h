#
/*
 *    Copyright (C) 2016, 2017, 2018
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is the API description of the DAB-library.
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
 *    along with DAB-library, if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __DAB_API__
#define __DAB_API__
#include <stdint.h>
#include <stdio.h>
#include <complex>
#include <string>
#include "ringbuffer.h"

//	Experimental API for controlling the dab software library
//
//	Version 3.0
//	Examples of the use of the DAB-API library are found in the
//	directories
//	a. C++ Example, which gives a simple command line interface to
//	   run DAB
//	b. python, which gives a python program implementing a simple
//	   command line interface to run DAB

#include <stdint.h>
#include "device-handler.h"
//
//
//	This struct (a pointer to) is returned by callbacks of the type
//	programdata_t. It contains parameters, describing the service.
typedef struct {
  bool defined;
  int16_t componentNr;  // 0 == main component
  int16_t subchId;
  int16_t startAddr;
  bool shortForm;     // false EEP long form
  int16_t protLevel;  //
  int16_t DSCTy;
  int16_t length;
  int16_t bitRate;
  int16_t FEC_scheme;
  int16_t DGflag;
  int16_t packetAddress;
  int16_t appType;
  int16_t protIdxOrCase;
  int16_t protTabIdx;
  int16_t subChanSize;
  bool is_madePublic;
  bool componentHasLabel;
  char componentLabel[32];
  char componentAbbr[32];
} packetdata;

//
typedef struct {
  bool defined;
  int16_t componentNr;  // 0 == main component
  int16_t subchId;
  int16_t startAddr;
  bool shortForm;
  int16_t protLevel;
  int16_t length; /* CUs */
  int16_t bitRate;
  int16_t ASCTy;
  int16_t language;
  int16_t programType;
  int16_t protIdxOrCase;
  int16_t protTabIdx;
  int16_t subChanSize;
  bool is_madePublic;
  bool componentHasLabel;
  char componentLabel[32];
  char componentAbbr[32];
} audiodata;

//////////////////////// C A L L B A C K F U N C T I O N S ///////////////
//
//
//	A signal is sent as soon as the library knows that time
//	synchronization will be ok.
//	Especially, if the value sent is false, then it is (almost)
//	certain that no ensemble will be detected
typedef void (*syncsignal_t)(bool, void *);
//
//	the systemdata is sent once per second with information
//	a. whether or not time synchronization is OK
//	b. the SNR,
//	c. the computed frequency offset (in Hz)
typedef void (*systemdata_t)(bool, int16_t, int32_t, void *);
//
//	the fibQuality is sent regularly and indicates the percentage
//	of FIB packages that pass the CRC test
typedef void (*fib_quality_t)(int16_t, void *);
//
//	the ensemblename is sent whenever the library detects the
//	name an ensemble
typedef void (*ensemblename_t)(std::string label, std::string abbr, int32_t, void *);

//
//	the ensembleId (EId) is sent whenever the library detects the
//	ensemble's id
typedef void (*ensembleid_t)(int32_t, void *);
//
//	Each programname in the ensemble is sent once
typedef void (*programname_t)(std::string label, std::string abbr, int32_t, void *);
//
//	after selecting an audio program, the audiooutput, packed
//	as PCM data (always two channels) is sent back
typedef void (*audioOut_t)(int16_t *,  // buffer
                           int,        // size
                           int,        // samplerate
                           bool,       // stereo
                           void *);
//
//	dynamic label data, embedded in the audio stream, is sent as string
typedef void (*dataOut_t)(std::string, void *);
//
//
//	byte oriented data, emitted by various dataHandlers, is sent
//	as array of uint8_t values (packed bytes)
typedef void (*bytesOut_t)(uint8_t *, int16_t, uint8_t, void *);

//	the quality of the DAB data is reflected in 1 number in case
//	of DAB, and 3 in case of DAB+,
//	the first number indicates the percentage of dab packages that
//	passes tests, and for DAB+ the percentage of valid DAB_ frames.
//	The second and third number are for DAB+: the second gives the
//	percentage of packages passing the Reed Solomon correction,
//	and the third number gives the percentage of valid AAC frames
typedef void (*programQuality_t)(int16_t, int16_t, int16_t, void *);

//	immediate report of program errors - to allow early abortion
//	1st number is error type:
//		1: DAB _frame error
//		2: Reed Solom correction failed
//		3: AAC frame error
//		4: OFDM time/phase sync error
//		5: FIC CRC error
//		6: MP4/DAB+ CRC error
//	2nd number (int16_t) is amount of errors, usually 1
//	3nd number (int32_t) is, total amount of DAB frames - for type = 1
//		0 for unknown
typedef void (*decodeErrorReport_t)(int16_t, int16_t, int32_t, void *);

//
//	After selecting a service, parameters of the selected program
//	are sent back.
typedef void (*programdata_t)(audiodata *, void *);

//
//	MOT pictures - i.e. slides encoded in the Program Associated data
//	are stored in a file. Each time such a file is created, the
//	function registered as
typedef void (*motdata_t)(std::string, int, void *);
//	is invoked (if not specified as NULL)

//	TII
typedef void (*tii_t)(int16_t mainId, int16_t subId, unsigned num, void *);
typedef void (*tii_ex_t)(int numOut, int *outTii, float *outAvgSNR,
                         float *outMinSNR, float *outNxtSNR, unsigned numAvg,
                         const float *Pavg, int Pavg_T_u, void *userData);

//	FIB data handler - always binary data of 32 bytes
//	allows processing of FIB FIG/extensions, which are not parsed
typedef void (*fibdata_t)(const uint8_t *fib, int crc_ok, void *);

/////////////////////////////////////////////////////////////////////////
//
//	The API functions
extern "C" {
//	dabInit is called first, with a valid deviceHandler and a valid
//	Mode.
//	The parameters "spectrumBuffer" and "iqBuffer" will contain
//	-- if no NULL parameters are passed -- data to compute a
//	spectrumbuffer	and a constellation diagram.
//
//	The other parameters are as described above. For each of them a NULL
//	can be passed as parameter, with the expected result.
//
void *dabInit(
    deviceHandler *, uint8_t Mode, syncsignal_t syncsignalHandler,
    systemdata_t systemdataHandler, ensemblename_t ensemblenameHandler,
    programname_t programnamehandler, fib_quality_t fib_qualityHandler,
    audioOut_t audioOut_Handler, dataOut_t dataOut_Handler, bytesOut_t bytesOut,
    programdata_t programdataHandler, programQuality_t program_qualityHandler,
    motdata_t motdata_Handler, RingBuffer<std::complex<float>> *spectrumBuffer,
    RingBuffer<std::complex<float>> *iqBuffer, void *userData);

//	dabExit cleans up the library on termination
void dabExit(void *);
//
//	the actual processing starts with calling startProcessing,
//	note that the input device needs to be started separately
void dabStartProcessing(void *);
//
//	dabReset is as the name suggests for resetting the state of the library
void dabReset(void *);
//
//	dabStop will stop operation of the functions in the library
void dabStop(void *);
//
//	dabReset_msc will terminate the operation of active audio and/or data
//	handlers (there may be more than one active!).
//	If selecting a service (or services),
//	normal operation is to call first
//	on dabReset_msc, and then call set_xxxChannel for
//	the requested services
void dabReset_msc(void *);
//
//	is_audioService will return true id the main service with the
//	name is an audioservice
bool is_audioService(void *, const char *);
//
//	is_dataService will return true id the main service with the
//	name is a dataservice
bool is_dataService(void *, const char *);
//
//	is_audioService will return true id the main service with the
//	ID is an audioservice
bool is_audioService_by_id(void *, int SId);
//
//	is_dataService will return true id the main service with the
//	ID is a dataservice
bool is_dataService_by_id(void *, int SId);
//
//	dataforAudioService will search for the audiodata of the i-th
//	(sub)service with the name as given. If no such service exists,
//	the "defined" bit in the struct will be set to false;
void dataforAudioService(void *, const char *, audiodata *, int);
//
//	dataforDataService will search for the packetdata of the i-th
//	(sub)service with the name as given. If no such service exists,
//	the "defined" bit in the struct will be set to false;
void dataforDataService(void *, const char *, packetdata *, int);
//
//	dataforAudioService_by_id will search for the audiodata of the i-th
//	(sub)service with the name as given. If no such service exists,
//	the "defined" bit in the struct will be set to false;
void dataforAudioService_by_id(void *, int SId, audiodata *, int);
//
//	dataforDataService_by_id will search for the packetdata of the i-th
//	(sub)service with the name as given. If no such service exists,
//	the "defined" bit in the struct will be set to false;
void dataforDataService_by_id(void *, int SId, packetdata *, int);
//
//	set-audioChannel will add - if properly defined - a handler
//	for handling the audiodata as described in the parameter
//	to the list of active handlers
void set_audioChannel(void *, audiodata *);
//
//	set-dataChannel will add - if properly defined - a handler
//	for handling the packetdata as described in the parameter
//	to the list of active handlers
void set_dataChannel(void *, packetdata *);
//
//	mapping from a name to a Service identifier is done
int32_t dab_getSId(void *, const char *);
//
//	and the other way around, mapping the service identifier to a name
std::string dab_getserviceName(void *, int32_t);

//	set/activate TII processing - not with dabInit() - for compatiblity
void dab_setTII_handler(void *, tii_t tii_Handler, tii_ex_t tii_ExHandler,
                        int tii_framedelay, float alfa, int resetFrameCount);

//	set/activate reporting of ensembleId EId - extra function for
//compatibility
void dab_setEId_handler(void *, ensembleid_t EId_Handler);

//	set/activate reporting of errors
void dab_setError_handler(void *, decodeErrorReport_t err_Handler);

// save binary FIC data (all FIBs) to following file, saveFile is closed at end
// of DAB decoding
void dab_setFIB_handler(void *Handle, fibdata_t fib_Handler);

void dab_printAll_metaInfo(void *Handle, FILE* out);

}

//
//	Additions, suggested by Hayati
void dab_getCoordinates(void *, int16_t mainId, int16_t subId, float *latitude,
                        float *longitude, bool *success,
                        int16_t *pMainId = nullptr, int16_t *pSubId = nullptr,
                        int16_t *pTD = nullptr);
uint8_t dab_getExtendedCountryCode(void *, bool *success);
uint8_t dab_getInternationalTabId(void *, bool *success);

#endif
