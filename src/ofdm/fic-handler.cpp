#
/*
 *    Copyright (C) 2013 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
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
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "fic-handler.h"
#include "msc-handler.h"
#include "protTables.h"
//
//	The 3072 bits of the serial motherword shall be split into
//	24 blocks of 128 bits each.
//	The first 21 blocks shall be subjected to
//	puncturing (per 32 bits) according to PI_16
//	The next three blocks shall be subjected to
//	puncturing (per 32 bits) according to PI_15

/**
 *	\class ficHandler
 * 	We get in - through get_ficBlock - the FIC data
 * 	in units of 768 bits.
 * 	We follow the standard and apply conv coding and
 * 	puncturing.
 *	The data is sent through to the fic processor
 */
ficHandler::ficHandler(uint8_t dabMode, ensemblename_t ensemblenameHandler,
                       programname_t programnameHandler,
                       fib_quality_t fib_qualityHandler, void *userData)
    : viterbiHandler(768),
      params(dabMode),
      fibProcessor(ensemblenameHandler, programnameHandler, userData) {
  int16_t i, j, k;
  int16_t local = 0;

  (void)dabMode;
  this->fib_qualityHandler = fib_qualityHandler;
  this->errorReportHandler = nullptr;
  this->fib_dataHandler = nullptr;
  this->userData = userData;
  index = 0;
  BitsperBlock = 2 * params.get_carriers();
  ficno = 0;
  memset(shiftRegister, 1, 9);

  for (i = 0; i < 768; i++) {
    PRBS[i] = shiftRegister[8] ^ shiftRegister[4];
    for (j = 8; j > 0; j--) shiftRegister[j] = shiftRegister[j - 1];

    shiftRegister[0] = PRBS[i];
  }

  /**
   *	a block of 2304 bits is considered to be a codeword
   *	In the first step we have 21 blocks with puncturing according to PI_16
   *	each 128 bit block contains 4 subblocks of 32 bits
   *	on which the given puncturing is applied
   */
  memset(punctureTable, 0, (3072 + 24) * sizeof(bool));

  for (i = 0; i < 21; i++) {
    for (k = 0; k < 32 * 4; k++) {
      if (get_PCodes(16 - 1)[k % 32] == 1) punctureTable[local] = true;
      local++;
    }
  }
  /**
   *	In the second step
   *	we have 3 blocks with puncturing according to PI_15
   *	each 128 bit block contains 4 subblocks of 32 bits
   *	on which the given puncturing is applied
   */
  for (i = 0; i < 3; i++) {
    for (k = 0; k < 32 * 4; k++) {
      if (get_PCodes(15 - 1)[k % 32] == 1) punctureTable[local] = true;
      local++;
    }
  }

  /**
   *	we have a final block of 24 bits  with puncturing according to PI_X
   *	This block constitues the 6 * 4 bits of the register itself.
   */
  for (k = 0; k < 24; k++) {
    if (get_PCodes(8 - 1)[k] == 1) punctureTable[local] = true;
    local++;
  }
}

ficHandler::~ficHandler(void) {}

/**
 *	\brief process_ficBlock
 *	The number of bits to be processed per incoming block
 *	is 2 * p -> K, which still depends on the Mode.
 *	for Mode I it is 2 * 1536, for Mode II, it is 2 * 384,
 *	for Mode III it is 192, Mode IV gives 2 * 768.
 *	for Mode II we will get the 2304 bits after having read
 *	the 3 FIC blocks, each with 768 bits.
 *	for Mode IV we will get 3 * 2 * 768 = 4608, i.e. two resulting blocks
 *	Note that Mode III is NOT supported
 *
 *	The function is called with a blkno. This should be 1, 2 or 3
 *	for each time 2304 bits are in, we call process_ficInput
 */
void ficHandler::process_ficBlock(std::vector<int16_t> data, int16_t blkno) {
  int32_t i;

  if (blkno == 1) {
    index = 0;
    ficno = 0;
    fibProcessor.newFrame();
  }
  //
  if ((1 <= blkno) && (blkno <= 3)) {
    for (i = 0; i < BitsperBlock; i++) {
      ofdm_input[index++] = data[i];
      if (index >= 2304) {
        process_ficInput(ficno);
        index = 0;
        ficno++;
      }
    }
  } else
    fprintf(stderr, "You should not call ficBlock here\n");
  //	we are pretty sure now that after block 4, we end up
  //	with index = 0
}

/**
 *	\brief process_ficInput
 *	we have a vector of 2304 (0 .. 2303) soft bits that has
 *	to be de-punctured and de-conv-ed into a block of 768 bits
 *	In this approach we first create the full 3072 block (i.e.
 *	we first depuncture, and then we apply the deconvolution
 *	In the next coding step, we will combine this function with the
 *	one above
 */
void ficHandler::process_ficInput(int16_t ficno) {
  int16_t i;
  int16_t viterbiBlock[3072 + 24];
  int16_t inputCount = 0;
  uint8_t fibBinData[32 + 2];

  memset(viterbiBlock, 0, (3072 + 24) * sizeof(int16_t));

  for (i = 0; i < 4 * 768 + 24; i++)
    if (punctureTable[i]) viterbiBlock[i] = ofdm_input[inputCount++];
  /**
   *	Now we have the full word ready for deconvolution
   *	deconvolution is according to DAB standard section 11.2
   */
  deconvolve(viterbiBlock, bitBuffer_out);
  /**
   *	if everything worked as planned, we now have a
   *	768 bit vector containing three FIB's
   *
   *	first step: energy dispersal according to the DAB standard
   *	We use a predefined vector PRBS
   */
  for (i = 0; i < 768; i++) bitBuffer_out[i] ^= PRBS[i];
  /**
   *	each of the fib blocks is protected by a crc
   *	(we know that there are three fib blocks each time we are here
   *	we keep track of the successrate
   *	and show that per 100 fic blocks
   */
  for (i = ficno * 3; i < ficno * 3 + 3; i++) {
    uint8_t *p = &bitBuffer_out[(i % 3) * 256];

    if (fib_dataHandler) {
      for (int byteOff = 0; byteOff < 32; ++byteOff)
        fibBinData[byteOff] = getBits_8(p, byteOff * 8);
      fibBinData[32] = 0;
      fibBinData[33] = 0;
    }
    if (!check_CRC_bits(p, 256)) {
      show_ficCRC(false);
      if (errorReportHandler) {
        errorReportHandler(5, 1, 0, userData);
        // fprintf(stderr, "fic CRC at ficNo %d for FIB block broken\n",
        // (int)ficno);
      }
      if (fib_dataHandler)
        fib_dataHandler(fibBinData, 0 /* wrong CRC */, userData);
      continue;
    }
    show_ficCRC(true);
    fibProtector.lock();
    if (fib_dataHandler)
      fib_dataHandler(fibBinData, 1 /* good CRC */, userData);
    fibProcessor.process_FIB(p, ficno);
    fibProtector.unlock();
  }
}

void ficHandler::clearEnsemble(void) {
  fibProtector.lock();
  fibProcessor.clearEnsemble();
  fibProtector.unlock();
}

uint8_t ficHandler::kindofService(std::string &s) {
  uint8_t result;
  fibProtector.lock();
  result = fibProcessor.kindofService(s);
  fibProtector.unlock();
  return result;
}

void ficHandler::dataforAudioService(std::string &s, audiodata *d, int c) {
  fibProtector.lock();
  fibProcessor.dataforAudioService(s, d, c);
  fibProtector.unlock();
}

void ficHandler::dataforDataService(std::string &s, packetdata *d, int c) {
  fibProtector.lock();
  fibProcessor.dataforDataService(s, d, c);
  fibProtector.unlock();
}

int32_t ficHandler::get_CIFcount(void) const {
  // no lock, because using std::atomic<> in fib_processor class
  int32_t r = fibProcessor.get_CIFcount();
  return r;
}

bool ficHandler::has_CIFcount(void) const {
  // no lock, because using std::atomic<> in fib_processor class
  bool r = fibProcessor.has_CIFcount();
  return r;
}

std::complex<float> ficHandler::get_coordinates(int16_t mainId, int16_t subId,
                                                bool *success) {
  std::complex<float> result;

  fibProtector.lock();
  result = fibProcessor.get_coordinates(mainId, subId, success);
  fibProtector.unlock();
  return result;
}
//
//	Alternative function (extended), contributed by Hayati Ayguen
// mainId < 0 (-1) => don't check mainId
// subId == -1 => deliver first available offset
// subId == -2 => deliver coarse coordinates
std::complex<float> ficHandler::get_coordinates(int16_t mainId, int16_t subId,
                                                bool *success, int16_t *pMainId,
                                                int16_t *pSubId, int16_t *pTD) {
  std::complex<float> result;

  fibProtector.lock();
  result = fibProcessor.get_coordinates(mainId, subId, success, pMainId, pSubId,
                                        pTD);
  fibProtector.unlock();
  return result;
}

uint8_t ficHandler::getECC(bool *success) {
  uint8_t result;

  // no lock, because using std::atomic<> in fib_processor class
  result = fibProcessor.getECC(success);
  return result;
}

uint8_t ficHandler::getInterTabId(bool *success) {
  uint8_t result;

  // no lock, because using std::atomic<> in fib_processor class
  result = fibProcessor.getInterTabId(success);
  return result;
}

void ficHandler::setEId_handler(ensembleid_t EId_Handler) {
  fibProcessor.setEId_handler(EId_Handler);
}

void ficHandler::setError_handler(decodeErrorReport_t err_Handler) {
  errorReportHandler = err_Handler;
}

void ficHandler::setFIB_handler(fibdata_t fib_Handler) {
  fib_dataHandler = fib_Handler;
}

bool ficHandler::syncReached(void) { return fibProcessor.syncReached(); }

std::string ficHandler::nameFor(int32_t serviceId) {
  return fibProcessor.nameFor(serviceId);
}

int32_t ficHandler::SIdFor(std::string &name) {
  return fibProcessor.SIdFor(name);
}

static int pos = 0;
static int amount = 0;
void ficHandler::show_ficCRC(bool b) {
  if (b) pos++;
  if (++amount >= 100) {
    if (fib_qualityHandler != nullptr) fib_qualityHandler(pos, userData);
    pos = 0;
    amount = 0;
  }
}

void ficHandler::reset(void) { fibProcessor.reset(); }