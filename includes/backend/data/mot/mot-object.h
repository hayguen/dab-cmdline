#
/*
 *    Copyright (C) 2015 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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

#ifndef __MOT_OBJECT__
#define __MOT_OBJECT__
#include <iterator>
#include <map>
#include <vector>
#include "dab-api.h"
#include "dab-constants.h"

class motObject {
 public:
  motObject(motdata_t motdataHandler, bool dirElement, uint16_t transportId,
            uint8_t *segment, int32_t segmentSize, bool lastFlag, void *ctx);
  ~motObject();
  void addBodySegment(uint8_t *bodySegment, int16_t segmentNumber,
                      int32_t segmentSize, bool lastFlag);
  uint16_t get_transportId();
  int get_headerSize();

 private:
  motdata_t motdataHandler;
  bool dirElement;
  uint16_t transportId;
  int16_t numofSegments;
  uint32_t segmentSize;
  void *ctx;
  uint32_t headerSize;
  uint32_t bodySize;
  int contentType;
  int contentsubType;
  std::string name;
  int unknown_fileno;
  std::map<int, std::vector<uint8_t>> motMap;

  void handleComplete();
};

#endif
