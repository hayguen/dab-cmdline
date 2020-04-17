#
/*
 *    Copyright (C) 2014 .. 2020
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
 *    along with Qt-DAB-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 	Default (void) implementation of
 * 	virtual input class
 */
#include "device-handler.h"

deviceHandler::deviceHandler() { lastFrequency = 100000; }

deviceHandler::~deviceHandler() {}

bool deviceHandler::restartReader(int32_t) { return true; }

void deviceHandler::stopReader(void) {}

void deviceHandler::run(void) {}

int32_t deviceHandler::getSamples(std::complex<float> *v, int32_t amount) {
  (void)v;
  (void)amount;
  return 0;
}

int32_t deviceHandler::Samples() { return 0; }

int32_t deviceHandler::defaultFrequency(void) { return 220000000; }

void deviceHandler::resetBuffer() {}

void deviceHandler::setGain(int32_t x) { (void)x; }

bool deviceHandler::has_autogain() { return false; }

void deviceHandler::set_autogain(bool b) { (void)b; }

void deviceHandler::set_ifgainReduction(int x) { (void)x; }

void deviceHandler::set_lnaState(int x) { (void)x; }
