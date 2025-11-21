// BlockConv.h
//

#ifndef BLOCKCONV_H
#define BLOCKCONV_H

#include "FFT.h"
#include <cstring>

namespace soundmath {
	//! Fast block convolution in frequency domain
	template <typename T>
	class BlockConv {
	public:
		BlockConv (const T* imp0, const T* imp1, 
			int impSize, int blockSize, T scale) {
			m_bsize = blockSize;
			m_nblock = (int)((impSize + (blockSize - 1)) / blockSize);
	
			m_cfftSize = 2 * 2 * m_bsize;
			m_currBlock	= 0;
	
			m_lastIn = new T[m_bsize];
			m_inFft	= new T[m_cfftSize];
			m_outFft = new T[m_cfftSize];
			m_impFft = new T*[m_nblock];
			m_accFft = new T*[m_nblock];
	
			for (int i = 0; i < m_nblock; i++) {
				m_impFft[i] = new T[m_cfftSize];
				m_accFft[i] = new T[m_cfftSize];
			}
	
			//store impulse FFTs (and reset accumulators)
			const int fft_n = 2 * m_bsize;
			m_fft = createFFT<T> (fft_n);
	
			for (int i = 0; i < m_nblock; i++) {
				int curSize = (impSize > m_bsize) ? m_bsize : impSize;
	
				//zero-pad
				memset (m_impFft[i], 0, m_cfftSize * sizeof (T));
	
				//reset accumulators
				memset (m_accFft[i], 0, m_cfftSize * sizeof (T));
	
				//put responses
				putReal (imp0, m_impFft[i], curSize);
				putImag (imp1, m_impFft[i], curSize);
	
				//FFT of block i
				m_fft->forward (m_impFft[i]);
	
				imp0 += curSize;
				imp1 += curSize;
	
				impSize -= curSize;
			}
	
			memset (m_lastIn, 0, m_bsize * sizeof (T));
	
			m_factor = 1. / (m_cfftSize / 2);
			m_factor *= scale;
		}
	
		int blocks () const {
			return m_nblock;
		}
		void process (const T* input, T* out0, T* out1) {
			const int fft_n = 2 * m_bsize;
	
			//build input sequence
			memset (m_inFft, 0, m_cfftSize * sizeof (T));
	
			//last input
			putReal (m_lastIn, m_inFft, m_bsize);
	
			//current input
			putReal (input, m_inFft + 2 * m_bsize, m_bsize);
	
			//store last input
			memcpy (m_lastIn, input, m_bsize * sizeof (T));
	
			//input FFT
			m_fft->forward (m_inFft);
	
			//FDL calculation
			for (int i = 0; i < m_nblock; i++) {
				if (i < (m_nblock - 1)) {
					complexMultiplyAdd (
						m_inFft, m_impFft[i], m_accFft[m_currBlock], fft_n);
					if (i == 0) {
						memcpy (
							m_outFft, m_accFft[m_currBlock], m_cfftSize * sizeof (T));
					}
				} else {
					complexMultiplyReplace (
						m_inFft, m_impFft[i], m_accFft[m_currBlock], fft_n);
				}
				m_currBlock = (m_currBlock + 1) % m_nblock;
			}
	
			m_currBlock = (m_currBlock + 1) % m_nblock;
	
			// output IFFT
			//fft (m_outFft, fft_n, 1);
			m_fft->inverse (m_outFft);
			
			// get output
			getReal (m_outFft + 2 * m_bsize, out0, m_bsize);
			getImag (m_outFft + 2 * m_bsize, out1, m_bsize);
	
			// normalize data
			norm (out0, m_bsize);
			norm (out1, m_bsize);
		}    
	
	private:
		void putReal (const T* realSrc, T* cplxDest, int num) {
			while (num--) {
				*cplxDest = *realSrc++;
				cplxDest += 2;
			}
		}
	
		void putImag (const T* realSrc, T* cplxDest, int num) {
			cplxDest++;
	
			while (num--) {
				*cplxDest = *realSrc++;
				cplxDest += 2;
			}
		}
	
		void getReal (const T* cplxSrc, T* realDest, int num) {
			while (num--) {
				*realDest++ = *cplxSrc;
				cplxSrc += 2;
			}
		}
	
		void getImag (const T* cplxSrc, T* realDest, int num) {
			cplxSrc++;
	
			while (num--) {
				*realDest++ = *cplxSrc;
				cplxSrc += 2;
			}
		}
	
		void complexMultiplyAdd (
			const T* src1, const T* src2, T* dest, int num) {
			while (num--) {
				T r1 = *src1++;
				T r2 = *src2++;
				T i1 = *src1++;
				T i2 = *src2++;
	
				*dest++ += r1 * r2 - i1 * i2;
				*dest++ += r1 * i2 + r2 * i1;
			}
		}
	
		void complexMultiplyReplace (
			const T* src1, const T* src2, T* dest, int num) {
			while (num--) {
				T r1 = *src1++;
				T r2 = *src2++;
				T i1 = *src1++;
				T i2 = *src2++;
	
				*dest++ = r1*r2 - i1*i2;
				*dest++ = r1*i2 + r2*i1;
			}
		}
	
		void norm (T* buf, int num) {
			while (num--) {
				*buf++ *= m_factor;
			}
		}
	protected:
		int m_bsize;		// block size (samples)
		int m_nblock;		// number of blocks
		int m_cfftSize;	    // complex FFT size: 2*2*m_bsize
		int m_currBlock;	// current block number
		T** m_impFft;	    // impulse FFT (size is m_cfftSize)
		T** m_accFft;	    // accumulator FFT (size is m_cfftSize)
		T* m_lastIn;   	    // size is m_bsize
		T* m_inFft;		    // input FFT buffer (size is m_cfftSize)
		T* m_outFft;	    // output FFT buffer (size is m_cfftSize)
		T m_factor;
		
		AbstractFFT<T>* m_fft;
	};
}

#endif

// EOF

