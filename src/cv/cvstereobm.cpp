//M*//////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

/****************************************************************************************\
*    Very fast SAD-based (Sum-of-Absolute-Diffrences) stereo correspondence algorithm.   *
*    Contributed by Kurt Konolige                                                        *
\****************************************************************************************/

#include "_cv.h"

CV_IMPL CvStereoBMState*
cvCreateStereoBMState( int /*preset*/, int numberOfDisparities )
{
    CvStereoBMState* state = 0;

    CV_FUNCNAME( "cvCreateStereoBMState" );

    __BEGIN__;

    state = (CvStereoBMState*)cvAlloc( sizeof(*state) );
    if( !state )
        EXIT;
    
    state->preFilterType = CV_STEREO_BM_NORMALIZED_RESPONSE;
    state->preFilterSize = 9;
    state->preFilterCap = 31;
    state->SADWindowSize = 15;
    state->minDisparity = 0;
    state->numberOfDisparities = numberOfDisparities > 0 ? numberOfDisparities : 64;
    state->textureThreshold = 10;
    state->uniquenessRatio = 15;
    state->speckleRange = state->speckleWindowSize = 0;

    state->preFilteredImg0 = state->preFilteredImg1 = state->slidingSumBuf = 0;

    __END__;

    if( cvGetErrStatus() < 0 )
        cvReleaseStereoBMState( &state );
    return state;
}


CV_IMPL void
cvReleaseStereoBMState( CvStereoBMState** state )
{
    CV_FUNCNAME( "cvReleaseStereoBMState" );

    __BEGIN__;

    if( !state )
        CV_ERROR( CV_StsNullPtr, "" );

    if( !*state )
        EXIT;

    cvReleaseMat( &(*state)->preFilteredImg0 );
    cvReleaseMat( &(*state)->preFilteredImg1 );
    cvReleaseMat( &(*state)->slidingSumBuf );
    cvFree( state );

    __END__;
}

#define CV_STEREO_BM_INT_ACCUM 1

#if CV_STEREO_BM_INT_ACCUM
#undef CV_SSE2
#define CV_SSE2 0
typedef int sum_t;
#else
typedef ushort sum_t;
#endif

static void icvPrefilter( const CvMat* src, CvMat* dst, int winsize, int ftzero, CvMat* buf )
{
    int x, y, wsz2 = winsize/2;
    sum_t* vsum = (sum_t*)cvAlignPtr(buf->data.ptr + (wsz2 + 1)*sizeof(vsum[0]), 32);
    int scale_g = winsize*winsize/8, scale_s = (1024 + scale_g)/(scale_g*2);
    const int OFS = 256*5, TABSZ = OFS*2 + 256;
    uchar tab[TABSZ];
    const uchar* sptr = src->data.ptr;
    int srcstep = src->step;
    CvSize size = cvGetMatSize(src);

    scale_g *= scale_s;

    for( x = 0; x < TABSZ; x++ )
        tab[x] = (uchar)(x - OFS < -ftzero ? 0 : x - OFS > ftzero ? ftzero*2 : x - OFS + ftzero);

    for( x = 0; x < size.width; x++ )
        vsum[x] = (ushort)(sptr[x]*(wsz2 + 2));

    for( y = 1; y < wsz2; y++ )
    {
        for( x = 0; x < size.width; x++ )
            vsum[x] = (ushort)(vsum[x] + sptr[srcstep*y + x]);
    }

    for( y = 0; y < size.height; y++ )
    {
        const uchar* top = sptr + srcstep*MAX(y-wsz2-1,0);
        const uchar* bottom = sptr + srcstep*MIN(y+wsz2,size.height-1);
        const uchar* prev = sptr + srcstep*MAX(y-1,0);
        const uchar* curr = sptr + srcstep*y;
        const uchar* next = sptr + srcstep*MIN(y+1,size.height-1);
        uchar* dptr = dst->data.ptr + dst->step*y;
        x = 0;

#if CV_SSE2
        __m128i z = _mm_setzero_si128();
        for( ; x <= size.width - 16; x += 16 )
        {
            __m128i b0 = _mm_loadu_si128((const __m128i*)(bottom + x));
            __m128i t0 = _mm_loadu_si128((const __m128i*)(top + x));
            __m128i b1 = _mm_unpackhi_epi8(b0, z);
            __m128i t1 = _mm_unpackhi_epi8(t0, z);
            __m128i s0 = _mm_load_si128((const __m128i*)(vsum + x));
            __m128i s1 = _mm_load_si128((const __m128i*)(vsum + x + 8));
            b0 = _mm_unpacklo_epi8(b0, z);
            t0 = _mm_unpacklo_epi8(t0, z);
            s0 = _mm_add_epi16(s0,_mm_sub_epi16(b0,t0));
            s1 = _mm_add_epi16(s1,_mm_sub_epi16(b1,t1));
            _mm_store_si128((__m128i*)(vsum + x), s0);
            _mm_store_si128((__m128i*)(vsum + x + 8), s1);
        }
#endif
        for( ; x < size.width; x++ )
            vsum[x] = (ushort)(vsum[x] + bottom[x] - top[x]);

        for( x = 0; x <= wsz2; x++ )
        {
            vsum[-x-1] = vsum[0];
            vsum[size.width+x] = vsum[size.width-1];
        }

        int sum = vsum[0]*(wsz2 + 1);
        for( x = 1; x <= wsz2; x++ )
            sum += vsum[x];

        int val = ((curr[0]*5 + curr[1] + prev[0] + next[0])*scale_g - sum*scale_s) >> 10;
        dptr[0] = tab[val + OFS];

        for( x = 1; x < size.width-1; x++ )
        {
            sum += vsum[x+wsz2] - vsum[x-wsz2-1];
            val = ((curr[x]*4 + curr[x-1] + curr[x+1] + prev[x] + next[x])*scale_g - sum*scale_s) >> 10;
            dptr[x] = tab[val + OFS];
        }
        
        sum += vsum[x+wsz2] - vsum[x-wsz2-1];
        val = ((curr[x]*5 + curr[x-1] + prev[x] + next[x])*scale_g - sum*scale_s) >> 10;
        dptr[x] = tab[val + OFS];
    }
}

CV_IMPL void
cvFindStereoCorrespondenceBM( const CvArr* leftarr, const CvArr* rightarr,
                              CvArr* disparr, CvStereoBMState* state )
{
    const int DISPARITY_SHIFT = 4;
    
    CV_FUNCNAME( "cvFindStereoCorrespondenceBM" );

    __BEGIN__;

    CvMat lstub, *left0 = cvGetMat( leftarr, &lstub );
    CvMat rstub, *right0 = cvGetMat( rightarr, &rstub );
    CvMat left, right;
    CvMat dstub, *disp = cvGetMat( disparr, &dstub );
    int bufSize, x, y, d, width, width1, height;
    int ftzero, wsz, wsz2, ndisp, mindisp, textureThreshold, uniquenessRatio;
    sum_t* sad;
    sum_t *hsad0, *hsad, *hsad_sub;
    int* htext;
    uchar *cbuf0, *cbuf;
    const uchar *lptr0, *lptr, *lptr_sub, *rptr0, *rptr;
    short* dptr;
    int lofs, rofs, sstep, dstep, cstep;
    const int TABSZ = 256;
    uchar tab[TABSZ];
    short FILTERED;
#if CV_SSE2
    const __m128i d0_8 = _mm_setr_epi16(0,1,2,3,4,5,6,7), dd_8 = _mm_set1_epi16(8);
#endif

    if( !CV_ARE_SIZES_EQ(left0, right0) ||
        !CV_ARE_SIZES_EQ(disp, left0) )
        CV_ERROR( CV_StsUnmatchedSizes, "All the images must have the same size" );

    if( CV_MAT_TYPE(left0->type) != CV_8UC1 ||
        !CV_ARE_TYPES_EQ(left0, right0) ||
        CV_MAT_TYPE(disp->type) != CV_16SC1 )
        CV_ERROR( CV_StsUnsupportedFormat,
        "Both input images must have 8uC1 format and the disparity image must have 16sC1 format" );

    if( !state )
        CV_ERROR( CV_StsNullPtr, "Stereo BM state is NULL." );

    if( state->preFilterType != CV_STEREO_BM_NORMALIZED_RESPONSE )
        CV_ERROR( CV_StsOutOfRange, "preFilterType must be =CV_STEREO_BM_NORMALIZED_RESPONSE" );

    if( state->preFilterSize < 5 || state->preFilterSize > 21+CV_STEREO_BM_INT_ACCUM*128 || state->preFilterSize % 2 == 0 )
        CV_ERROR( CV_StsOutOfRange, "preFilterSize must be odd and be within 5..21+" );

    if( state->preFilterCap < 1 || state->preFilterCap > 31+CV_STEREO_BM_INT_ACCUM*32 )
        CV_ERROR( CV_StsOutOfRange, "preFilterCap must be within 1..31+" );

    if( state->SADWindowSize < 5 || state->SADWindowSize > 21+CV_STEREO_BM_INT_ACCUM*128 || state->SADWindowSize % 2 == 0 ||
        state->SADWindowSize >= MIN(left0->cols, left0->rows) )
        CV_ERROR( CV_StsOutOfRange, "SADWindowSize must be odd, be within 5..21+ and "
                                    "be not larger than image width or height" );

    if( state->numberOfDisparities <= 0 || state->numberOfDisparities % 16 != 0 )
        CV_ERROR( CV_StsOutOfRange, "numberOfDisparities must be positive and divisble by 16" );
    if( state->textureThreshold < 0 )
        CV_ERROR( CV_StsOutOfRange, "texture threshold must be non-negative" );
    if( state->uniquenessRatio < 0 )
        CV_ERROR( CV_StsOutOfRange, "uniqueness ratio must be non-negative" );

    if( !state->preFilteredImg0 ||
        state->preFilteredImg0->cols*state->preFilteredImg0->rows < left0->cols*left0->rows )
    {
        cvReleaseMat( &state->preFilteredImg0 );
        cvReleaseMat( &state->preFilteredImg1 );

        state->preFilteredImg0 = cvCreateMat( left0->rows, left0->cols, CV_8U );
        state->preFilteredImg1 = cvCreateMat( left0->rows, left0->cols, CV_8U );
    }
    left = cvMat(left0->rows, left0->cols, CV_8U, state->preFilteredImg0->data.ptr);
    right = cvMat(right0->rows, right0->cols, CV_8U, state->preFilteredImg1->data.ptr);
    
    mindisp = state->minDisparity;
    ndisp = state->numberOfDisparities;
    ftzero = state->preFilterCap;
    FILTERED = (short)((state->minDisparity - 1) << DISPARITY_SHIFT);
    textureThreshold = state->textureThreshold;
    uniquenessRatio = state->uniquenessRatio;

    width = left0->cols;
    height = left0->rows;
    lofs = MAX(ndisp - 1 + mindisp, 0);
    rofs = -MIN(ndisp - 1 + mindisp, 0);
    width1 = width - rofs - ndisp + 1;
    if( lofs >= width || rofs >= width || width1 < 1 )
    {
        cvSet( disp, cvScalarAll(FILTERED) );
        EXIT;
    }

    wsz = state->SADWindowSize; wsz2 = wsz/2;
    bufSize = (ndisp + 2)*sizeof(sad[0]) + height*ndisp*sizeof(hsad[0]) +
        (height + wsz + 2)*sizeof(htext[0]) + height*ndisp*(wsz+1)*sizeof(cbuf[0]);
    bufSize = MAX(bufSize, (width + state->preFilterSize + 2)*(int)sizeof(hsad[0])) + 1024;
    if( !state->slidingSumBuf || state->slidingSumBuf->cols < bufSize )
    {
        cvReleaseMat( &state->slidingSumBuf );
        state->slidingSumBuf = cvCreateMat( 1, bufSize, CV_8U );
    }
    
    icvPrefilter( left0, &left, state->preFilterSize, ftzero, state->slidingSumBuf );
    icvPrefilter( right0, &right, state->preFilterSize, ftzero, state->slidingSumBuf );

    sad = (sum_t*)cvAlignPtr(state->slidingSumBuf->data.ptr + sizeof(sad[0]));
    hsad0 = (sum_t*)cvAlignPtr(sad + ndisp + 1);
    htext = (int*)cvAlignPtr((int*)(hsad0 + height*ndisp) + wsz2 + 2);
    cbuf0 = (uchar*)cvAlignPtr(htext + height + wsz2 + 2);
    lptr0 = left.data.ptr + lofs;
    rptr0 = right.data.ptr + rofs;
    dptr = disp->data.s;
    sstep = left.step;
    dstep = disp->step/sizeof(dptr[0]);
    cstep = height*ndisp;

    for( x = 0; x < TABSZ; x++ )
        tab[x] = (uchar)abs(x - ftzero);

    // initialize buffers
    memset( hsad0, 0, height*ndisp*sizeof(hsad0[0]) );
    memset( htext - wsz2 - 1, 0, (height + wsz + 1)*sizeof(htext[0]) );

    for( x = -wsz2-1; x < wsz2; x++ )
    {
        hsad = hsad0; cbuf = cbuf0 + (x + wsz2 + 1)*cstep;
        lptr = lptr0 + MIN(MAX(x, -lofs), width-lofs-1);
        rptr = rptr0 + MIN(MAX(x, -rofs), width-rofs-1);

        for( y = 0; y < height; y++, hsad += ndisp, cbuf += ndisp, lptr += sstep, rptr += sstep )
        {
            int lval = lptr[0];
            for( d = 0; d < ndisp; d++ )
            {
                int diff = abs(lval - rptr[d]);
                cbuf[d] = (uchar)diff;
                hsad[d] = (sum_t)(hsad[d] + diff);
            }
            htext[y] += tab[lval];
        }
    }

    // initialize the left and right borders of the disparity map
    for( y = 0; y < height; y++ )
    {
        for( x = 0; x < lofs; x++ )
            dptr[y*dstep + x] = FILTERED;
        for( x = lofs + width1; x < width; x++ )
            dptr[y*dstep + x] = FILTERED;
    }
    dptr += lofs;

    for( x = 0; x < width1; x++, dptr++ )
    {
        int x0 = x - wsz2 - 1, x1 = x + wsz2;
        const uchar* cbuf_sub = cbuf0 + ((x0 + wsz2 + 1) % (wsz + 1))*cstep;
        uchar* cbuf = cbuf0 + ((x1 + wsz2 + 1) % (wsz + 1))*cstep;
        hsad = hsad0;
        lptr_sub = lptr0 + MIN(MAX(x0, -lofs), width-1-lofs);
        lptr = lptr0 + MIN(MAX(x1, -lofs), width-1-lofs);
        rptr = rptr0 + MIN(MAX(x1, -rofs), width-1-rofs);

        for( y = 0; y < height; y++, cbuf += ndisp, cbuf_sub += ndisp,
             hsad += ndisp, lptr += sstep, lptr_sub += sstep, rptr += sstep )
        {
            int lval = lptr[0];
#if CV_SSE2            
            __m128i lv = _mm_set1_epi8((char)lval), z = _mm_setzero_si128();
            for( d = 0; d < ndisp; d += 16 )
            {
                __m128i rv = _mm_loadu_si128((const __m128i*)(rptr + d));
                __m128i hsad_l = _mm_load_si128((__m128i*)(hsad + d));
                __m128i hsad_h = _mm_load_si128((__m128i*)(hsad + d + 8));
                __m128i cbs = _mm_load_si128((const __m128i*)(cbuf_sub + d));
                __m128i diff = _mm_adds_epu8(_mm_subs_epu8(lv, rv), _mm_subs_epu8(rv, lv));
                __m128i diff_h = _mm_sub_epi16(_mm_unpackhi_epi8(diff, z), _mm_unpackhi_epi8(cbs, z));
                _mm_store_si128((__m128i*)(cbuf + d), diff);
                diff = _mm_sub_epi16(_mm_unpacklo_epi8(diff, z), _mm_unpacklo_epi8(cbs, z));
                hsad_h = _mm_add_epi16(hsad_h, diff_h);
                hsad_l = _mm_add_epi16(hsad_l, diff);
                _mm_store_si128((__m128i*)(hsad + d), hsad_l);
                _mm_store_si128((__m128i*)(hsad + d + 8), hsad_h);
            }
#else
            for( d = 0; d < ndisp; d++ )
            {
                int diff = abs(lval - rptr[d]);
                cbuf[d] = (uchar)diff;
                hsad[d] = (sum_t)(hsad[d] + diff - cbuf_sub[d]);
            }
#endif
            htext[y] += tab[lval] - tab[lptr_sub[0]];
        }

        // fill borders
        for( y = 0; y <= wsz2; y++ )
        {
            htext[height+y] = htext[height-1];
            htext[-y-1] = htext[0];
        }

        // initialize sums
        for( d = 0; d < ndisp; d++ )
            sad[d] = (sum_t)(hsad0[d]*(wsz2 + 2));
        
        hsad = hsad0 + ndisp;
        for( y = 1; y < wsz2; y++, hsad += ndisp )
            for( d = 0; d < ndisp; d++ )
                sad[d] = (sum_t)(sad[d] + hsad[d]);
        int tsum = 0;
        for( y = -wsz2-1; y < wsz2; y++ )
            tsum += htext[y];

        // finally, start the real processing
        for( y = 0; y < height; y++ )
        {
            int minsad = INT_MAX, mind = -1;
            hsad = hsad0 + MIN(y + wsz2, height-1)*ndisp;
            hsad_sub = hsad0 + MAX(y - wsz2 - 1, 0)*ndisp;
#if CV_SSE2
            __m128i minsad8 = _mm_set1_epi16(SHRT_MAX);
            __m128i mind8 = _mm_set1_epi16(-1), d8 = d0_8, mask;

            for( d = 0; d < ndisp; d += 8 )
            {
                __m128i v0 = _mm_load_si128((__m128i*)(hsad_sub + d));
                __m128i v1 = _mm_load_si128((__m128i*)(hsad + d));
                __m128i sad8 = _mm_load_si128((__m128i*)(sad + d));
                sad8 = _mm_sub_epi16(sad8, v0);
                sad8 = _mm_add_epi16(sad8, v1);

                mask = _mm_cmpgt_epi16(minsad8, sad8);
                _mm_store_si128((__m128i*)(sad + d), sad8);
                minsad8 = _mm_min_epi16(minsad8, sad8);
                mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(d8,mind8),mask));
                d8 = _mm_add_epi16(d8, dd_8);
            }

            __m128i minsad82 = _mm_unpackhi_epi64(minsad8, minsad8);
            __m128i mind82 = _mm_unpackhi_epi64(mind8, mind8);
            mask = _mm_cmpgt_epi16(minsad8, minsad82);
            mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(mind82,mind8),mask));
            minsad8 = _mm_min_epi16(minsad8, minsad82);

            minsad82 = _mm_shufflelo_epi16(minsad8, _MM_SHUFFLE(3,2,3,2));
            mind82 = _mm_shufflelo_epi16(mind8, _MM_SHUFFLE(3,2,3,2));
            mask = _mm_cmpgt_epi16(minsad8, minsad82);
            mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(mind82,mind8),mask));
            minsad8 = _mm_min_epi16(minsad8, minsad82);

            minsad82 = _mm_shufflelo_epi16(minsad8, 1);
            mind82 = _mm_shufflelo_epi16(mind8, 1);
            mask = _mm_cmpgt_epi16(minsad8, minsad82);
            mind8 = _mm_xor_si128(mind8,_mm_and_si128(_mm_xor_si128(mind82,mind8),mask));
            mind = (short)_mm_cvtsi128_si32(mind8);
            minsad = sad[mind];
#else
            for( d = 0; d < ndisp; d++ )
            {
                int currsad = sad[d] + hsad[d] - hsad_sub[d];
                sad[d] = (sum_t)currsad;
                if( currsad < minsad )
                {
                    minsad = currsad;
                    mind = d;
                }
            }
#endif
            tsum += htext[y + wsz2] - htext[y - wsz2 - 1];
            if( tsum < textureThreshold )
            {
                dptr[y*dstep] = FILTERED;
                continue;
            }

            if( uniquenessRatio > 0 )
            {
                int thresh = minsad + (minsad * uniquenessRatio/100);
#if CV_SSE2
                __m128i thresh8 = _mm_set1_epi16((short)(thresh + 1));
                __m128i d1 = _mm_set1_epi16((short)(mind-1)), d2 = _mm_set1_epi16((short)(mind+1));
                __m128i d8 = d0_8;

                for( d = 0; d < ndisp; d += 8 )
                {
                    __m128i sad8 = _mm_load_si128((__m128i*)(sad + d));
                    __m128i mask = _mm_cmpgt_epi16( thresh8, sad8 );
                    if( _mm_movemask_epi8(mask) )
                    {
                        mask = _mm_and_si128(mask, _mm_or_si128(_mm_cmpgt_epi16(d1,d8), _mm_cmpgt_epi16(d8,d2)));
                        if( _mm_movemask_epi8(mask) )
                            break;
                    }
                    d8 = _mm_add_epi16(d8, dd_8);
                }
#else
                for( d = 0; d < ndisp; d++ )
                {
                    if( sad[d] <= thresh && (d < mind-1 || d > mind+1))
                        break;
                }
#endif
                if( d < ndisp )
                {
                    dptr[y*dstep] = FILTERED;
                    continue;
                }
            }
            
            {
            sad[-1] = sad[1];
            sad[ndisp] = sad[ndisp-2];
            int p = sad[mind+1], n = sad[mind-1], d = p + n - 2*sad[mind];
            dptr[y*dstep] = (short)(((ndisp - mind - 1 + mindisp)*256 + (d != 0 ? (p-n)*128/d : 0) + 15) >> 4);
            }
        }
    }

    __END__;
}

/* End of file. */
