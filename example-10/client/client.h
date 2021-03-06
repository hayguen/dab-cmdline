#
/*
 *    Copyright (C) 2015
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
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
 *	Communication via network to a DAB receiver to
 *	show the tdc data
 */

#ifndef __TDC_CLIENT__
#define __TDC_CLIENT__
#include <QDialog>
#include <QHostAddress>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>
#include "constants.h"
#include "header-test.h"
#include "ui_widget.h"
//

class Client : public QDialog, public Ui_widget {
  Q_OBJECT
 public:
  Client(QWidget *parent = NULL);
  ~Client(void);

  bool connected;
 private slots:
  void wantConnect(void);
  void setConnection(void);
  void readData(void);
  void handle(uint8_t);
  void terminate(void);
  void timerTick(void);

 private:
  void handleFrameType_0(uint8_t *, int16_t);
  void handleFrameType_1(uint8_t *, int16_t);
  bool serviceComponentFrameheaderCRC(uint8_t *data, int16_t offset,
                                      int16_t maxL);

  QTcpSocket streamer;
  QTimer *connectionTimer;
  headerTest headertester;
  int16_t toRead;
  int16_t dataLength;
  uint8_t *dataBuffer;
  uint8_t frameType;
  int16_t dataIndex;
};
#endif
