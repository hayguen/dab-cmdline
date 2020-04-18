#
/*
 *    Copyright (C) 2008 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the dab library
 *
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
 */
#include "fft_handler.h"

fft_handler::fft_handler(uint8_t dabMode) {
  int i;
  dabParams p(dabMode);
  this->fftSize = p.get_T_u();
  vector = (std::complex<float> *)FFTW_MALLOC(sizeof(std::complex<float>) * fftSize);
  for (i = 0; i < fftSize; i++) vector[i] = std::complex<float>(0, 0);
  plan = FFTW_PLAN_DFT_1D(fftSize, reinterpret_cast<fftwf_complex *>(vector),
                          reinterpret_cast<fftwf_complex *>(vector),
                          FFTW_FORWARD, FFTW_ESTIMATE);
}

fft_handler::~fft_handler() {
  FFTW_DESTROY_PLAN(plan);
  FFTW_FREE(vector);
}

