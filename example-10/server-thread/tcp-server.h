#
/*
 *    Copyright (C) 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the  DAB-library
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
 *	Simple streaming server, for e.g. epg data and tpg data
 */

#ifndef __TCP_SERVER__
#define __TCP_SERVER__

#include <netdb.h>
#include <ringbuffer.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic>
#include <string>
#include <thread>

class tcpServer {
 public:
  tcpServer(int);
  ~tcpServer(void);
  void sendData(uint8_t *, int32_t);
  void run(int port);

 private:
  std::thread threadHandle;
  RingBuffer<uint8_t> *buffer;
  std::atomic<bool> running;
  std::atomic<bool> connected;
};
#endif
