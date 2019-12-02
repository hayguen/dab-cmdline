#
/******************************************************************************
** kjmp2 -- a minimal MPEG-1 Audio Layer II decoder library                  **
*******************************************************************************
** Copyright (C) 2006 Martin J. Fiedler <martin.fiedler@gmx.net>             **
**                                                                           **
** This software is provided 'as-is', without any express or implied         **
** warranty. In no event will the authors be held liable for any damages     **
** arising from the use of this software.                                    **
**                                                                           **
** Permission is granted to anyone to use this software for any purpose,     **
** including commercial applications, and to alter it and redistribute it    **
** freely, subject to the following restrictions:                            **
**   1. The origin of this software must not be misrepresented; you must not **
**      claim that you wrote the original software. If you use this software **
**      in a product, an acknowledgment in the product documentation would   **
**      be appreciated but is not required.                                  **
**   2. Altered source versions must be plainly marked as such, and must not **
**      be misrepresented as being the original software.                    **
**   3. This notice may not be removed or altered from any source            **
**      distribution.                                                        **
******************************************************************************/
//
//	This software is a rewrite of the original kjmp2 software,
//	Rewriting in the form of a class
//	for use in the sdr-j DAB/DAB+ receiver
//	all rights remain where they belong
#ifndef 	__MP2PROCESSOR__
#define		__MP2PROCESSOR__

#include	<stdio.h>
#include	<stdint.h>
#include	<math.h>
#include	"backend-base.h"
#include	"pad-handler.h"
#include	<stdio.h>
#include	"ringbuffer.h"
#include	"dab-api.h"

#define KJMP2_MAX_FRAME_SIZE    1440  // the maximum size of a frame
#define KJMP2_SAMPLES_PER_FRAME 1152  // the number of samples per frame

// quantizer specification structure
struct quantizer_spec {
	int32_t nlevels;
	uint8_t grouping;
	uint8_t cw_bits;
};

class	mp2Processor: public backendBase {
public:
			mp2Processor	(int16_t,
	                                 audioOut_t,
	                                 dataOut_t,
	                                 programQuality_t,
	                                 motdata_t,
	                                 void	*);
			~mp2Processor	(void);
	virtual void	setError_handler(decodeErrorReport_t err_Handler);
	void		addtoFrame	(uint8_t *);
	
private:
	audioOut_t	soundOut;
	dataOut_t	dataOut;
	programQuality_t	mscQuality;
	decodeErrorReport_t		errorReportHandler;
	void		*ctx;
	int16_t		bitRate;
	padHandler	my_padHandler;
	void		output		(int16_t *, int, int, bool);
	int32_t		mp2sampleRate	(uint8_t *);
	int32_t		mp2decodeFrame	(uint8_t *, int16_t *, bool *);
	int32_t		baudRate;
	void		setSamplerate		(int32_t);
	struct quantizer_spec *read_allocation (int, int);
	void		read_samples	(struct quantizer_spec *, int, int *);
	int32_t		get_bits	(int32_t);
	int16_t		V [2][1024];
	int16_t		Voffs;
	int16_t		N [64][32];
	struct quantizer_spec *allocation[2][32];
	int32_t		scfsi[2][32];
	int32_t		scalefactor[2][32][3];
	int32_t		sample[2][32][3];
	int32_t		U[512];

	int32_t		bit_window;
	int32_t		bits_in_window;
	uint8_t		*frame_pos;
	uint8_t		*MP2frame;
	int16_t		MP2framesize;
	int16_t		MP2Header_OK;
	int16_t		MP2headerCount;
	int16_t		MP2bitCount;
	void		addbittoMP2	(uint8_t *, uint8_t, int16_t);
	int32_t		totalFrameCount;
	int16_t		numberofFrames;
	int16_t		errorFrames;
};
#endif

