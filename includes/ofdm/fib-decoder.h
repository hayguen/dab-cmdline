#
/*
 *    Copyright (C) 2013 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
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
 */
#
#ifndef __FIB_DECODER__
#define __FIB_DECODER__
#
//
#include <stdint.h>
#include <stdio.h>
#include <atomic>
#include <mutex>
#include <string>
#include "dab-api.h"
#include "dab-constants.h"
#include "tii_table.h"

//	from FIG1/2
struct serviceId
{
  serviceId() {
    inUse = false;
    clear();
  }

  void clear() {
    inUse = false;
    SId = -1;
    label[0] = 0;
    abbr[0] = 0;
    hasName = false;
    hasPNum       = false;
    hasLanguage   = false;
    language      = -1;
    programType   = -1;
    pNum          = -1;
    numSCIds      = 0;
    numSCid       = 0;
    numSubChId    = 0;
    SCIds[0] = SCIds[1] = SCIds[2] = SCIds[3] = -1;
    SCid[0] = SCid[1] = SCid[2] = SCid[3] = -1;
    SubChId[0] = SubChId[1] = SubChId[2] = SubChId[3] = -1;
  }

  bool inUse;
  int32_t SId;  // serviceId
  char label[32];
  char abbr[32];
  bool hasName; // -> change to hasLabel ?
  bool hasPNum;
  bool hasLanguage;
  int16_t language;
  int16_t programType;
  uint16_t pNum;
  // following fields are filled from HandleFIG0Extension8()
  int16_t numSCIds;
  int16_t numSCid;
  int16_t numSubChId;
  int16_t SCIds[4];
  int16_t SCid[4];
  int16_t SubChId[4];
};

//      The service component describes the actual service
//      It really should be a union
struct serviceComponent
{
  serviceComponent() {
    inUse = false;
    service = nullptr;
    clear();
  }

  void clear() {
    inUse = false;
    compType      = '?';
    service       = nullptr;
    TMid          = -1;
    componentNr   = -1;
    ASCTy         = -1;
    PS_flag       = -1;
    subchannelId  = -1;
    SCId          = -1;
    CAflag        = 0xff;

    appType       = -1;
    SCIds         = -1;

    is_madePublic = false;
    DSCTy         = -1;
    DGflag        = 0xff;
    packetAddress = -1;

    hasLabel      = false;
    label[0] = 0;
    abbr[0] = 0;
  }

  bool inUse;           // just administration
  char compType;
  int8_t TMid;          // the transport mode
  serviceId *service;   // belongs to the service
  int16_t componentNr;  // component

  int16_t ASCTy;          // used for audio
  int16_t PS_flag;        // use for both audio and packet
  int16_t subchannelId;   // used in both audio and packet
  int16_t SCId;           // used in packet: SCId (Service Component Identifier):
                          // this 12-bit field shall uniquely identify the service component within the ensemble
  uint8_t CAflag;         // used in packet (or not at all)

  int16_t appType;        // used in packet; from FIG0Extension13()
  int16_t SCIds;          // from FIG0Extension13()

  // following fields from FIG0Extension3()
  bool is_madePublic;
  int16_t DSCTy;          // used in packet
  uint8_t DGflag;         // used for TDC
  int16_t packetAddress;  // used in packet

  bool hasLabel; // -> change to hasLabel ?
  char label[32];
  char abbr[32];
};

struct channelMap {
  channelMap() {
    inUse = false;
    clear();
  }

  void clear() {
    inUse = false;
    SubChId = -1;
    StartAddr = -1;
    Length = -1;
    subChanSize = -1;
    shortForm = false;
    protIdxOrCase = -1;
    protTabIdx = -1;
    BitRate = -1;
    language = -1;
    protLevel = -1;
    FEC_scheme = -1;
  }

  bool inUse;
  int32_t SubChId;
  int32_t StartAddr;
  int32_t Length;
  int32_t subChanSize;
  bool shortForm;
  int32_t protIdxOrCase;
  int32_t protTabIdx;
  int32_t protLevel;
  int32_t BitRate;
  int16_t language;
  int16_t FEC_scheme;
};

class fib_processor {
 public:
  fib_processor(ensemblename_t, programname_t, void *);
  ~fib_processor(void);

  void process_FIB(const uint8_t *, uint16_t);
  void clearEnsemble(void);
  bool syncReached(void);
  std::string nameFor(int32_t);
  int32_t SIdFor(std::string &);
  uint8_t kindofService(std::string &);
  uint8_t kindofService(int32_t SId);
  void dataforAudioService(std::string &, audiodata *);
  void dataforDataService(std::string &, packetdata *);
  void dataforAudioService(std::string &, audiodata *, int16_t);
  void dataforDataService(std::string &, packetdata *, int16_t);
  void dataforAudioService(int32_t SId, audiodata *, int16_t);
  void dataforDataService(int32_t SId, packetdata *, int16_t);
  std::complex<float> get_coordinates(int16_t, int16_t, bool *);
  void printAll_metaInfo(FILE *out);

  void reset(void);
  int32_t get_CIFcount(void) const;
  bool has_CIFcount(void) const;
  void newFrame(void);

  //	Extended functions, contributed by Hayati Ayguen
  std::complex<float> get_coordinates(int16_t, int16_t, bool *,
                                      int16_t *pMainId, int16_t *pSubId,
                                      int16_t *pTD);
  uint8_t getECC(bool *);
  uint8_t getInterTabId(bool *);

  void setEId_handler(ensembleid_t EId_Handler);

 private:
  ensembleid_t ensembleidHandler;
  ensemblename_t ensemblenameHandler;
  programname_t programnameHandler;
  void *userData;
  serviceId *findServiceId(int32_t);
  serviceComponent *find_packetComponent(int16_t SubChId, int16_t SCId);
  serviceComponent *find_serviceComponent(int32_t SId, int16_t SCId);
  serviceComponent *find_serviceComponentNr(int32_t SId, int16_t componentNr);
  serviceId *findServiceId(std::string, bool fullMatchOnly = false);

  void bind_audioService(int8_t, uint32_t, int16_t, int16_t, int16_t, int16_t);
  void bind_packetService(int8_t, uint32_t, int16_t, int16_t, int16_t, int16_t);
  void dataforAudioService(serviceId *selectedService, audiodata *, int16_t);
  void dataforDataService(serviceId *selectedService, packetdata *, int16_t);

  std::atomic<int32_t> CIFcount;
  std::atomic<bool> hasCIFcount;
  void process_FIG0(const uint8_t *);
  void process_FIG1(const uint8_t *);
  bool FIG0Extension0(const uint8_t *);
  bool FIG0Extension1(const uint8_t *);
  bool FIG0Extension2(const uint8_t *);
  bool FIG0Extension3(const uint8_t *);
  bool FIG0Extension4(const uint8_t *);
  bool FIG0Extension5(const uint8_t *);
  bool FIG0Extension6(const uint8_t *);
  bool FIG0Extension7(const uint8_t *);
  bool FIG0Extension8(const uint8_t *);
  bool FIG0Extension9(const uint8_t *);
  bool FIG0Extension10(const uint8_t *);
  bool FIG0Extension11(const uint8_t *);
  bool FIG0Extension12(const uint8_t *);
  bool FIG0Extension13(const uint8_t *);
  bool FIG0Extension14(const uint8_t *);
  bool FIG0Extension15(const uint8_t *);
  bool FIG0Extension16(const uint8_t *);
  bool FIG0Extension17(const uint8_t *);
  bool FIG0Extension18(const uint8_t *);
  bool FIG0Extension19(const uint8_t *);
  bool FIG0Extension20(const uint8_t *);
  bool FIG0Extension21(const uint8_t *);
  bool FIG0Extension22(const uint8_t *);
  bool FIG0Extension23(const uint8_t *);
  bool FIG0Extension24(const uint8_t *);
  bool FIG0Extension25(const uint8_t *);
  bool FIG0Extension26(const uint8_t *);

  int16_t HandleFIG0Extension1(const uint8_t *, int16_t, uint8_t, uint8_t, uint8_t);
  int16_t HandleFIG0Extension2(const uint8_t *, int16_t, uint8_t, uint8_t, uint8_t);
  int16_t HandleFIG0Extension3(const uint8_t *, int16_t, uint8_t, uint8_t, uint8_t);
  int16_t HandleFIG0Extension5(const uint8_t *, uint8_t, uint8_t, uint8_t, int16_t);
  int16_t HandleFIG0Extension8(const uint8_t *, int16_t, uint8_t, uint8_t, uint8_t);
  int16_t HandleFIG0Extension13(const uint8_t *, int16_t, uint8_t, uint8_t, uint8_t);
  int16_t HandleFIG0Extension21(const uint8_t *, uint8_t, uint8_t, uint8_t, int16_t);
  int16_t HandleFIG0Extension22(const uint8_t *, uint8_t, uint8_t, uint8_t, int16_t);

  bool FIG0processingOutput[32];
  uint8_t FIBrawCompressed[32];

  int32_t dateTime[8];
  channelMap subChannels[64];
  serviceComponent ServiceComps[64];
  serviceId listofServices[64+1];
  tii_table coordinates;

  std::atomic<uint8_t> ecc_byte;
  std::atomic<uint8_t> interTabId;

  bool dateFlag;
  bool firstTimeEId;
  bool firstTimeEName;
  std::atomic<bool> ecc_Present;
  std::atomic<bool> interTab_Present;

  bool isSynced;
  mutex fibLocker;
  //
  //	these were signals
  void addtoEnsemble(const std::string & label, const std::string & abbr, int32_t);
  void nameofEnsemble(int, const std::string & label, const std::string & abbr);
  void idofEnsemble(int32_t);
  void changeinConfiguration(void);
};

#endif
