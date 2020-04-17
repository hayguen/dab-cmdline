#
/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J.
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __RTL_TCP_CLIENT
#define __RTL_TCP_CLIENT
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <sstream>
#include <thread>

#include "dab-constants.h"
#include "device-handler.h"
#include "ringbuffer.h"

//	commands are packed in 5 bytes, one "command byte"
//	and an integer parameter
struct command {
  unsigned char cmd;
  unsigned int param;
} __attribute__((packed));

class rtl_tcp_client : public deviceHandler {
 public:
  rtl_tcp_client(std::string hostname, int32_t port, int32_t frequency,
                 int16_t gain, bool autogain, int16_t ppm);

  ~rtl_tcp_client(void);
  void stopReader(void);
  int32_t getSamples(std::complex<float> *V, int32_t size);
  int32_t Samples(void);
  int16_t bitDepth(void);

 private:
  virtual void run(void);
  std::string hostname;
  int32_t basePort;
  int32_t vfoFrequency;
  int16_t gain;
  bool autogain;
  int16_t ppm;

  int32_t theRate;
  RingBuffer<uint8_t> *theBuffer;
  int theSocket;
  struct sockaddr_in server;
  std::thread threadHandle;
  std::atomic<bool> running;

  void sendCommand(uint8_t cmd, uint32_t param);
};

#endif
