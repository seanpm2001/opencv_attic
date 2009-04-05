/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
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
//   * The name of the copyright holders may not be used to endorse or promote products
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

/* ////////////////////////////////////////////////////////////////////
//
//  Filling CvMat/IplImage instances with random numbers
//
// */

#include "_cxcore.h"

namespace cv
{

///////////////////////////// Functions Declaration //////////////////////////////////////

/*
   Multiply-with-carry generator is used here:
   temp = ( A*X(n) + carry )
   X(n+1) = temp mod (2^32)
   carry = temp / (2^32)
*/

#define  RNG_NEXT(x)    ((uint64)(unsigned)(x)*RNG::A + ((x) >> 32))

/***************************************************************************************\
*                           Pseudo-Random Number Generators (PRNGs)                     *
\***************************************************************************************/

template<typename T> static void
RandBits_( Mat& _arr, uint64* state, const void* _param )
{
    uint64 temp = *state;
    const int* param = (const int*)_param;
    int small_flag = (param[12]|param[13]|param[14]|param[15]) <= 255;
    Size size = getContinuousSize(_arr,_arr.channels());

    for( int y = 0; y < size.height; y++ )
    {
        T* arr = (T*)(_arr.data + _arr.step*y);
        int i, k = 3;
        const int* p = param;

        if( !small_flag )
        {
            for( i = 0; i <= size.width - 4; i += 4 )
            {
                int t0, t1;

                temp = RNG_NEXT(temp);
                t0 = ((int)temp & p[i + 12]) + p[i];
                temp = RNG_NEXT(temp);
                t1 = ((int)temp & p[i + 13]) + p[i+1];
                arr[i] = saturate_cast<T>(t0);
                arr[i+1] = saturate_cast<T>(t1);

                temp = RNG_NEXT(temp);
                t0 = ((int)temp & p[i + 14]) + p[i+2];
                temp = RNG_NEXT(temp);
                t1 = ((int)temp & p[i + 15]) + p[i+3];
                arr[i+2] = saturate_cast<T>(t0);
                arr[i+3] = saturate_cast<T>(t1);

                if( !--k )
                {
                    k = 3;
                    p -= 12;
                }
            }
        }
        else
        {
            for( i = 0; i <= size.width - 4; i += 4 )
            {
                int t0, t1, t;

                temp = RNG_NEXT(temp);
                t = (int)temp;
                t0 = (t & p[i + 12]) + p[i];
                t1 = ((t >> 8) & p[i + 13]) + p[i+1];
                arr[i] = saturate_cast<T>(t0);
                arr[i+1] = saturate_cast<T>(t1);

                t0 = ((t >> 16) & p[i + 14]) + p[i + 2];
                t1 = ((t >> 24) & p[i + 15]) + p[i + 3];
                arr[i+2] = saturate_cast<T>(t0);
                arr[i+3] = saturate_cast<T>(t1);

                if( !--k )
                {
                    k = 3;
                    p -= 12;
                }
            }
        }

        for( ; i < size.width; i++ )
        {
            unsigned t0;
            temp = RNG_NEXT(temp);

            t0 = ((int)temp & p[i + 12]) + p[i];
            arr[i] = saturate_cast<T>(t0);
        }
    }

    *state = temp;
}


template<typename T, typename PT> static void
Randi_( Mat& _arr, uint64* state, const void* _param )
{
    uint64 temp = *state;
    const PT* param = (const PT*)_param;
    Size size = getContinuousSize(_arr,_arr.channels());

    for( int y = 0; y < size.height; y++ )
    {
        T* arr = (T*)(_arr.data + _arr.step*y);
        int i, k = 3;
        const PT* p = param;

        for( i = 0; i <= size.width - 4; i += 4 )
        {
            PT f0, f1;
            temp = RNG_NEXT(temp);
            f0 = (int)temp * p[i+12] + p[i];
            temp = RNG_NEXT(temp);
            f1 = (int)temp * p[i+13] + p[i+1];
            arr[i] = saturate_cast<T>(cvFloor(f0));
            arr[i+1] = saturate_cast<T>(cvFloor(f1));

            temp = RNG_NEXT(temp);
            f0 = (int)temp * p[i+14] + p[i+2];
            temp = RNG_NEXT(temp);
            f1 = (int)temp * p[i+15] + p[i+3];
            arr[i+2] = saturate_cast<T>(cvFloor(f0));
            arr[i+3] = saturate_cast<T>(cvFloor(f1));

            if( !--k )
            {
                k = 3;
                p -= 12;
            }
        }

        for( ; i < size.width; i++ )
        {
            temp = RNG_NEXT(temp);
            arr[i] = saturate_cast<T>(cvFloor((int)temp * p[i + 12] + p[i]));
        }
    }

    *state = temp;
}


template<typename T, typename iT> static void
Randf_( Mat& _arr, uint64* state, const void* _param )
{
    uint64 temp = *state;
    const T* param = (const T*)_param;
    Size size = getContinuousSize(_arr,_arr.channels());

    for( int y = 0; y < size.height; y++ )
    {
        T* arr = (T*)(_arr.data + _arr.step*y);
        int i, k = 3;
        const T* p = param;
        for( i = 0; i <= size.width - 4; i += 4 )
        {
            T f0, f1;

            temp = RNG_NEXT(temp);
            f0 = (iT)temp*p[i+12] + p[i];
            temp = RNG_NEXT(temp);
            f1 = (iT)temp*p[i+13] + p[i+1];
            arr[i] = f0; arr[i+1] = f1;

            temp = RNG_NEXT(temp);
            f0 = (iT)temp*p[i+14] + p[i+2];
            temp = RNG_NEXT(temp);
            f1 = (iT)temp*p[i+15] + p[i+3];
            arr[i+2] = f0; arr[i+3] = f1;

            if( !--k )
            {
                k = 3;
                p -= 12;
            }
        }

        for( ; i < size.width; i++ )
        {
            temp = RNG_NEXT(temp);
            arr[i] = (iT)temp*p[i+12] + p[i];
        }
    }

    *state = temp;
}


/***************************************************************************************\
    The code below implements algorithm from the paper:

    G. Marsaglia and W.W. Tsang,
    The Monty Python method for generating random variables,
    ACM Transactions on Mathematical Software, Vol. 24, No. 3,
    Pages 341-350, September, 1998.
\***************************************************************************************/

static CvStatus CV_STDCALL
Randn_0_1_32f_C1R( float* arr, int len, uint64* state )
{
    uint64 temp = *state;
    int i;
    temp = RNG_NEXT(temp);

    for( i = 0; i < len; i++ )
    {
        double x, y, v, ax, bx;

        for(;;)
        {
            x = ((int)temp)*1.167239e-9;
            temp = RNG_NEXT(temp);
            ax = fabs(x);
            v = 2.8658 - ax*(2.0213 - 0.3605*ax);
            y = ((unsigned)temp)*2.328306e-10;
            temp = RNG_NEXT(temp);

            if( y < v || ax < 1.17741 )
                break;

            bx = x;
            x = bx > 0 ? 0.8857913*(2.506628 - ax) : -0.8857913*(2.506628 - ax);
            
            if( y > v + 0.0506 )
                break;

            if( std::log(y) < .6931472 - .5*bx*bx )
            {
                x = bx;
                break;
            }

            if( std::log(1.8857913 - y) < .5718733-.5*x*x )
                break;

            do
            {
                v = ((int)temp)*4.656613e-10;
                x = -std::log(fabs(v))*.3989423;
                temp = RNG_NEXT(temp);
                y = -std::log(((unsigned)temp)*2.328306e-10);
                temp = RNG_NEXT(temp);
            }
            while( y+y < x*x );

            x = v > 0 ? 2.506628 + x : -2.506628 - x;
            break;
        }

        arr[i] = (float)x;
    }
    *state = temp;
    return CV_OK;
}


template<typename T, typename PT> static void
Randn_( Mat& _arr, uint64* state, const void* _param )
{
    const int RAND_BUF_SIZE = 96;
    float buffer[RAND_BUF_SIZE];
    const PT* param = (const PT*)_param;
    Size size = getContinuousSize(_arr);

    for( int y = 0; y < size.height; y++ )
    {
        T* arr = (T*)(_arr.data + _arr.step*y);
        int i, j, len = RAND_BUF_SIZE;
        for( i = 0; i < size.width; i += RAND_BUF_SIZE )
        {
            int k = 3;
            const PT* p = param;

            if( i + len > size.width )
                len = size.width - i;

            Randn_0_1_32f_C1R( buffer, len, state );

            for( j = 0; j <= len - 4; j += 4 )
            {
                PT f0, f1;

                f0 = buffer[j]*p[j+12] + p[j];
                f1 = buffer[j+1]*p[j+13] + p[j+1];
                arr[i+j] = saturate_cast<T>(f0);
                arr[i+j+1] = saturate_cast<T>(f1);

                f0 = buffer[j+2]*p[j+14] + p[j+2];
                f1 = buffer[j+3]*p[j+15] + p[j+3];
                arr[i+j+2] = saturate_cast<T>(f0);
                arr[i+j+3] = saturate_cast<T>(f1);

                if( --k == 0 )
                {
                    k = 3;
                    p -= 12;
                }
            }

            for( ; j < len; j++ )
                arr[i+j] = saturate_cast<T>(buffer[j]*p[j+12] + p[j]);
        }
    }
}


typedef void (*RandFunc)(Mat& dst, uint64* state, const void* param);

void RNG::fill( Mat& mat, int disttype, const Scalar& param1, const Scalar& param2 )
{
    static RandFunc rngtab[3][8] =
    {
        {RandBits_<uchar>, 0,
        RandBits_<ushort>,
        RandBits_<short>,
        RandBits_<int>, 0, 0},

        {Randi_<uchar,float>, 0,
        Randi_<ushort,float>,
        Randi_<short,float>,
        Randi_<int,float>,
        Randf_<float,int>,
        Randf_<double,int64>, 0},

        {Randn_<uchar,float>, 0,
        Randn_<ushort,float>,
        Randn_<short,float>,
        Randn_<int,float>,
        Randn_<float,float>,
        Randn_<double,double>, 0}
    };

    int depth = mat.depth(), channels = mat.channels();
    double dparam[2][12];
    float fparam[2][12];
    int iparam[2][12];
    void* param = dparam;
    int i, fast_int_mode = 0;
    RandFunc func = 0;

    CV_Assert( channels <= 4 );

    if( disttype == UNIFORM )
    {
        if( depth <= CV_32S )
        {
            for( i = 0, fast_int_mode = 1; i < channels; i++ )
            {
                int t0 = iparam[0][i] = cvCeil(param1.val[i]);
                int t1 = iparam[1][i] = cvFloor(param2.val[i]) - t0;
                double diff = param1.val[i] - param2.val[i];

                fast_int_mode &= INT_MIN <= diff && diff <= INT_MAX && (t1 & (t1 - 1)) == 0;
            }
        }

        if( fast_int_mode )
        {
            for( i = 0; i < channels; i++ )
                iparam[1][i]--;
        
            for( ; i < 12; i++ )
            {
                int t0 = iparam[0][i - channels];
                int t1 = iparam[1][i - channels];

                iparam[0][i] = t0;
                iparam[1][i] = t1;
            }

            func = rngtab[0][depth];
            param = iparam;
        }
        else
        {
            double scale = depth == CV_64F ?
                5.4210108624275221700372640043497e-20 : // 2**-64
                2.3283064365386962890625e-10;           // 2**-32

            // for each channel i compute such dparam[0][i] & dparam[1][i],
            // so that a signed 32/64-bit integer X is transformed to
            // the range [param1.val[i], param2.val[i]) using
            // dparam[1][i]*X + dparam[0][i]
            for( i = 0; i < channels; i++ )
            {
                double t0 = param1.val[i];
                double t1 = param2.val[i];
                dparam[0][i] = (t1 + t0)*0.5;
                dparam[1][i] = (t1 - t0)*scale;
            }
            
            func = rngtab[1][depth];
            param = dparam;
        }
    }
    else if( disttype == CV_RAND_NORMAL )
    {
        for( i = 0; i < channels; i++ )
        {
            double t0 = param1.val[i];
            double t1 = param2.val[i];

            dparam[0][i] = t0;
            dparam[1][i] = t1;
        }

        func = rngtab[2][depth];
        param = dparam;
    }
    else
        CV_Error( CV_StsBadArg, "Unknown distribution type" );

    if( !fast_int_mode )
    {
        for( i = channels; i < 12; i++ )
        {
            double t0 = dparam[0][i - channels];
            double t1 = dparam[1][i - channels];

            dparam[0][i] = t0;
            dparam[1][i] = t1;
        }

        if( depth != CV_64F )
        {
            for( i = 0; i < 12; i++ )
            {
                fparam[0][i] = (float)dparam[0][i];
                fparam[1][i] = (float)dparam[1][i];
            }
            param = fparam;
        }
    }

    func( mat, &state, param );
}


#ifdef WIN32
#ifdef WINCE
#	define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif
static DWORD tlsRNGKey = TLS_OUT_OF_INDEXES;

void deleteThreadRNGData()
{
    if( tlsRNGKey != TLS_OUT_OF_INDEXES )
        delete (RNG*)TlsGetValue( tlsRNGKey );
}

RNG& theRNG()
{
    if( tlsRNGKey == TLS_OUT_OF_INDEXES )
    {
        tlsRNGKey = TlsAlloc();
        CV_Assert(tlsRNGKey != TLS_OUT_OF_INDEXES);
    }
    RNG* rng = (RNG*)TlsGetValue( tlsRNGKey );
    if( !rng )
    {
        rng = new RNG;
        TlsSetValue( tlsRNGKey, rng );
    }
    return *rng;
}

#else

static pthread_key_t tlsRNGKey = 0;

static void deleteRNG(void* data)
{
    delete (RNG*)data;
}

RNG& theRNG()
{
    if( !tlsRNGKey )
    {
        pthread_key_create(&tlsRNGKey, deleteRNG);
        CV_Assert(tlsRNGKey != 0);
    }
    RNG* rng = (RNG*)pthread_getspecific(tlsRNGKey);
    if( !rng )
    {
        rng = new RNG;
        pthread_setspecific(tlsRNGKey, rng);
    }
    return *rng;
}

#endif

template<typename T> static void
randShuffle_( Mat& _arr, RNG& rng, double iterFactor )
{
    int sz = _arr.rows*_arr.cols, iters = cvRound(iterFactor*sz);
    if( _arr.isContinuous() )
    {
        T* arr = (T*)_arr.data;
        for( int i = 0; i < iters; i++ )
        {
            int j = (unsigned)rng % sz, k = (unsigned)rng % sz;
            std::swap( arr[j], arr[k] );
        }
    }
    else
    {
        uchar* data = _arr.data;
        int step = _arr.step, cols = _arr.cols;
        for( int i = 0; i < iters; i++ )
        {
            int j1 = (unsigned)rng % sz, k1 = (unsigned)rng % sz;
            int j0 = j1/cols, k0 = k1/cols;
            j1 -= j0*cols; k1 -= k0*cols;
            std::swap( ((T*)(data + step*j0))[j1], ((T*)(data + step*k0))[k1] );
        }
    }
}

typedef void (*RandShuffleFunc)( Mat& dst, RNG& rng, double iterFactor );

void randShuffle( Mat& dst, RNG& rng, double iterFactor )
{
    RandShuffleFunc tab[] =
    {
        0,
        randShuffle_<uchar>, // 1
        randShuffle_<ushort>, // 2
        randShuffle_<Vec_<uchar,3> >, // 3
        randShuffle_<int>, // 4
        0,
        randShuffle_<Vec_<ushort,3> >, // 6
        0,
        randShuffle_<int64>, // 8
        0, 0, 0,
        randShuffle_<Vec_<int,3> >, // 12
        0, 0, 0,
        randShuffle_<Vec_<int64,2> >, // 16
        0, 0, 0, 0, 0, 0, 0,
        randShuffle_<Vec_<int64,3> >, // 24
        0, 0, 0, 0, 0, 0, 0,
        randShuffle_<Vec_<int64,4> > // 32
    };

    CV_Assert( dst.elemSize() <= 32 );
    RandShuffleFunc func = tab[dst.elemSize()];
    CV_Assert( func != 0 );
    func( dst, rng, iterFactor );
}

}

CV_IMPL void
cvRandArr( CvRNG* _rng, CvArr* arr, int disttype, CvScalar param1, CvScalar param2 )
{
    cv::Mat mat = cv::cvarrToMat(arr);
    // !!! this will only work for current 64-bit MWC RNG !!!
    cv::RNG& rng = _rng ? (cv::RNG&)*_rng : cv::theRNG();
    rng.fill(mat, disttype == CV_RAND_NORMAL ?
        cv::RNG::NORMAL : cv::RNG::UNIFORM, param1, param2 );
}

CV_IMPL void cvRandShuffle( CvArr* arr, CvRNG* _rng, double iter_factor )
{
    cv::Mat dst = cv::cvarrToMat(arr);
    cv::RNG& rng = _rng ? (cv::RNG&)*_rng : cv::theRNG();
    cv::randShuffle( dst, rng, iter_factor );
}

/* End of file. */
