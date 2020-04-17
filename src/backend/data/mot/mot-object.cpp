#
/*
 *    Copyright (C) 2015 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the dab library
 *    dab library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    dab library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with dab library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	MOT handling is a crime, here we have a single class responsible
 *	for handling a single MOT message with a given transportId
 */
#include "mot-object.h"

#include <algorithm>

motObject::motObject(motdata_t motdataHandler, bool dirElement,
                     uint16_t transportId, uint8_t *segment,
                     int32_t segmentSize, bool lastFlag, void *ctx) {
  int32_t pointer = 7;

  (void)segmentSize;
  (void)lastFlag;

  this->motdataHandler = motdataHandler;
  this->dirElement = dirElement;
  this->transportId = transportId;
  this->numofSegments = -1;
  this->segmentSize = -1;
  this->ctx = ctx;
  this->unknown_fileno = 0;
  headerSize = ((segment[3] & 0x0F) << 9) | (segment[4] << 1) |
               ((segment[5] >> 7) & 0x01);
  bodySize = (segment[0] << 20) | (segment[1] << 12) | (segment[2] << 4) |
             ((segment[3] & 0xF0) >> 4);
  contentType = ((segment[5] >> 1) & 0x3F);
  contentsubType = ((segment[5] & 0x01) << 8) | segment[6];

  //	we are actually only interested in the name, if any
  while (pointer < (int32_t)headerSize) {
    uint8_t PLI = (segment[pointer] & 0300) >> 6;
    uint8_t paramId = (segment[pointer] & 077);
    uint16_t length;
    switch (PLI) {
      case 00:
        pointer += 1;
        break;

      case 01:
        pointer += 2;
        break;

      case 02:
        pointer += 5;
        break;

      case 03:
        if ((segment[pointer + 1] & 0200) != 0) {
          length = (segment[pointer + 1] & 0177) << 8 | segment[pointer + 2];
          pointer += 3;
        } else {
          length = segment[pointer + 1] & 0177;
          pointer += 2;
        }

        if (paramId == 12) {
          int16_t i;
          for (i = 0; i < length - 1; i++)
            name.push_back(segment[pointer + i + 1]);
        }
        pointer += length;
    }
  }
}

motObject::~motObject(void) {}

uint16_t motObject::get_transportId(void) { return transportId; }

//      type 4 is a segment.
//	The pad/dir software will only call this whenever it has
//	established that the current slide has th right transportId
//
//	Note that segments do not need to come in in the right order
void motObject::addBodySegment(uint8_t *bodySegment, int16_t segmentNumber,
                               int32_t segmentSize, bool lastFlag) {
  int32_t i;

  if ((segmentNumber < 0) || (segmentNumber >= 8192)) return;

  if (motMap.find(segmentNumber) != motMap.end()) return;

  //      Note that the last segment may have a different size
  if (!lastFlag && ((int32_t)this->segmentSize == -1))
    this->segmentSize = segmentSize;

  std::vector<uint8_t> segment;
  segment.resize(segmentSize);
  for (i = 0; i < segmentSize; i++) segment[i] = bodySegment[i];
  motMap.insert(std::make_pair(segmentNumber, segment));
  //
  if (lastFlag) numofSegments = segmentNumber + 1;

  if (numofSegments == -1) return;
  //
  //      once we know how many segments there are/should be,
  //      we check for completeness
  for (i = 0; i < numofSegments; i++) {
    if (motMap.find(i) == motMap.end()) return;
  }

  //      The motObject is (seems to be) complete
  handleComplete();
}

void motObject::handleComplete(void) {
  std::vector<uint8_t> result;

  for (const auto &it : motMap)
    result.insert(result.end(), (it.second).begin(), (it.second).end());

  if (contentType == 7) {  // epg data
    return;
  }
  //
  //	Only send the picture to show when it is a slide and not
  //	an element of a directory
  if (contentType != 2) {
    return;
  }

  std::string realName;

  //	MOT slide, to show
  if (name == "") {
    ++unknown_fileno;
    realName = "no_name" + std::to_string(unknown_fileno);
  } else {
    realName = name;
    // produce valid filename - replace all suspect characters
    std::replace(realName.begin(), realName.end(), '/',
                 '_');  // replace all '/'
    std::replace(realName.begin(), realName.end(), '\\', '_');
    std::replace(realName.begin(), realName.end(), ':', '_');
  }
  FILE *temp = fopen(realName.c_str(), "w");
  if (temp) {
    fwrite(result.data(), 1, result.size(), temp);
    if (motdataHandler != nullptr)
      motdataHandler(realName, contentsubType, ctx);
  } else {
    fprintf(stderr, "unable to save and handle MOT slide of name '%s'\n",
            name.c_str());
  }
}

int motObject::get_headerSize(void) { return headerSize; }
