#
/*
 *    Copyright (C) 2014 - 2017
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
 *    along with DAB-library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 	fib and fig processor
 */
#include "fib-decoder.h"
#include <cstring>
#include "charsets.h"
#include "ensemble-handler.h"

#define OUT_CIF_COUNTER 0
#define OUT_FIG0E1      0
#define OUT_ALL_LABELS  1


static const char hexTab[] = "0123456789ABCDEF";

// macros for writeLabel()
#define CHAR_POS_MASK(POS)      ( 1 << (15-(POS)) )
#define ABBR_CHAR(POS,LBL)      LBL[POS]
#define ABBR_POS_CHAR(POS,LBL)  hexTab[POS]


//
//
// Tabelle ETSI EN 300 401 Page 50
// Table is copied from the work of Michael Hoehn
//   subChannels[SubChId].Length    = ProtLevel[tabelIndex][0];
//   subChannels[SubChId].protLevel = ProtLevel[tabelIndex][1];
//   subChannels[SubChId].BitRate   = ProtLevel[tabelIndex][2];
static const int ProtLevel[64][3] = {
//  +0             +1             +2             +3             +4
// Len,PL,  BR    Len,PL,  BR    Len,PL,  BR    Len,PL,  BR    Len,PL,  BR
  { 16, 5,  32}, { 21, 4,  32}, { 24, 3,  32}, { 29, 2,  32}, { 35, 1,  32},  // 0+.
  { 24, 5,  48}, { 29, 4,  48}, { 35, 3,  48}, { 42, 2,  48}, { 52, 1,  48},  // 5+.
  { 29, 5,  56}, { 35, 4,  56}, { 42, 3,  56}, { 52, 2,  56}, { 32, 5,  64},  // 10+.
  { 42, 4,  64}, { 48, 3,  64}, { 58, 2,  64}, { 70, 1,  64}, { 40, 5,  80},  // 15+.
  { 52, 4,  80}, { 58, 3,  80}, { 70, 2,  80}, { 84, 1,  80}, { 48, 5,  96},  // 20+.
  { 58, 4,  96}, { 70, 3,  96}, { 84, 2,  96}, {104, 1,  96}, { 58, 5, 112},  // 25+.
  { 70, 4, 112}, { 84, 3, 112}, {104, 2, 112}, { 64, 5, 128}, { 84, 4, 128},  // 30+.
  { 96, 3, 128}, {116, 2, 128}, {140, 1, 128}, { 80, 5, 160}, {104, 4, 160},  // 35+.
  {116, 3, 160}, {140, 2, 160}, {168, 1, 160}, { 96, 5, 192}, {116, 4, 192},  // 40+.
  {140, 3, 192}, {168, 2, 192}, {208, 1, 192}, {116, 5, 224}, {140, 4, 224},  // 45+.
  {168, 3, 224}, {208, 2, 224}, {232, 1, 224}, {128, 5, 256}, {168, 4, 256},  // 50+.
  {192, 3, 256}, {232, 2, 256}, {280, 1, 256}, {160, 5, 320}, {208, 4, 320},  // 55+.
  {280, 2, 320}, {192, 5, 384}, {280, 3, 384}, {416, 1, 384}                  // 60+. - 63
};

#if OUT_FIG0E1
static const char* Afactors[] = { "/ 12 *  8", "/ 8 *   8", "/  6 *  8", "/  4 *  8" };
static const char* Bfactors[] = { "/ 27 * 32", "/ 21 * 32", "/ 18 * 32", "/ 15 * 32" };
#endif


inline static bool existsInTab(int16_t num, const int16_t *tab, int16_t v) {
  for (int16_t k = 0; k < num; ++k) {
    if (tab[k] == v)
      return true;
  }
  return false;
}


//
fib_processor::fib_processor(ensemblename_t ensemblenameHandler,
                             programname_t programnameHandler, void *userData)
    : ensembleidHandler(nullptr) {
  this->ensemblenameHandler = ensemblenameHandler;
  if (programnameHandler == nullptr) fprintf(stderr, "NULL detected\n");
  this->programnameHandler = programnameHandler;
  this->userData = userData;
  memset(dateTime, 0, sizeof(dateTime));

  for (int k = 0; k < 32; ++k) FIG0processingOutput[k] = false;

  reset();
}

fib_processor::~fib_processor(void) {}

void fib_processor::newFrame(void) {
  ++CIFcount;
#if OUT_CIF_COUNTER
  fprintf(stderr, "+\n");
#endif
}

//
//	FIB's are segments of 256 bits. When here, they already
//	passed the crc and we start unpacking into FIGs
//	This is merely a dispatcher
void fib_processor::process_FIB(const uint8_t *p, uint16_t fib) {
  uint8_t FIGtype;
  int8_t processedBytes = 0;
  const uint8_t *d = p;
  uint16_t hdr;
  // uint8_t	extension;

  fibLocker.lock();
  (void)fib;
  while (processedBytes < 30) {
    hdr = getBits_8(d, 0);
    if (hdr == 0xFF)  // == end marker?
      break;
    FIGtype = getBits_3(d, 0);
    // extension = 0xFF;

    switch (FIGtype) {
      case 0:
        // extension	= getBits_5 (d, 8 + 3);  // FIG0
        process_FIG0(d);
        break;

      case 1:
        // extension	= getBits_3 (d, 8 + 5);  // FIG1
        process_FIG1(d);
        break;

      case 7:
        break;

      default:
        //	         fprintf (stderr, "FIG%d aanwezig\n", FIGtype);
        break;
    }
    //
    //	Thanks to Ronny Kunze, who discovered that I used
    //	a p rather than a d
    processedBytes += getBits_5(d, 3) + 1;
    //	   processedBytes += getBits (p, 3, 5) + 1;
    d = p + processedBytes * 8;
  }
  fibLocker.unlock();
}

//
//	Handle ensemble is all through FIG0
//
void fib_processor::process_FIG0(const uint8_t *d) {
  uint8_t extension = getBits_5(d, 8 + 3);
  bool p = false;  // processed
                   // uint8_t	CN	= getBits_1 (d, 8 + 0);

  switch (extension) {
    case 0:  // ensemble information (6.4.1)
      p = FIG0Extension0(d);
      break;

    case 1:  // sub-channel organization (6.2.1)
      p = FIG0Extension1(d);
      break;

    case 2:  // service organization (6.3.1)
      p = FIG0Extension2(d);
      break;

    case 3:  // service component in packet mode (6.3.2)
      p = FIG0Extension3(d);
      break;

    case 4:  // service component with CA (6.3.3)
      p = FIG0Extension4(d);
      break;

    case 5:  // service component language (8.1.2)
      p = FIG0Extension5(d);
      break;

    case 6:  // service linking information (8.1.15)
      p = FIG0Extension6(d);
      break;

    case 7:  // configuration information (6.4.2)
      p = FIG0Extension7(d);
      break;

    case 8:  // service component global definition (6.3.5)
      p = FIG0Extension8(d);
      break;

    case 9:  // country, LTO & international table (8.1.3.2)
      p = FIG0Extension9(d);
      break;

    case 10:  // date and time (8.1.3.1)
      p = FIG0Extension10(d);
      break;

    case 11:  // obsolete
      p = FIG0Extension11(d);
      break;

    case 12:  // obsolete
      p = FIG0Extension12(d);
      break;

    case 13:  // user application information (6.3.6)
      p = FIG0Extension13(d);
      break;

    case 14:  // FEC subchannel organization (6.2.2)
      p = FIG0Extension14(d);
      break;

    case 15:  // obsolete
      p = FIG0Extension14(d);
      break;

    case 16:  // obsolete
      p = FIG0Extension16(d);
      break;

    case 17:  // Program type (8.1.5)
      p = FIG0Extension17(d);
      break;

    case 18:  // announcement support (8.1.6.1)
      p = FIG0Extension18(d);
      break;

    case 19:  // announcement switching (8.1.6.2)
      p = FIG0Extension19(d);
      break;

    case 20:  // service component information (8.1.4)
      p = FIG0Extension20(d);
      break;

    case 21:  // frequency information (8.1.8)
      p = FIG0Extension21(d);
      break;

    case 22:  // obsolete
      p = FIG0Extension22(d);
      break;

    case 23:  // obsolete
      p = FIG0Extension23(d);
      break;

    case 24:  // OE services (8.1.10)
      p = FIG0Extension24(d);
      break;

    case 25:  // OE announcement support (8.1.6.3)
      p = FIG0Extension25(d);
      break;

    case 26:  // OE announcement switching (8.1.6.4)
      p = FIG0Extension26(d);
      break;

    default:
      p = false;
      // fprintf (stderr, "FIG0/%d skipped\n", extension);
      break;
  }
  if (!FIG0processingOutput[extension]) {
    // fprintf (stderr, "FIG0/%d %s\n", extension, p ? "processed":"skipped");
    (void)p;
    FIG0processingOutput[extension] = true;
  }
}

//	Ensemble information, 6.4.1
//	FIG0/0 indicated a change in channel organization
//	we are not equipped for that, so we just return
//	control to the init
bool fib_processor::FIG0Extension0(const uint8_t *d) {
  uint16_t EId;
  uint8_t changeflag;
  uint16_t highpart, lowpart;
  int16_t occurrenceChange;
  uint8_t CN = getBits_1(d, 8 + 0);

  (void)CN;
  changeflag = getBits_2(d, 16 + 16);

  EId = getBits(d, 16, 16);
  highpart = getBits_5(d, 16 + 19) % 20;
  lowpart = getBits_8(d, 16 + 24) % 250;

  CIFcount = highpart * 250 + lowpart;
  hasCIFcount = true;
#if OUT_CIF_COUNTER
  fprintf(stderr, "FIG0Extension0: CIFcount %d\n", CIFcount);
#endif

  if (firstTimeEId) {
    idofEnsemble(EId);
    firstTimeEId = false;
  }

  if (changeflag == 0) {
    // fprintf(stderr, "FIG0Extension0: change return\n");
    return true;
  }

  occurrenceChange = getBits_8(d, 16 + 32);
  (void)occurrenceChange;

  if (getBits(d, 34,
              1)) {  // only alarm, just ignore
                     // fprintf(stderr, "FIG0Extension0: alarm return\n");
    return true;
  }

  return true;

  //	if (changeflag == 1) {
  //	   fprintf (stderr, "Changes in sub channel organization\n");
  //	   fprintf (stderr, "cifcount = %d\n", highpart * 250 + lowpart);
  //	   fprintf (stderr, "Change happening in %d CIFs\n", occurrenceChange);
  //	}
  //	else if (changeflag == 3) {
  //	   fprintf (stderr, "Changes in subchannel and service organization\n");
  //	   fprintf (stderr, "cifcount = %d\n", highpart * 250 + lowpart);
  //	   fprintf (stderr, "Change happening in %d CIFs\n", occurrenceChange);
  //	}
  fprintf(stderr, "changes in config not supported, choose again\n");
  //	emit  changeinConfiguration ();
  //
}
//
//	Subchannel organization 6.2.1
//	FIG0 extension 1 creates a mapping between the
//	sub channel identifications and the positions in the
//	relevant CIF.
bool fib_processor::FIG0Extension1(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length - 1)
    used = HandleFIG0Extension1(d, used, CN_bit, OE_bit, PD_bit);
  return true;
}
//
//	defining the channels
int16_t fib_processor::HandleFIG0Extension1(const uint8_t *d, int16_t offset,
                                            uint8_t CN_bit, uint8_t OE_bit,
                                            uint8_t pd) {
  int16_t bitOffset = offset * 8;
  int16_t SubChId = getBits_6(d, bitOffset);
  int16_t StartAdr = getBits(d, bitOffset + 6, 10);
  int16_t tabelIndex;
  int16_t option, protLevel, subChanSize;
  (void)pd;  // not used right now, maybe later

#if OUT_FIG0E1
  const char *wasAlreadyInUse = subChannels[SubChId].inUse ? "used" : "free";
#endif
  subChannels[SubChId].SubChId = SubChId;
  subChannels[SubChId].StartAddr = StartAdr;
  subChannels[SubChId].inUse = true;

  if (getBits_1(d, bitOffset + 16) == 0) {  // short form
    tabelIndex = getBits_6(d, bitOffset + 18);
    int32_t Length = ProtLevel[tabelIndex][0];
    int32_t protLevel = ProtLevel[tabelIndex][1];
    int32_t BitRate = ProtLevel[tabelIndex][2];
#if OUT_FIG0E1
    fprintf(stderr, "FIG0/1: SubChId = %02d (%s), short form, tab idx %d, [len %d, protLevel %d, bitrate %d]\n",
            SubChId, wasAlreadyInUse, tabelIndex, Length, protLevel, BitRate );
#endif

    subChannels[SubChId].shortForm = true;
    subChannels[SubChId].protIdxOrCase = tabelIndex;
    subChannels[SubChId].Length = Length;
    subChannels[SubChId].protLevel = protLevel;
    subChannels[SubChId].subChanSize = 0;
    subChannels[SubChId].BitRate = BitRate;

    bitOffset += 24;
  } else {  // EEP long form
    subChannels[SubChId].shortForm = false;
    option = getBits_3(d, bitOffset + 17);
    if (option == 0) {  // A Level protection
      protLevel = getBits_2(d, bitOffset + 20);
      //
      subChannels[SubChId].protLevel = protLevel;
      subChanSize = getBits(d, bitOffset + 22, 10);
      subChannels[SubChId].Length = subChanSize;
      subChannels[SubChId].subChanSize = subChanSize;
      subChannels[SubChId].protIdxOrCase = 100 + protLevel;
      int32_t BitRate = -1;
      // protLevel == 0: actually protLevel 1
      if (protLevel == 0) BitRate = subChanSize / 12 * 8;
      if (protLevel == 1) BitRate = subChanSize / 8 * 8;
      if (protLevel == 2) BitRate = subChanSize / 6 * 8;
      if (protLevel == 3) BitRate = subChanSize / 4 * 8;
      subChannels[SubChId].BitRate = BitRate;
#if OUT_FIG0E1
      fprintf(stderr, "FIG0/1: SubChId = %02d (%s), EEP-A long form, protLevel %d, #CU %d  %s, bitrate %d\n",
               SubChId, wasAlreadyInUse, protLevel, subChanSize,
               ( (0 <= protLevel && protLevel <= 3) ? Afactors[protLevel] : "?" ),
               BitRate );
#endif
    } else if (option == 001) {  // B Level protection
      protLevel = getBits_2(d, bitOffset + 20);
      //
      //	we encode the B protection levels by adding a 04 to the level
      subChannels[SubChId].protLevel = protLevel + (1 << 2);
      subChanSize = getBits(d, bitOffset + 22, 10);
      subChannels[SubChId].Length = subChanSize;
      subChannels[SubChId].subChanSize = subChanSize;
      subChannels[SubChId].protIdxOrCase = 200 + protLevel;
      int32_t BitRate = -1;
      // protLevel == 0: actually prot level 1
      if (protLevel == 0) BitRate = subChanSize / 27 * 32;
      if (protLevel == 1) BitRate = subChanSize / 21 * 32;
      if (protLevel == 2) BitRate = subChanSize / 18 * 32;
      if (protLevel == 3) BitRate = subChanSize / 15 * 32;
      subChannels[SubChId].BitRate = BitRate;
#if OUT_FIG0E1
      fprintf(stderr, "FIG0/1: SubChId = %02d (%s), EEP-B long form, protLevel %d, #CU %d  %s, bitrate %d\n",
               SubChId, wasAlreadyInUse, protLevel, subChanSize,
               ( (0 <= protLevel && protLevel <= 3) ? Bfactors[protLevel] : "?" ),
               BitRate );
#endif
    } else {
      subChannels[SubChId].protIdxOrCase = 300 + option;
#if OUT_FIG0E1
      fprintf(stderr, "FIG0/1: SubChId = %02d (%s), ???, option %d\n",
               SubChId, wasAlreadyInUse, option );
#endif
    }

    bitOffset += 32;
  }
  return bitOffset / 8;  // we return bytes
}
//
//	Service organization, 6.3.1
//	bind channels to serviceIds
bool fib_processor::FIG0Extension2(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length) {
    used = HandleFIG0Extension2(d, used, CN_bit, OE_bit, PD_bit);
  }

  return true;
}
//
//	Note Offset is in bytes
int16_t fib_processor::HandleFIG0Extension2(const uint8_t *d, int16_t offset,
                                            uint8_t CN_bit, uint8_t OE_bit,
                                            uint8_t pd) {
  int16_t lOffset = 8 * offset;
  int16_t i;
  uint8_t ecc;
  uint8_t cId;
  uint32_t SId;
  int16_t numberofComponents;
  (void)CN_bit;
  (void)OE_bit;

  if (pd == 1) {  // long Sid
    ecc = getBits_8(d, lOffset);
    (void)ecc;
    cId = getBits_4(d, lOffset + 1);
    SId = getLBits(d, lOffset, 32);
    lOffset += 32;
  } else {
    cId = getBits_4(d, lOffset);
    (void)cId;
    SId = getBits(d, lOffset + 4, 12);
    SId = getBits(d, lOffset, 16);
    lOffset += 16;
  }

  numberofComponents = getBits_4(d, lOffset + 4);
  lOffset += 8;

  for (i = 0; i < numberofComponents; i++) {
    uint8_t TMid = getBits_2(d, lOffset);
    if (TMid == 00) {  // Audio
      uint8_t ASCTy = getBits_6(d, lOffset + 2);
      uint8_t SubChId = getBits_6(d, lOffset + 8);
      uint8_t PS_flag = getBits_1(d, lOffset + 14);
      bind_audioService(TMid, SId, i, SubChId, PS_flag, ASCTy);
    } else if (TMid == 3) {  // MSC packet data
      int16_t SCId = getBits(d, lOffset + 2, 12);
      uint8_t PS_flag = getBits_1(d, lOffset + 14);
      uint8_t CA_flag = getBits_1(d, lOffset + 15);
      bind_packetService(TMid, SId, i, SCId, PS_flag, CA_flag);
    } else {
      ;
    }  // for now
    lOffset += 16;
  }
  return lOffset / 8;  // in Bytes
}
//
//	Service component in packet mode 6.3.2
//      The Extension 3 of FIG type 0 (FIG 0/3) gives
//      additional information about the service component
//      description in packet mode.
//      manual: page 55
bool fib_processor::FIG0Extension3(const uint8_t *d) {
  int16_t used = 2;
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length)
    used = HandleFIG0Extension3(d, used, CN_bit, OE_bit, PD_bit);

  return true;
}

//
//      DSCTy   DataService Component Type
int16_t fib_processor::HandleFIG0Extension3(const uint8_t *d, int16_t used,
                                         uint8_t CN_bit, uint8_t OE_bit,
                                         uint8_t PD_bit) {
  int16_t SCId = getBits(d, used * 8, 12);
  int16_t CAOrgflag = getBits_1(d, used * 8 + 15);
  int16_t DGflag = getBits_1(d, used * 8 + 16);
  int16_t DSCTy = getBits_6(d, used * 8 + 18);
  int16_t SubChId = getBits_6(d, used * 8 + 24);
  int16_t packetAddress = getBits(d, used * 8 + 30, 10);
  uint16_t CAOrg = 0;

  serviceComponent *packetComp = find_packetComponent(SubChId, SCId);

  (void)OE_bit;
  (void)PD_bit;

  if (CAOrgflag == 1) {
    CAOrg = getBits(d, used * 8 + 40, 16);
    used += 16 / 8;
  }
  used += 40 / 8;
  (void)CAOrg;
  if (packetComp == nullptr)  // no serviceComponent yet
    return used;

  //      We want to have the subchannel OK
  if (!subChannels[SubChId].inUse) return used;

  //      If the component exists, we first look whether is
  //      was already handled
  if (packetComp->is_madePublic) return used;

  //      if the  Data Service Component Type == 0, we do not deal
  //      with it
  //if (DSCTy == 0) return used;

  serviceId *svc = packetComp->service;
  packetComp->is_madePublic = true;
  packetComp->subchannelId = SubChId;
  packetComp->DSCTy = DSCTy;
  packetComp->DGflag = DGflag;
  packetComp->packetAddress = packetAddress;

  if ( svc && svc->numSubChId < 4 && !existsInTab(svc->numSubChId, svc->SubChId, SubChId) )
    svc->SubChId[svc->numSubChId++] = SubChId;

  if (packetComp->componentNr == 0)  // otherwise sub component
    addtoEnsemble(svc->label, svc->abbr, svc->SId);

  return used;
}
//
//      Service component with CA in stream mode 6.3.3
bool fib_processor::FIG0Extension4(const uint8_t *d) {
  int16_t used = 3;  // offset in bytes
  int16_t Rfa = getBits_1(d, 0);
  int16_t Rfu = getBits_1(d, 0 + 1);
  int16_t SubChId = getBits_6(d, 0 + 1 + 1);
  int32_t CAOrg = getBits(d, 2 + 6, 16);

  //      fprintf(stderr,"FIG0/4: Rfa=\t%D, Rfu=\t%d, SudChId=\t%02X,
  //      CAOrg=\t%04X\n", Rfa, Rfu, SubChId, CAOrg);
  (void)used;
  (void)Rfa;
  (void)Rfu;
  (void)SubChId;
  (void)CAOrg;

  return false;  // npthing happens with extracted bits
}
//
//	Service component language
bool fib_processor::FIG0Extension5(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length) {
    used = HandleFIG0Extension5(d, CN_bit, OE_bit, PD_bit, used);
  }

  return true;
}

int16_t fib_processor::HandleFIG0Extension5(const uint8_t *d, uint8_t CN_bit,
                                         uint8_t OE_bit, uint8_t PD_bit,
                                         int16_t offset) {
  int16_t loffset = offset * 8;
  uint8_t lsFlag = getBits_1(d, loffset);
  int16_t subChId, serviceComp, language;

  if (lsFlag == 0) {  // short form
    if (getBits_1(d, loffset + 1) == 0) {
      subChId = getBits_6(d, loffset + 2);
      language = getBits_8(d, loffset + 8);
      subChannels[subChId].language = language;
      fprintf(stderr, "FIG0Ext5: Short, SubChId %d, language %d\n", (int)subChId, (int)language);
    }
    loffset += 16;
  } else {  // long form
    serviceComp = getBits(d, loffset + 4, 12);
    language = getBits_8(d, loffset + 16);
    fprintf(stderr, "FIG0Ext5: Long, serviceComp %d, language %d\n", (int)serviceComp, (int)language);
    loffset += 24;
  }
  (void)serviceComp;
  return loffset / 8;
}

// FIG0/6: Service linking information 8.1.15, not implemented
bool fib_processor::FIG0Extension6(const uint8_t *d) {
  (void)d;
  return false;
}

// FIG0/7: Configuration linking information 6.4.2, not implemented
bool fib_processor::FIG0Extension7(const uint8_t *d) {
  (void)d;
  return false;
}

bool fib_processor::FIG0Extension8(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length) {
    used = HandleFIG0Extension8(d, used, CN_bit, OE_bit, PD_bit);
  }

  return true;
}

int16_t fib_processor::HandleFIG0Extension8(const uint8_t *d, int16_t used,
                                            uint8_t CN_bit, uint8_t OE_bit,
                                            uint8_t pdBit) {
  int16_t lOffset = used * 8;
  uint32_t SId = getLBits(d, lOffset, pdBit == 1 ? 32 : 16);
  uint8_t lsFlag;
  int16_t SCIds;
  int16_t SCid;
  int16_t MSCflag;
  int16_t SubChId;
  uint8_t extensionFlag;

  (void)OE_bit;
  lOffset += pdBit == 1 ? 32 : 16;
  extensionFlag = getBits_1(d, lOffset);
  SCIds = getBits_4(d, lOffset + 4);
  lOffset += 8;

  serviceId *svc = findServiceId(SId);
  if ( svc && svc->numSCIds < 4 && !existsInTab(svc->numSCIds, svc->SCIds, SCIds) )
    svc->SCIds[svc->numSCIds++] = SCIds;

  lsFlag = getBits_1(d, lOffset);
  if (lsFlag == 1) {
    SCid = getBits(d, lOffset + 4, 12);
    lOffset += 16;
    if ( svc && svc->numSCid < 4 && !existsInTab(svc->numSCid, svc->SCid, SCid) )
      svc->SCid[svc->numSCid++] = SCid;
    //           if (find_packetComponent ((SCIds << 4) | SCid) != nullptr) {
    //              fprintf (stderr, "packet component bestaat !!\n");
    //           }
  } else {
    MSCflag = getBits_1(d, lOffset + 1);
    SubChId = getBits_6(d, lOffset + 2);
    lOffset += 8;
    if ( svc && svc->numSubChId < 4 && !existsInTab(svc->numSubChId, svc->SubChId, SubChId) )
      svc->SubChId[svc->numSubChId++] = SubChId;
  }
  if (extensionFlag) lOffset += 8;  // skip Rfa
  (void)SId;
  (void)SCIds;
  (void)SCid;
  (void)SubChId;
  (void)MSCflag;
  return lOffset / 8;
}
//
//	Country, LTO & international table 8.1.3.2
//	FIG0/9 and FIG0/10 are copied from the work of
//	Michael Hoehn
bool fib_processor::FIG0Extension9(const uint8_t *d) {
  int16_t offset = 16;

  dateTime[6] = (getBits_1(d, offset + 2) == 1) ? -1 * getBits_4(d, offset + 3)
                                                : getBits_4(d, offset + 3);
  dateTime[7] = (getBits_1(d, offset + 7) == 1) ? 30 : 0;
  uint16_t ecc = getBits(d, offset + 8, 8);

  uint16_t internationalTabId = getBits(d, offset + 16, 8);
  interTabId = internationalTabId & 0xFF;
  interTab_Present = true;

  if (!ecc_Present) {
    ecc_byte = ecc & 0xFF;
    ecc_Present = true;
  }

  return true;
}

//
bool fib_processor::FIG0Extension10(const uint8_t *fig) {
  int16_t offset = 16;
  int32_t mjd = getLBits(fig, offset + 1, 17);
  // Modified Julian Date umrechnen (Nach wikipedia)
  int32_t J = mjd + 2400001;
  int32_t j = J + 32044;
  int32_t g = j / 146097;
  int32_t dg = j % 146097;
  int32_t c = ((dg / 36524) + 1) * 3 / 4;
  int32_t dc = dg - c * 36524;
  int32_t b = dc / 1461;
  int32_t db = dc % 1461;
  int32_t a = ((db / 365) + 1) * 3 / 4;
  int32_t da = db - a * 365;
  int32_t y = g * 400 + c * 100 + b * 4 + a;
  int32_t m = ((da * 5 + 308) / 153) - 2;
  int32_t d = da - ((m + 4) * 153 / 5) + 122;
  int32_t Y = y - 4800 + ((m + 2) / 12);
  int32_t M = ((m + 2) % 12) + 1;
  int32_t D = d + 1;

  dateTime[0] = Y;                            // Jahr
  dateTime[1] = M;                            // Monat
  dateTime[2] = D;                            // Tag
  dateTime[3] = getBits_5(fig, offset + 21);  // Stunden
  if (getBits_6(fig, offset + 26) != dateTime[4])
    dateTime[5] = 0;  // Sekunden (Uebergang abfangen)

  dateTime[4] = getBits_6(fig, offset + 26);  // Minuten
  if (fig[offset + 20] == 1)
    dateTime[5] = getBits_6(fig, offset + 32);  // Sekunden
  dateFlag = true;
  //	emit newDateTime (dateTime);

  return true;
}
//
//
bool fib_processor::FIG0Extension11(const uint8_t *d) {
  (void)d;
  return false;
}
//
//
bool fib_processor::FIG0Extension12(const uint8_t *d) {
  (void)d;
  return false;
}
//
//
bool fib_processor::FIG0Extension13(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length)
    used = HandleFIG0Extension13(d, used, CN_bit, OE_bit, PD_bit);

  return true;
}

int16_t fib_processor::HandleFIG0Extension13(const uint8_t *d, int16_t used,
                                             uint8_t CN_bit, uint8_t OE_bit,
                                             uint8_t pdBit) {
  int16_t lOffset = used * 8;
  uint32_t SId = getLBits(d, lOffset, pdBit == 1 ? 32 : 16);

  lOffset += pdBit == 1 ? 32 : 16;
  int16_t SCIds = getBits_4(d, lOffset);
  int16_t NoApplications = getBits_4(d, lOffset + 4);
  lOffset += 8;

  for (int16_t i = 0; i < NoApplications; i++) {
    int16_t appType = getBits(d, lOffset, 11);
    int16_t length = getBits_5(d, lOffset + 11);
    lOffset += (11 + 5 + 8 * length);
    serviceComponent *packetComp = find_serviceComponent(SId, SCIds);
    if (packetComp != nullptr) {
      packetComp->SCIds = SCIds;
      packetComp->appType = appType;
    }
  }

  return lOffset / 8;
}
//
//      FEC sub-channel organization 6.2.2
bool fib_processor::FIG0Extension14(const uint8_t *d) {
  int16_t Length = getBits_5(d, 3);  // in Bytes
  int16_t used = 2;                  // in Bytes
  int16_t i;

  while (used < Length) {
    int16_t SubChId = getBits_6(d, used * 8);
    uint8_t FEC_scheme = getBits_2(d, used * 8 + 6);
    used = used + 1;
    for (i = 0; i < 64; i++) {
      if (subChannels[i].SubChId == SubChId) {
        subChannels[i].FEC_scheme = FEC_scheme;
      }
    }
  }

  return true;
}

bool fib_processor::FIG0Extension15(const uint8_t *d) {
  (void)d;
  return false;
}
//
//      Obsolete in ETSI EN 300 401 V2.1.1 (2017-01)
bool fib_processor::FIG0Extension16(const uint8_t *d) {
  int16_t length = getBits_5(d, 3);  // in bytes
  int16_t offset = 16;               // in bits
  serviceId *s;

  while (offset < length * 8) {
    uint16_t SId = getBits(d, offset, 16);
    s = findServiceId(SId);
    if (!s->hasPNum) {
      uint8_t PNum = getBits(d, offset + 16, 16);
      s->pNum = PNum;
      s->hasPNum = true;
      fprintf(stderr, "FIG0Ext16: Program number info for SID %08X, PNum %d\n", SId, (int)PNum);
    }
    offset += 72;
  }

  return true;
}
//
//      Programme Type (PTy) 8.1.5
bool fib_processor::FIG0Extension17(const uint8_t *d) {
  int16_t length = getBits_5(d, 3);
  int16_t offset = 16;
  serviceId *s;

  while (offset < length * 8) {
    uint16_t SId = getBits(d, offset, 16);
    bool L_flag = getBits_1(d, offset + 18);
    bool CC_flag = getBits_1(d, offset + 19);
    int16_t type;
    int16_t Language = 0x00;  // init with unknown language
    s = findServiceId(SId);
    if (L_flag) {  // language field present
      Language = getBits_8(d, offset + 24);
      s->language = Language;
      s->hasLanguage = true;
      offset += 8;
    }

    type = getBits_5(d, offset + 27);
    s->programType = type;
    if (CC_flag)  // cc flag
      offset += 40;
    else
      offset += 32;
  }

  return true;
}
//
//      Announcement support 8.1.6.1
bool fib_processor::FIG0Extension18(const uint8_t *d) {
  int16_t offset = 16;  // bits
  uint16_t SId, AsuFlags;
  int16_t Length = getBits_5(d, 3);

  while (offset / 8 < Length - 1) {
    int16_t NumClusters = getBits_5(d, offset + 35);
    SId = getBits(d, offset, 16);
    AsuFlags = getBits(d, offset + 16, 16);
    //	   fprintf (stderr, "Announcement %d for SId %d with %d clusters\n",
    //	                    AsuFlags, SId, NumClusters);
    offset += 40 + NumClusters * 8;
  }
  (void)SId;
  (void)AsuFlags;

  return true;
}
//
//      Announcement switching 8.1.6.2
bool fib_processor::FIG0Extension19(const uint8_t *d) {
  int16_t offset = 16;  // bits
  uint16_t AswFlags;
  int16_t Length = getBits_5(d, 3);
  uint8_t region_Id_Lower;

  while (offset / 8 < Length - 1) {
    uint8_t ClusterId = getBits_8(d, offset);
    bool new_flag = getBits_1(d, offset + 24);
    bool region_flag = getBits_1(d, offset + 25);
    uint8_t SubChId = getBits_6(d, offset + 26);

    AswFlags = getBits(d, offset + 8, 16);
    //	   fprintf (stderr,
    //	          "%s %s Announcement %d for Cluster %2u on SubCh %2u ",
    //	              ((new_flag==1)?"new":"old"),
    //	              ((region_flag==1)?"regional":""),
    //	              AswFlags, ClusterId,SubChId);
    if (region_flag) {
      region_Id_Lower = getBits_6(d, offset + 34);
      offset += 40;
      //           fprintf(stderr,"for region %u",region_Id_Lower);
    } else
      offset += 32;

    //	   fprintf(stderr,"\n");
    (void)ClusterId;
    (void)new_flag;
    (void)SubChId;
  }
  (void)AswFlags;
  (void)region_Id_Lower;

  return true;
}
//
//      Service component information 8.1.4
bool fib_processor::FIG0Extension20(const uint8_t *d) {
  (void)d;
  return false;
}
//
//	Frequency information (FI) 8.1.8
bool fib_processor::FIG0Extension21(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length)
    used = HandleFIG0Extension21(d, CN_bit, OE_bit, PD_bit, used);

  return true;
}

int16_t fib_processor::HandleFIG0Extension21(const uint8_t *d, uint8_t CN_bit,
                                             uint8_t OE_bit, uint8_t PD_bit,
                                             int16_t offset) {
  int16_t l_offset = offset * 8;
  int16_t l = getBits_5(d, l_offset + 11);
  int16_t upperLimit = l_offset + 16 + l * 8;
  int16_t base = l_offset + 16;

  (void)CN_bit;
  (void)OE_bit, (void)PD_bit;

  while (base < upperLimit) {
    //uint16_t idField = getBits(d, base, 16);
    uint8_t RandM = getBits_4(d, base + 16);
    uint8_t continuity = getBits_1(d, base + 20);
    (void)continuity;
    uint8_t length = getBits_3(d, base + 21);
    if (RandM == 0x08) {
      uint16_t fmFrequency_key = getBits(d, base + 24, 8);
      int32_t fmFrequency = 87500 + fmFrequency_key * 100;
      (void)fmFrequency;
      //int16_t serviceIndex = findService(idField);
      //if (serviceIndex != -1) {
      //  if ((ensemble->services[serviceIndex].hasName) &&
      //      (ensemble->services[serviceIndex].fmFrequency == -1))
      //    ensemble->services[serviceIndex].fmFrequency = fmFrequency;
      //}
    }
    base += 24 + length * 8;
  }

  return upperLimit / 8;
}

//
//      Obsolete in ETSI EN 300 401 V2.1.1 (2017-01)
bool fib_processor::FIG0Extension22(const uint8_t *d) {
  int16_t used = 2;  // offset in bytes
  int16_t Length = getBits_5(d, 3);
  uint8_t CN_bit = getBits_1(d, 8 + 0);
  uint8_t OE_bit = getBits_1(d, 8 + 1);
  uint8_t PD_bit = getBits_1(d, 8 + 2);

  while (used < Length)
    used = HandleFIG0Extension22(d, CN_bit, OE_bit, PD_bit, used);

  return true;
}

int16_t fib_processor::HandleFIG0Extension22(const uint8_t *d, uint8_t CN_bit,
                                          uint8_t OE_bit, uint8_t PD_bit,
                                          int16_t used) {
  uint8_t MS;
  int16_t mainId;
  int16_t noSubfields;
  int i;

  mainId = getBits_7(d, used * 8 + 1);
  (void)mainId;
  MS = getBits_1(d, used * 8);
  if (MS == 0) {  // fixed size
    int16_t latitudeCoarse = getBits(d, used * 8 + 8, 16);
    int16_t longitudeCoarse = getBits(d, used * 8 + 24, 16);

    coordinates.add_main(mainId, latitudeCoarse * 90.0 / 32768.0,
                         longitudeCoarse * 180.0 / 32768.0);
    return used + 48 / 6;
  }

  //	MS == 1
  noSubfields = getBits_3(d, used * 8 + 13);
  for (i = 0; i < noSubfields; i++) {
    int16_t subId = getBits_5(d, used * 8 + 16 + i * 48);
    int16_t TD = getBits(d, used * 8 + 16 + i * 48 + 5, 11);
    int16_t latOff = getBits(d, used * 8 + 16 + i * 48 + 16, 16);
    int16_t lonOff = getBits(d, used * 8 + 16 + i * 48 + 32, 16);
    tii_element s(subId, TD, latOff * 90 / (16 * 32768.0),
                  lonOff * 180 / (16 * 32768.0));
    coordinates.add_element(&s);
  }

  used += (16 + noSubfields * 48) / 8;
  return used;
}
//
//
bool fib_processor::FIG0Extension23(const uint8_t *d) {
  (void)d;
  return false;
}
//
//      OE Services
bool fib_processor::FIG0Extension24(const uint8_t *d) {
  (void)d;
  return false;
}
//
//      OE Announcement support
bool fib_processor::FIG0Extension25(const uint8_t *d) {
  (void)d;
  return false;
}
//
//      OE Announcement Switching
bool fib_processor::FIG0Extension26(const uint8_t *d) {
  (void)d;
  return false;
}


static void writeLabel( const uint8_t *d, int16_t offset, char *label, char *abbrev
, uint32_t SId, int subID, const char * labelId, int printToConsole = 0 )
{
  for ( int16_t i = 0; i < 16; ++i )
    label[i] = getBits_8(d, offset + 8 * i);

  uint32_t char_flagfield = getLBits(d, offset + 16*8, 16);
  int16_t abbrLen = 0;
#if 1
  for ( int16_t i = 0; i < 16; ++i ) {
    if ( char_flagfield & CHAR_POS_MASK(i) )
      abbrev[abbrLen++] = ABBR_CHAR(i,label);
  }
  abbrev[abbrLen++] = 0;
#else
  char abbrCt[32];

  for ( int16_t i = 0; i < 16; ++i ) {
    if ( char_flagfield & CHAR_POS_MASK(i) ) {
      abbrev[abbrLen] = ABBR_POS_CHAR(i,label);
      abbrCt[abbrLen++] = ABBR_CHAR(i,label);
    }
  }
  abbrCt[abbrLen] = 0;
  abbrev[abbrLen++] = '#';
  abbrev[abbrLen] = hexTab[abbrLen-1];
  abbrev[++abbrLen] = '_';
  abbrev[++abbrLen] = 0;
  strcat(abbrev, abbrCt);
#endif

  if ( printToConsole ) {
    fprintf(stderr, "LABEL SID %08X: subID %d: decoded %s: '%s' / '%s'\n", SId, subID, labelId, label, abbrev);
  }
}


//      FIG 1 - Cover the different possible labels, section 5.2
//
void fib_processor::process_FIG1(const uint8_t *d) {
  uint8_t charSet, extension;
  uint32_t SId = 0;
  uint8_t Rfu;
  int16_t offset = 0;
  serviceId *myIndex;
  uint8_t pd_flag;
  uint8_t SCidS;
  uint8_t XPAD_aid;
  uint8_t region_id;
  char abbrev[64];
  char label[32];
  //
  //	from byte 1 we deduce:
  charSet = getBits_4(d, 8);
  Rfu = getBits_1(d, 8 + 4);  // == OE = Other Ensemble
  extension = getBits_3(d, 8 + 5);
  label[16] = 0;
  abbrev[0] = 0;
  (void)Rfu;
  switch (extension) {
      /* default: return; */
    case 0:  // ensemble label
      SId = getBits(d, 16, 16);
      offset = 32;
      if ((charSet <= 16)) {  // EBU Latin based repertoire

        writeLabel( d, offset,  label, abbrev, SId, -1, "ext0: ensemble label", 0 & OUT_ALL_LABELS );
        // fprintf (stderr, "Ensemblename: %16s\n", label);
        if (UnicodeUcs2 == (CharacterSet)charSet)
          fprintf(stderr,
                  "warning: ignoring ensemble name cause of "
                  "unimplemented Ucs2 conversion\n");
        else {
          std::string name = toStringUsingCharset(label, (CharacterSet)charSet);
          std::string abbr = toStringUsingCharset(abbrev, (CharacterSet)charSet);
          // without idofEnsemble: just report one name
          if (ensembleidHandler != nullptr || firstTimeEName)
            nameofEnsemble(SId, name, abbr);
          firstTimeEName = false;
        }
        isSynced = true;
      }
      // fprintf (stderr, "charset %d is used for ensemblename\n", charSet);
      break;

    case 1:  // 16 bit Identifier field for service label 8.1.14.1 Programme service label
      SId = getBits(d, 16, 16);
      offset = 32;
      myIndex = findServiceId(SId);
      if ( myIndex && (!myIndex->hasName) && (charSet <= 16)) {

        writeLabel( d, offset,  label, abbrev, SId, -1, "ext1: programme name", 0 & OUT_ALL_LABELS );

        if (UnicodeUcs2 == (CharacterSet)charSet)
          fprintf(stderr,
                  "warning: ignoring service label cause of "
                  "unimplemented Ucs2 conversion\n");
        else {
          std::string appStr = toStringUsingCharset(label, (CharacterSet)charSet);
          std::string abbr = toStringUsingCharset(abbrev, (CharacterSet)charSet);
          strcat(myIndex->label, appStr.c_str());
          strcat(myIndex->abbr, abbr.c_str());
          // fprintf (stderr, "FIG1/1: SId = %4x\t%s\n", SId, label);
          myIndex->hasName = true;
        }
      }
      break;

    case 3:
      // region label
      region_id = getBits_6(d, 16 + 2);
      (void)region_id;
      offset = 24;
      writeLabel( d, offset,  label, abbrev, SId, region_id, "ext3: region", OUT_ALL_LABELS );
      // fprintf (stderr, "FIG1/3: RegionID = %2x\t%s\n", region_id, label);
      break;

    case 4:  // service component label 8.1.14.3 Service component label
      pd_flag = getLBits(d, 16, 1);
      SCidS = getLBits(d, 20, 4);
      if (pd_flag) {  // 32 bit identifier field for service component label
        SId = getLBits(d, 24, 32);
        offset = 56;
      } else {  // 16 bit identifier field for service component label
        SId = getLBits(d, 24, 16);
        offset = 40;
      }

      writeLabel( d, offset,  label, abbrev, SId, SCidS, "ext4: service component", 0 && OUT_ALL_LABELS );
      //fprintf (stderr, "FIG1/4: Sid = %8x\tp/d=%d\tSCidS=%1X\tflag=%8X\t%s\n", SId, pd_flag, SCidS, flagfield, label);

      if ((charSet <= 16) && UnicodeUcs2 != (CharacterSet)charSet ) {
        // EBU Latin based repertoire
        serviceComponent *packetComp = find_serviceComponentNr(SId, SCidS);
        if ( packetComp && !packetComp->hasLabel ) {
          std::string name = toStringUsingCharset(label, (CharacterSet)charSet);
          std::string abbr = toStringUsingCharset(abbrev, (CharacterSet)charSet);
          strcpy( packetComp->label, name.c_str() );
          strcpy( packetComp->abbr, abbr.c_str() );
          packetComp->hasLabel = true;
        }
      }
      break;

    case 5:  // Data service label - 32 bits 8.1.14.2 Data service label
      SId = getLBits(d, 16, 32);
      offset = 48;
      myIndex = findServiceId(SId);
      if ( myIndex && (!myIndex->hasName) && (charSet <= 16)) {

        writeLabel( d, offset,  label, abbrev, SId, -1, "ext5: data service", 0 && OUT_ALL_LABELS );

        if (UnicodeUcs2 == (CharacterSet)charSet)
          fprintf(stderr,
                  "warning: ignoring service name/label cause of "
                  "unimplemented Ucs2 conversion\n");
        else {
          std::string appStr = toStringUsingCharset(label, (CharacterSet)charSet);
          std::string abbr = toStringUsingCharset(abbrev, (CharacterSet)charSet);
          strcat(myIndex->label, appStr.c_str());
          strcat(myIndex->abbr, abbr.c_str());
          myIndex->hasName = true;
          addtoEnsemble(myIndex->label, myIndex->abbr, SId);
        }
      }
      break;

    case 6:  // XPAD label - 8.1.14.4 X-PAD user application label
      pd_flag = getLBits(d, 16, 1);
      SCidS = getLBits(d, 20, 4);
      if (pd_flag) {  // 32 bits identifier for XPAD label
        SId = getLBits(d, 24, 32);
        XPAD_aid = getLBits(d, 59, 5);
        offset = 64;
      } else {  // 16 bit identifier for XPAD label
        SId = getLBits(d, 24, 16);
        XPAD_aid = getLBits(d, 43, 5);
        offset = 48;
      }

      writeLabel( d, offset,  label, abbrev, SId, XPAD_aid, "ext6: XPAD", OUT_ALL_LABELS );
      // fprintf (stderr, "FIG1/6: SId = %8x\tp/d = %d\t SCidS =
      //%1X\tXPAD_aid = %2u\t%s\n", 		       SId, pd_flag, SCidS,
      // XPAD_aid, label);
      break;

    default:
      //	      fprintf (stderr, "FIG1/%d: not handled now\n", extension);
      break;
  }
  (void)SCidS;
  (void)XPAD_aid;
}

#define FULL_MATCH 0100
#define PREFIX_MATCH 0200
#define NO_MATCH 0000

//	tricky: the names in the directoty contain spaces at the end
static int compareNames(std::string in, std::string ref) {
  if (ref == in) return FULL_MATCH;

  if (ref.length() < in.length()) return NO_MATCH;

  if (ref.find(in, 0) != 0) return NO_MATCH;

  if (in.length() == ref.length()) return FULL_MATCH;
  //
  //	Most likely we will find a prefix as match, since the
  //	FIC structure fills the service names woth spaces to 16 letters
  if (ref.at(in.length()) == ' ') return FULL_MATCH;

  return PREFIX_MATCH;
}

//	locate - and create if needed - a reference to the entry
//	for the serviceId serviceId
serviceId *fib_processor::findServiceId(int32_t serviceId) {
  int16_t i;

  for (i = 0; i < 64; i++) {
    if ((listofServices[i].inUse) &&
        (listofServices[i].SId == serviceId))
      return &listofServices[i];
  }

  for (i = 0; i < 64; i++) {
    if (!listofServices[i].inUse) {
      listofServices[i].inUse = true;
      listofServices[i].SId = serviceId;
      return &listofServices[i];
    }
  }

  listofServices[64].inUse = false;
  return &listofServices[64];  // should not happen
}
//
//	since some servicenames are long, we allow selection of a
//	service based on the first few letters/digits of the name.
//	However, in case of servicenames where one is a prefix
//	of the other, the full match should have precedence over the
//	prefix match
serviceId *fib_processor::findServiceId(std::string serviceName, bool fullMatchOnly) {
  int16_t i;
  int indexforprefixMatch = -1;

  for (i = 0; i < 64; i++) {
    if (listofServices[i].inUse) {
      int res = compareNames(serviceName, listofServices[i].label);
      if (res == FULL_MATCH) {
        return &listofServices[i];
      }
      if (res == PREFIX_MATCH) {
        indexforprefixMatch = i;
      }
    }
  }

  if (fullMatchOnly)
      return nullptr;
  return indexforprefixMatch >= 0 ? &listofServices[indexforprefixMatch]
                                  : nullptr;
}

serviceComponent *fib_processor::find_packetComponent(int16_t SubChId, int16_t SCId) {
  int16_t i;

  for (i = 0; i < 64; i++) {
    if (!ServiceComps[i].inUse) continue;
    if (ServiceComps[i].TMid != 03) continue;
    //if (ServiceComps[i].subchannelId == SubChId && ServiceComps[i].SCId == SCId)
    if (ServiceComps[i].SCId == SCId)
      return &ServiceComps[i];
  }
  return nullptr;
}

serviceComponent *fib_processor::find_serviceComponent(int32_t SId,
                                                       int16_t SCIdS) {
  int16_t i;
  (void)SCIdS;

  for (i = 0; i < 64; i++) {
    if (!ServiceComps[i].inUse) continue;

    if ( ServiceComps[i].service
      && ServiceComps[i].service->inUse
      && ServiceComps[i].service->SId == SId )
    {
      return &ServiceComps[i];
    }
  }

  return nullptr;
}

serviceComponent *fib_processor::find_serviceComponentNr(int32_t SId,
                                                int16_t componentNr) {
  int16_t i;

  for (i = 0; i < 64; i++) {
    if (!ServiceComps[i].inUse) continue;

    if ( ServiceComps[i].componentNr == componentNr
      && ServiceComps[i].service
      && ServiceComps[i].service->inUse
      && ServiceComps[i].service->SId == SId )
    {
      return &ServiceComps[i];
    }
  }

  return nullptr;
}

serviceComponent *find_serviceComponentNr(int32_t SId, int16_t componentNr);

//	bind_audioService is the main processor for - what the name suggests -
//	connecting the description of audioservices to a SID
void fib_processor::bind_audioService(int8_t TMid, uint32_t SId, int16_t compnr,
                                      int16_t SubChId, int16_t ps_flag,
                                      int16_t ASCTy) {
  serviceId *s = findServiceId(SId);
  int16_t i;
  int16_t firstFree = -1;

  if (!s->hasName) return;

  if (!subChannels[SubChId].inUse) return;

  for (i = 0; i < 64; i++) {
    if (!ServiceComps[i].inUse) {
      if (firstFree == -1) firstFree = i;
      continue;
    }
    if ((ServiceComps[i].service == s) &&
        (ServiceComps[i].componentNr == compnr))
      return;
  }

  addtoEnsemble(s->label, s->abbr, s->SId);

  ServiceComps[firstFree].inUse = true;
  ServiceComps[firstFree].compType = 'A';
  ServiceComps[firstFree].TMid = TMid;
  ServiceComps[firstFree].componentNr = compnr;
  ServiceComps[firstFree].service = s;
  ServiceComps[firstFree].subchannelId = SubChId;
  ServiceComps[firstFree].PS_flag = ps_flag;
  ServiceComps[firstFree].ASCTy = ASCTy;
}

//      bind_packetService is the main processor for - what the name suggests -
//      connecting the service component defining the service to the SId,
///     Note that the subchannel is assigned through a FIG0/3
void fib_processor::bind_packetService(int8_t TMid, uint32_t SId,
                                       int16_t compnr, int16_t SCId,
                                       int16_t ps_flag, int16_t CAflag) {
  serviceId *s = findServiceId(SId);
  int16_t i;
  int16_t firstFree = -1;

  // no need to wait for serviceLabel
  // Jan: if (!ensemble -> services [serviceIndex]. hasName)  - with class ensembleDescriptor * ensemble
  if (!s || !s->hasName)  // wait until we have a name
    return;

  for (i = 0; i < 64; i++) {
    if ( ServiceComps[i].inUse
        && ServiceComps[i].service == s  // this was 2nd source of bug missing PACKET DATA
                                         // see https://github.com/JvanKatwijk/dab-cmdline/pull/69
        && ServiceComps[i].componentNr == compnr && ServiceComps[i].SCId == SCId )
      return;

    if (!ServiceComps[i].inUse) {
      if (firstFree == -1) firstFree = i;
      continue;
    }
  }

  ServiceComps[firstFree].inUse = true;
  ServiceComps[firstFree].compType = 'P';
  ServiceComps[firstFree].TMid = TMid;
  ServiceComps[firstFree].service = s;
  ServiceComps[firstFree].componentNr = compnr;
  ServiceComps[firstFree].SCId = SCId;
  ServiceComps[firstFree].PS_flag = ps_flag;
  ServiceComps[firstFree].CAflag = CAflag;
}

void fib_processor::clearEnsemble(void) {
  int16_t i;
  isSynced = false;
  coordinates.cleanUp();
  for (i = 0; i < 64; i++)
    listofServices[i].clear();
  for (i = 0; i < 64; i++)
    ServiceComps[i].clear();
  for (i = 0; i < 64; i++)
    subChannels[i].clear();
  firstTimeEId = true;
  firstTimeEName = true;
}

std::string fib_processor::nameFor(int32_t serviceId) {
  int16_t i;

  for (i = 0; i < 64; i++) {
    if (!listofServices[i].inUse) continue;

    if (!listofServices[i].hasName) continue;

    if (listofServices[i].SId == serviceId) {
      return listofServices[i].label;
    }
  }
  return "no service found";
}

int32_t fib_processor::SIdFor(std::string &name) {
  int16_t i;
  int serviceIndex = -1;

  for (i = 0; i < 64; i++) {
    if (!listofServices[i].inUse) continue;

    if (!listofServices[i].hasName) continue;

    int res = compareNames(name, listofServices[i].label);
    if (res == NO_MATCH) continue;
    if (res == PREFIX_MATCH) {
      serviceIndex = i;
      continue;
    }
    //	it is a FULL match:
    return listofServices[i].SId;
  }
  if (serviceIndex >= 0) return listofServices[serviceIndex].SId;
  return -1;
}
//
//	Here we look for a primary service only
uint8_t fib_processor::kindofService(std::string &s) {
  int16_t i, j;
  int16_t service = UNKNOWN_SERVICE;
  int32_t selectedService = -1;
  int serviceIndex = -1;

  fibLocker.lock();
  //	first we locate the serviceId
  for (i = 0; i < 64; i++) {
    int res;
    if (!listofServices[i].inUse) continue;

    if (!listofServices[i].hasName) continue;

    res = compareNames(s, listofServices[i].label);
    if (res == NO_MATCH) continue;
    if (res == PREFIX_MATCH) {
      serviceIndex = i;
      continue;
    }
    serviceIndex = i;
    break;
  }

  if (serviceIndex != -1) {
    selectedService = listofServices[serviceIndex].SId;
    for (j = 0; j < 64; j++) {
      if (!ServiceComps[j].inUse) continue;
      if (selectedService != ServiceComps[j].service->SId)
        continue;

      if (ServiceComps[j].componentNr != 0) continue;

      if (ServiceComps[j].TMid == 03) {
        service = PACKET_SERVICE;
        break;
      }

      if (ServiceComps[j].TMid == 00) {
        service = AUDIO_SERVICE;
        break;
      }
    }
  }
  fibLocker.unlock();
  return service;
}
//
//	Here we look for a primary service only
uint8_t fib_processor::kindofService(int32_t SId) {
  int16_t i, j;
  int16_t service = UNKNOWN_SERVICE;
  int32_t selectedService = -1;
  int serviceIndex = -1;

  fibLocker.lock();
  //	first we locate the serviceId
  for (i = 0; i < 64; i++) {
    if (!listofServices[i].inUse) continue;
    if (!listofServices[i].hasName) continue;
    if ( listofServices[i].SId != SId ) continue;
    serviceIndex = i;
    break;
  }

  if (serviceIndex != -1) {
    selectedService = listofServices[serviceIndex].SId;
    for (j = 0; j < 64; j++) {
      if (!ServiceComps[j].inUse) continue;
      if (selectedService != ServiceComps[j].service->SId)
        continue;

      if (ServiceComps[j].componentNr != 0) continue;

      if (ServiceComps[j].TMid == 03) {
        service = PACKET_SERVICE;
        break;
      }

      if (ServiceComps[j].TMid == 00) {
        service = AUDIO_SERVICE;
        break;
      }
    }
  }
  fibLocker.unlock();
  return service;
}

void fib_processor::dataforDataService(std::string &s, packetdata *d) {
  dataforDataService(s, d, 0);
}

void fib_processor::dataforDataService(std::string &s, packetdata *d,
                                       int16_t compnr) {
  serviceId *selectedService;

  d->defined = false;  // always a decent default
  fibLocker.lock();
  const bool fullMatchOnly = true;
  selectedService = findServiceId(s, fullMatchOnly);
  if (selectedService == nullptr) {
    fibLocker.unlock();
    return;
  }
  dataforDataService(selectedService, d, compnr);
  fibLocker.unlock();
}

void fib_processor::dataforDataService(int SId, packetdata *d,
                                        int16_t compnr) {
  serviceId *selectedService;

  d->defined = false;
  fibLocker.lock();
  selectedService = findServiceId(SId);
  if (selectedService == nullptr) {
    fibLocker.unlock();
    return;
  }
  dataforDataService(selectedService, d, compnr);
  fibLocker.unlock();
}

void fib_processor::dataforDataService(serviceId *selectedService, packetdata *d,
                                       int16_t compnr) {
  int16_t j;
  //const int16_t SCid = ( selectedService && selectedService->numSCid > 0 ) ? selectedService->SCid[0] : -1;

  for (j = 0; j < 64; j++) {
    if ((!ServiceComps[j].inUse) || (ServiceComps[j].TMid != 03)) continue;

    if (ServiceComps[j].componentNr != compnr) continue;

    if (selectedService != ServiceComps[j].service) continue;

    const int16_t subchId = ServiceComps[j].subchannelId;

    int16_t subChanIdx = -1;
    if ( 0 <= subchId && subchId < 64 )
      subChanIdx = subchId;

    d->subchId = subChanIdx;

    d->startAddr = -1;
    d->shortForm = false;
    d->protLevel = -1;
    d->length = -1;
    d->subChanSize = -1;
    d->bitRate = -1;
    d->FEC_scheme = -1;
    d->protIdxOrCase = -1;

    if ( 0 <= subChanIdx && subChanIdx < 64 ) {
      d->startAddr = subChannels[subChanIdx].StartAddr;
      d->shortForm = subChannels[subChanIdx].shortForm;
      d->protLevel = subChannels[subChanIdx].protLevel;
      d->length = subChannels[subChanIdx].Length;
      d->subChanSize = subChannels[subChanIdx].subChanSize;
      d->bitRate = subChannels[subChanIdx].BitRate;
      d->FEC_scheme = subChannels[subChanIdx].FEC_scheme;
      d->protIdxOrCase = subChannels[subChanIdx].protIdxOrCase;
    }
    d->componentNr = ServiceComps[j].componentNr;
    d->DSCTy = ServiceComps[j].DSCTy;
    d->DGflag = ServiceComps[j].DGflag;
    d->packetAddress = ServiceComps[j].packetAddress;
    d->appType = ServiceComps[j].appType;
    d->defined = true;

    d->componentHasLabel = ServiceComps[j].hasLabel;
    if ( ServiceComps[j].hasLabel ) {
      strcpy( d->componentLabel, ServiceComps[j].label );
      strcpy( d->componentAbbr, ServiceComps[j].abbr );
    } else {
      d->componentLabel[0] = 0;
      d->componentAbbr[0] = 0;
    }
    break;
  }
}

void fib_processor::dataforAudioService(std::string &s, audiodata *d) {
  dataforAudioService(s, d, 0);
}

void fib_processor::dataforAudioService(std::string &s, audiodata *d,
                                        int16_t compnr) {
    serviceId *selectedService;

    d->defined = false;
    fibLocker.lock();
    const bool fullMatchOnly = true;
    selectedService = findServiceId(s, fullMatchOnly);
    if (selectedService == nullptr) {
      fibLocker.unlock();
      return;
    }
    dataforAudioService(selectedService, d, compnr);
    fibLocker.unlock();
}

void fib_processor::dataforAudioService(int SId, audiodata *d,
                                        int16_t compnr) {
  serviceId *selectedService;

  d->defined = false;
  fibLocker.lock();
  selectedService = findServiceId(SId);
  if (selectedService == nullptr) {
    fibLocker.unlock();
    return;
  }
  dataforAudioService(selectedService, d, compnr);
  fibLocker.unlock();
}

void fib_processor::dataforAudioService(serviceId *selectedService, audiodata *d,
                                        int16_t compnr) {
  int16_t j;
  //	first we locate the serviceId
  for (j = 0; j < 64; j++) {
    int16_t subchId;
    if ((!ServiceComps[j].inUse) || (ServiceComps[j].TMid != 00)) continue;

    if (ServiceComps[j].componentNr != compnr) continue;

    if (selectedService != ServiceComps[j].service) continue;

    subchId = ServiceComps[j].subchannelId;
    if ( !(0 <= subchId && subchId < 64) )
      continue;

    d->subchId = subchId;
    d->startAddr = subChannels[subchId].StartAddr;
    d->shortForm = subChannels[subchId].shortForm;
    d->protLevel = subChannels[subchId].protLevel;
    d->length = subChannels[subchId].Length;
    d->subChanSize = subChannels[subchId].subChanSize;
    d->bitRate = subChannels[subchId].BitRate;
    d->protIdxOrCase = subChannels[subchId].protIdxOrCase;
    d->componentNr = ServiceComps[j].componentNr;
    d->ASCTy = ServiceComps[j].ASCTy;
    d->language = selectedService->language;
    d->programType = selectedService->programType;
    d->defined = true;

    d->componentHasLabel = ServiceComps[j].hasLabel;
    if ( ServiceComps[j].hasLabel ) {
      strcpy( d->componentLabel, ServiceComps[j].label );
      strcpy( d->componentAbbr, ServiceComps[j].abbr );
    } else {
      d->componentLabel[0] = 0;
      d->componentAbbr[0] = 0;
    }
    break;
  }
}

void fib_processor::printAll_metaInfo(FILE *out) {
  fprintf(out, "all meta info of fib_processor:\n");

  fprintf(out, "services:\n");
  int num = 0;
  for (int k = 0; k < 64; ++k) {
    const serviceId &e = listofServices[k];
    if (!e.inUse)
      continue;
    ++num;
    fprintf(out, "%2d(%2d): SID %08X, lang %2d, PTy %2d, pNum %2d, #SCIds %d =[",
      k, num,
      e.SId,
      e.hasLanguage ? (int)e.language : -1,
      (int)e.programType,
      e.hasPNum ? (int)e.pNum : -1,
      (int)e.numSCIds
      );
    for (int i=0; i < e.numSCIds; ++i)
      fprintf(stderr, "%d,", (int)e.SCIds[i]);
    fprintf(out, "], #SCId %d =[", (int)e.numSCid);
    for (int i=0; i < e.numSCid; ++i)
      fprintf(stderr, "%d,", (int)e.SCid[i]);
    fprintf(out, "], #SubChId %d =[", (int)e.numSubChId);
    for (int i=0; i < e.numSubChId; ++i)
      fprintf(stderr, "%d,", (int)e.SubChId[i]);
    fprintf(out, "], label %d '%s' / '%s'\n",
      e.hasName ? 1 : 0,
      e.label,
      e.abbr
      );
  }

  fprintf(out, "components:\n");
  num = 0;
  int nA = 0;
  int nP = 0;
  for (int k = 0; k < 64; ++k) {
    const serviceComponent &e = ServiceComps[k];
    if (!e.inUse)
      continue;
    ++num;
    if (e.compType == 'A')
      ++nA;
    else if (e.compType == 'P')
      ++nP;
    fprintf(out, "%2d(%2d): %c(%2d) TMid %d, SID %08X, componentNr %2d, ASCTy %2d, PS %d, subChID %4d, SCId %2d, CA %d, appType %2d, SCIds %d, madePub %d",
      k, num,
      e.compType, ( e.compType == 'A' ? nA : ( e.compType == 'P' ? nP : -1 )  ),
      (int)e.TMid,
      e.service ? e.service->SId : -1,
      (int)e.componentNr,
      (int)e.ASCTy,
      (int)e.PS_flag,
      (int)e.subchannelId,
      (int)e.SCId,
      (int)e.CAflag,
      (int)e.appType,
      (int)e.SCIds,
      (int)e.is_madePublic ? 1 : 0
      );
    if ( e.hasLabel )
      fprintf(out, ", '%s' / '%s'", e.label, e.abbr);
    else
      fprintf(out, ", '' / ''");
    if ( e.is_madePublic )
      fprintf(out, ", DSCTy %2d, DG %d, addr %3d\n",
        (int)e.DSCTy,
        (int)e.DGflag,
        (int)e.packetAddress
        );
    else
      fprintf(out, "\n");
  }

  fprintf(out, "subChannels:\n");
  num = 0;
  for (int k = 0; k < 64; ++k) {
    const channelMap &e = subChannels[k];
    if (!e.inUse)
      continue;
    ++num;
    fprintf(out, "%2d(%2d): subChID %4d, startAddr %3d, len %3d, sz %3d, %s, idxOrCase %3d, tabIdx %2d, protLevel %d, bitRate %3d, lang %d, FEC %d\n",
      k, num,
      (int)e.SubChId,
      (int)e.StartAddr,
      (int)e.Length,
      (int)e.subChanSize,
      e.shortForm ? "short" : "long",
      e.protIdxOrCase,
      e.protTabIdx,
      e.protLevel,
      e.BitRate,
      (int)e.language,
      (int)e.FEC_scheme
      );
  }
}

//
//	and now for the would-be signals
//	Note that the main program may decide to execute calls
//	in the fib structures, so release the lock
void fib_processor::addtoEnsemble(const std::string &s, const std::string &abbr, int32_t SId) {
  fibLocker.unlock();
  if (programnameHandler != nullptr) programnameHandler(s, abbr, SId, userData);
  fibLocker.lock();
}

void fib_processor::nameofEnsemble(int id, const std::string &s, const std::string &abbr) {
  fibLocker.unlock();
  if (ensemblenameHandler != nullptr) ensemblenameHandler(s, abbr, id, userData);
  fibLocker.lock();
  isSynced = true;
}

void fib_processor::idofEnsemble(int32_t id) {
  fibLocker.unlock();
  if (ensembleidHandler != nullptr) ensembleidHandler(id, userData);
  fibLocker.lock();
}

void fib_processor::changeinConfiguration(void) {}

bool fib_processor::syncReached(void) { return isSynced; }

std::complex<float> fib_processor::get_coordinates(int16_t mainId,
                                                   int16_t subId,
                                                   bool *success) {
  coordinates.print_coordinates();
  return coordinates.get_coordinates(mainId, subId, success);
}

// mainId < 0 (-1) => don't check mainId
// subId == -1 => deliver first available offset
// subId == -2 => deliver coarse coordinates
std::complex<float> fib_processor::get_coordinates(int16_t mainId,
                                                   int16_t subId, bool *success,
                                                   int16_t *pMainId,
                                                   int16_t *pSubId,
                                                   int16_t *pTD) {
  // coordinates. print_coordinates ();
  return coordinates.get_coordinates(mainId, subId, success, pMainId, pSubId,
                                     pTD);
}

uint8_t fib_processor::getECC(bool *success) {
  *success = ecc_Present;
  return ecc_byte;
}

uint8_t fib_processor::getInterTabId(bool *success) {
  *success = interTab_Present;
  return interTabId;
}

void fib_processor::setEId_handler(ensembleid_t EId_Handler) {
  ensembleidHandler = EId_Handler;
}

void fib_processor::reset(void) {
  dateFlag = false;
  ecc_Present = false;
  interTab_Present = false;
  clearEnsemble();
  CIFcount = 0;
#if OUT_CIF_COUNTER
  fprintf(stderr, "fib_processor::reset() => CIFcount := 0\n");
#endif
  hasCIFcount = false;
}

int32_t fib_processor::get_CIFcount(void) const { return CIFcount; }

bool fib_processor::has_CIFcount(void) const { return hasCIFcount; }
