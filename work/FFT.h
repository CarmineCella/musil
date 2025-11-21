// FFT.h
// 

#ifndef FFT_H
#define FFT_H 

//#define USE_VDSP  

#include "constants.h"
#include "algorithms.h"

#ifdef USE_VDSP
	#include <vecLib/vDSP.h>
#endif

#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cmath>

namespace soundmath {
	//! Peak representation
	template <typename T>
	struct Peak {
		Peak() {
			amp = 0;
			freq = 0;
		}
		T amp;
		T freq;
	};
	
	//! Metatemplate sine/cosine generation
	template<unsigned M, unsigned N, unsigned B, unsigned A>
		struct SinCosSeries {
		static double value () {
			return 1 - (A * M_PI / B) * (A * M_PI / B) / M / (M + 1)
				*SinCosSeries<M + 2, N, B, A>::value ();
		}
	};
	
	template<unsigned N, unsigned B, unsigned A>
		struct SinCosSeries<N, N, B, A> {
		static double value () { return 1.; }
	};
	
	template<unsigned B, unsigned A, typename T = double>
		struct Sin;
	
	template<unsigned B, unsigned A>
		struct Sin<B,A,float> {
		static float value () {
			return (A * M_PI / B) * SinCosSeries<2, 24, B, A>::value ();
		}
	};
	
	template<unsigned B, unsigned A>
		struct Sin<B, A, double> {
		static double value () {
			return (A * M_PI / B) * SinCosSeries<2, 34, B, A>::value ();
		}
	};
	
	template<unsigned B, unsigned A, typename T=double>
		struct Cos;
	
	template<unsigned B, unsigned A>
		struct Cos<B, A, float> {
		static float value () {
			return SinCosSeries<1, 23, B, A>::value ();
		}
	};
	template<unsigned B, unsigned A>
		struct Cos<B, A, double> {
		static double value () {
			return SinCosSeries<1, 33, B, A>::value ();
		}
	};
	
	//! Metatemplate version of the Danielson Lanczos Algorithm
	template<unsigned N, typename T = double>
		class DanielsonLanczos {
		DanielsonLanczos<N/2, T> next;
		public:
		void apply (T* data) {
			next.apply (data);
			next.apply (data + N);
	
			T wtemp, tempr, tempi, wr, wi, wpr, wpi;
			//      wtemp = sin (M_PI / N);
			wtemp = -Sin<N, 1, T>::value ();
			wpr = -2.0 * wtemp * wtemp;
			//      wpi = -sin (2 * M_PI / N);
			wpi = -Sin<N, 2, T>::value ();
			wr = 1.0;
			wi = 0.0;
			for (unsigned i = 0; i < N; i += 2) {
				tempr = data[i + N] * wr - data[i + N + 1] * wi;
				tempi = data[i + N] * wi + data[i + N + 1] * wr;
				data[i + N] = data[i] - tempr;
				data[i + N + 1] = data[i + 1] - tempi;
				data[i] += tempr;
				data[i + 1] += tempi;
	
				wtemp = wr;
				wr += wr * wpr - wi * wpi;
				wi += wi * wpr + wtemp * wpi;
			}
		}
	};
	
	template<typename T>
	class DanielsonLanczos<4, T> {
	public:
	   void apply (T* data) {
		  T tr = data[2];
		  T ti = data[3];
		  data[2] = data[0] - tr;
		  data[3] = data[1] - ti;
		  data[0] += tr;
		  data[1] += ti;
		  tr = data[6];
		  ti = data[7];
		  data[6] = data[5] - ti;
		  data[7] = tr - data[4];
		  data[4] += tr;
		  data[5] += ti;
	
		  tr = data[4];
		  ti = data[5];
		  data[4] = data[0] - tr;
		  data[5] = data[1] - ti;
		  data[0] += tr;
		  data[1] += ti;
		  tr = data[6];
		  ti = data[7];
		  data[6] = data[2] - tr;
		  data[7] = data[3] - ti;
		  data[2] += tr;
		  data[3] += ti;
	   }
	};
	
	template<typename T>
	class DanielsonLanczos<2, T> {
	public:
	   void apply (T* data) {
		  T tr = data[2];
		  T ti = data[3];
		  data[2] = data[0] - tr;
		  data[3] = data[1] - ti;
		  data[0] += tr;
		  data[1] += ti;
	   }
	};
	
	//! Base class for the all FFTs
	template<typename T>
	class AbstractFFT {
	public:
		virtual ~AbstractFFT  () {}
		virtual void forward (T*) = 0;
		virtual void inverse (T*) = 0;
	};
	
	//! Fast-Fourier transform based on metatemplates
	template<unsigned P, typename T = double>
	class FFT : public AbstractFFT<T> {
		enum { N = 1 << P };
		DanielsonLanczos<N, T> recursion;
	public:
		FFT<P, T> () {
#ifdef USE_VDSP
			m_fftSize = N;
			m_fftSizeOver2 = m_fftSize / 2;
			m_log2n = P; //log2f (m_fftSize); // bins
			m_log2nOver2 = m_log2n / 2;

			m_splitData.realp = new T[m_fftSize];
			m_splitData.imagp = new T[m_fftSize];

			m_scale = 1.0f / (T) (4.0f * m_fftSize);

			// allocate the fft object once
			m_fftSetup = vDSP_create_fftsetup (m_log2n, FFT_RADIX2);
			if (m_fftSetup == NULL || /*m_inReal == NULL || m_outReal == NULL || */
					m_splitData.realp == NULL || m_splitData.imagp == NULL /*|| m_window == NULL*/) {
				throw std::runtime_error ("FFT_Setup failed to allocate enough memory");
			}
#endif
		}
		
		virtual ~FFT () {
#ifdef USE_VDSP
			delete [] (m_splitData.realp);
			delete [] (m_splitData.imagp);
#endif
		}
		void conjugate (T* data) {
			for (int i = 0; i < N; ++i) {
				data[2 * i + 1] *= -1;
			}
		}
		void scramble (T* data) {
			int i, m, j = 1;
			for (i = 1; i < 2 * N; i += 2) {
				if (j > i) {
					std::swap (data[j - 1], data[i - 1]);
					std::swap (data[j], data[i]);
				}
				m = N;
				while (m >= 2 && j > m) {
					j -= m;
					m >>= 1;
				}
				j += m;
			}
		}
		public:
		enum { id = P };
		void forward (T* data) {
#ifdef USE_VDSP			
			//convert to split complex format with evens in real and odds in imag
			vDSP_ctoz ((COMPLEX *) data, 2, &m_splitData, 1, m_fftSize);

			//calc fft
			vDSP_fft_zip (m_fftSetup, &m_splitData, 1, m_log2n, FFT_FORWARD);

			//m_splitData.imagp[0] = 0.0;

			vDSP_ztoc (&m_splitData, 1, (COMPLEX*) data, 2, m_fftSize);
#else
			scramble (data);
			recursion.apply (data);
#endif
		}
		void inverse (T* data) {
#ifdef USE_VDSP
			//convert to split complex format with evens in real and odds in imag
			vDSP_ctoz ((COMPLEX *) data, 2, &m_splitData, 1, m_fftSize);
			
			vDSP_fft_zip (m_fftSetup, &m_splitData, 1, m_log2n, FFT_INVERSE);
			vDSP_ztoc (&m_splitData, 1, (COMPLEX*) data, 2, m_fftSize);

			//vDSP_vsmul (buffer, 1, &m_scale, buffer, 1, m_fftSize);			
#else
			conjugate (data);
			scramble (data);
			recursion.apply (data);

#endif
		}
	private:
#ifdef USE_VDSP
		int	m_fftSize;
		int m_fftSizeOver2;
		int m_log2n;
		int m_log2nOver2;
		int m_windowSize;

		T* m_inReal;
		T* m_outReal;
		T* m_window;

		T m_scale;

		FFTSetup m_fftSetup;
		COMPLEX_SPLIT m_splitData;		
#endif
	};
	
	template <typename T>
		AbstractFFT<T>* createFFT (int N) {
			switch (N) {
			case 4:
				return new FFT<2, T> ();
				break;
			case 8:
				return new FFT<3, T> ();
				break;
			case 16:
				return new FFT<4, T> ();
				break;
			case 32:
				return new FFT<5, T> ();
				break;
			case 64:
				return new FFT<6, T> ();
				break;
			case 128:
				return new FFT<7, T> ();
				break;
			case 256:
				return new FFT<8, T> ();
				break;
			case 512:
				return new FFT<9, T> ();
				break;
			case 1024:
				return new FFT<10, T> ();
				break;
			case 2048:
				return new FFT<11, T> ();
				break;
			case 4096:
				return new FFT<12, T> ();
				break;
			case 8192:
				return new FFT<13, T> ();
				break;
			case 16384:
				return new FFT<14, T> ();
				break;
			case 32768:
				return new FFT<15, T> ();
				break;
			case 65536:
				return new FFT<16, T> ();
				break;
			case 131072:
				return new FFT<17, T> ();
				break;
			case 262144:
				return new FFT<18, T> ();
				break;
			case 524288:
				return new FFT<19, T> ();
				break;
			case 1048576:
				return new FFT<20, T> ();
				break;
			case 2097152:
				return new FFT<21, T> ();
				break;
			case 4194304:
				return new FFT<22, T> ();
				break;
			case 8388608:
				return new FFT<23, T> ();
				break;
			case 16777216:
				return new FFT<24, T> ();
				break;
			default:
				throw std::runtime_error ("invalid size requested for fft");
			}
	}

	template <typename T>
	void fft (T *fftBuffer, long fftFrameSize, long sign) {
		T wr, wi, arg, *p1, *p2, temp;
		T tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
		long i, bitm, j, le, le2, k;
	
		int fftFrameSize2 = fftFrameSize << 1;
			
		for (i = 2; i < fftFrameSize2 - 2; i += 2) {
			for (bitm = 2, j = 0; bitm < fftFrameSize2; bitm <<= 1) {
				if (i & bitm) j++;
				j <<= 1;
			}
			if (i < j) {
				p1 = fftBuffer + i;
				p2 = fftBuffer + j;
				temp = *p1;
				*(p1++) = *p2;
				*(p2++) = temp;
				temp = *p1;
				*p1 = *p2;
				*p2 = temp;
			}
		}
	
		for (k = 0, le = 2; k < ceil (log2f (fftFrameSize)); k++) {
			le <<= 1;
			le2 = le >> 1;
			ur = 1.;
			ui = 0.;
			arg = PI / (le2 >> 1);
			wr = cosf (arg);
			wi = sign* sinf (arg);
			for (j = 0; j < le2; j += 2) {
				p1r = fftBuffer + j;
				p1i = p1r + 1;
				p2r = p1r + le2;
				p2i = p2r + 1;
				for (i = j; i < fftFrameSize2; i += le) {
					tr = *p2r * ur - *p2i * ui;
					ti = *p2r * ui + *p2i * ur;
					*p2r = *p1r - tr;
					*p2i = *p1i - ti;
					*p1r += tr;
					*p1i += ti;
					p1r += le;
					p1i += le;
					p2r += le;
					p2i += le;
				}
				tr = ur*wr - ui*wi;
				ui = ur*wi + ui*wr;
				ur = tr;
			}
		}
	}

	// ---------------------------------------------------------------------- //
	template <typename T>
	void hanningz (T* out, int N) {
		for (int i = 0; i < N; ++i) {
			out[i] = .5 * (1. - cos (TWOPI * (T) i / N));
		}
	}
	
	template <typename T>
	void makeWindow (T* out, int N, T a0, T a1, T a2) {
		// .5, .5, 0     --> hanning
		// .54, .46, 0   --> hamming
		// .42, .5, 0.08 --> blackman
		for (int i = 0; i < N; ++i) {
			out[i] = a0 - a1 * cos ((TWOPI * (T) i) / (N - 1)) + a2 *
					 cos ((2 * TWOPI * (T) i) / (N - 1)); // hamming, hann or blackman
		}
	}
	
	template <typename T>
	void fftshift (T* cbuf, int N) {
		int N2 = N >> 1;
		for (int i = 0; i < (N2); ++i) {
			//cout << "\t" << i << endl;
			T tmp = cbuf[i];
			cbuf[i] = cbuf[i + N2];
			cbuf[i + N2] = tmp;
		}
	}
	
	template <typename T>
	T princarg (T phi) {
		T a = phi / TWOPI;
		int k = (int) round (a);
		return phi - (T) k * TWOPI;
	}
	
	template <typename T>
	void pol2rect (T* cbuf, int N) {
#ifdef USE_VDSP
		vDSP_rect (cbuf, 2, cbuf, 2, N);
#else		
		for (int i = 0; i < N; ++i) {
			T amp = cbuf[2 * i];
			T phi = cbuf[2 * i + 1];
			T real = amp * cosf (phi);
			T imag = amp * sinf (phi);
			cbuf[2 * i] = real;
			cbuf[2 * i + 1] = imag;
		}
#endif
	}
	
	template <typename T>
	void rect2pol (T* cbuf, int N) {
#ifdef USE_VDSP
		vDSP_polar (cbuf, 2, cbuf, 2, N);
#else
		for (int i = 0; i < N; ++i) {
			T real = cbuf[2 * i];
			T imag = cbuf[2 * i + 1];
			T amp = sqrtf (real * real + imag * imag);
			//T amp = hypot (real, imag);
			T phi = atan2f (imag, real);
			cbuf[2 * i] = amp;
			cbuf[2 * i + 1] = phi;
		}
#endif
	}
	
	template <typename T>
	int locmax (const T* amp, int N, int* max) {
		T maxPeak = amp[1];
		int count = 0;
		for (int i = 1; i < N - 1; ++i) {
			T magCurr = amp[i];
			T magPrev = amp[(i - 1)];
			T magNext = amp[(i + 1)];
	
			if (magCurr > magPrev && magCurr > magNext) {
				max[count] = i;
				++count;
				if (magCurr > maxPeak) maxPeak = magCurr;
			}
		}
		return count;
	}
	
	template <typename T>
		int locmax2 (const T* cplx, int N, int* max) {
		T maxPeak = cplx[2];
		int count = 0;
		for (int i = 2; i < N - 2; ++i) {
			T magCurr = cplx[2 * i];
			T magPrev = cplx[2 * (i - 1)];
			T magPrevPrev = cplx[2 * (i - 2)];
			T magNext = cplx[2 * (i + 1)];
			T magNextNext = cplx[2 * (i + 2)];
	
			if (magCurr > magPrev && magCurr > magNext &&
				magCurr > magPrevPrev && magCurr > magNextNext) {
				max[count] = i;
				++count;
				if (magCurr > maxPeak) maxPeak = magCurr;
			}
		}
		return count;
	}
	template <typename T>
	T locmax2AmpFreq (const T* amp, const T* freq, int size, std::vector<int>& max, T FUNDAMENTAL) {
		T maxPeak = amp[2];
		for (int i = 2; i < size - 2; ++i) {
			T magCurr = amp[i]; //20 * log10 (amp[i] + .00000001);
			T magPrev = amp[i - 1]; //20 * log10 (amp[i - 1] + .00000001);
			T magPrevPrev = amp[i - 2]; //20 * log10 (amp[i - 1] + .00000001);
			T magNext = amp[i + 1]; // 20 * log10 (amp[i + 1] + .00000001);
			T magNextNext = amp[i + 2]; // 20 * log10 (amp[i + 1] + .00000001);
			
			if (magCurr > magPrev && magCurr > magNext &&
				magCurr > magPrevPrev && magCurr > magNextNext) {
				max.push_back (i);
				if (magCurr > maxPeak) maxPeak = magCurr;
			}        
		}
	
		return maxPeak;
	}
	
	// rect to amp-freq
	template <typename T>
	inline void ampFreqPhaseDiff (const T* cbuffer, T* amp, T* freq, T* oldPhi,
		int N, int hop, T R) {
		T osamp = N / hop;
		T freqPerBin = R / (T) N;
		T expct = TWOPI * (T) hop / (T) N;
		rect2pol<T> (cbuffer, N);
		for (int i = 0; i < N; ++i) {
			amp[i] = cbuffer[2 * i];
			T phase = cbuffer[2 * i + 1];
			T tmp = phase - oldPhi[i];
			oldPhi[i] = phase;
			tmp -= (T) i * expct;
			int qpd = (int) (tmp / PI);
			if (qpd >= 0) qpd += qpd & 1;
			else qpd -= qpd & 1;
			tmp -= PI * (T) qpd;
			tmp = osamp * tmp / (2. * PI);
			tmp = (T) i * freqPerBin + tmp * freqPerBin;
			freq[i] = tmp;
		}
	}

	template <typename T>
	void ampFreqParabolic (const T* cbuffer, T* amp, T* freq, int N,  double R) {
		T freqPerBin = (R ) / (T) N;
		T min = 0;
		for (int i = 0; i < N; ++i) {
			amp[i] = sqrtf (cbuffer[2 * i] * cbuffer[2 * i] + cbuffer[2 * i + 1] * cbuffer[2 * i + 1]);
		}
		
		freq[0] = freqPerBin;
		for (int i = 1; i < N - 1; ++i) {
			freq[i] = parabolicInterpolate (freqPerBin * (i - 1), freqPerBin * (i), freqPerBin * (i + 1),
				amp[i - 1], amp[i], amp[i + 1], &min);
		}
	}

	template <typename T>
	void ampFreqBins (const T* cbuffer, T* amp, T* freq, int N,  double R) {
		T freqPerBin = (R) / (T) N;
		for (int i = 0; i < N; ++i) {
			amp[i] = sqrtf (cbuffer[2 * i] * cbuffer[2 * i] + cbuffer[2 * i + 1] * cbuffer[2 * i + 1]);
		}
		
		for (int i = 1; i < N - 1; ++i) {
			freq[i] = (T) i * freqPerBin;
		}
	}
	template<typename T>
	void sortSpectrum (Peak<T>* peaks, int k) {
		Peak<T> p;
		for (int i = 0; i < k; ++i) {
			for (int j = i + 1; j < k; ++j) {
				//if (peaks[i].freq > peaks[j].freq) {
				if (peaks[i].amp < peaks[j].amp) {
					p = peaks[i];
					peaks[i] = peaks[j];
					peaks[j] = p;
				}
			}	
		}	
	}
}

#endif	// FFT_H 

// EOF
