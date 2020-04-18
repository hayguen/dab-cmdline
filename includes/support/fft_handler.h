#
/*
 *    Copyright (C) 2009 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the DAB library
 *
 *    DAB libray is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __FFT_HANDLER__
#define __FFT_HANDLER__

//  Simple wrapper around fftwf
#include "dab-constants.h"
#include "dab-params.h"

#define FFTW_MALLOC fftwf_malloc
#define FFTW_PLAN_DFT_1D fftwf_plan_dft_1d
#define FFTW_DESTROY_PLAN fftwf_destroy_plan
#define FFTW_FREE fftwf_free
#define FFTW_PLAN fftwf_plan
#define FFTW_EXECUTE fftwf_execute
#include <fftw3.h>

/*
 *  a simple wrapper
 */

class fft_handler {
 public:
  fft_handler(uint8_t dabMode);
  ~fft_handler();

  inline std::complex<float> *getVector() { return vector; }

  inline void do_FFT() { FFTW_EXECUTE(plan); }

  //	Note that we do not scale here, not needed
  //	for the purpose we are using it for
  inline void do_IFFT() {
    int32_t i;

    for (i = 0; i < fftSize; i++) vector[i] = conj(vector[i]);
    FFTW_EXECUTE(plan);
    for (i = 0; i < fftSize; i++) vector[i] = conj(vector[i]);
  }

 private:
  std::complex<float> *vector;
  FFTW_PLAN plan;
  int32_t fftSize;
};

#endif
