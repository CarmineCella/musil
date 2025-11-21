// BlockVocoder.h
// 

#ifndef BLOCKVOCODER_H
#define BLOCKVOCODER_H 

//#define NO_PEAKS_ESTIMATION
//#define USE_SLOW_FFT

#include "constants.h"
#include "FFT.h"

#include <iostream>
#include <complex>
#include <fstream>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cmath>

namespace soundmath {
	static const double COMPENSATION_EXPONENT = 15;
	
	template <typename T>
	//! Advanced phase-vocoder with formants preservation and phase-sync processing.
	/*!
	  The input of the process function is a workspace ready for FFT; the input is wksp while
	  the output is wksp2; note the the input is also transformed during processing.
	  The data in wksp MUST be windowed correctly and the output wksp2 must be overlapp-added.
	 */
	class BlockVocoder {
	private:
		BlockVocoder& operator= (BlockVocoder&);
		BlockVocoder (const BlockVocoder&);
	public: 
		BlockVocoder (int fftSize) {
			m_N  = fftSize;
			m_NN = m_N << 1;
			m_N2 = m_N >> 1;		
			m_sicvt = TWOPI / (double) m_N;
	
			m_phiMod = 0;
			m_crossMod = 0;
			
			// FFT
			m_mfft = createFFT<T> (m_N);
		
			// working buffers
			m_peaks = new int[m_N];
			memset (m_peaks, 0, sizeof (int) * m_N);
	
			// phases
			m_psi = new T[m_N];
			memset (m_psi, 0, sizeof (T) * m_N);
			m_phaseInc = new T[m_N];
			memset (m_phaseInc, 0, sizeof (T) * m_N);
			m_oldAnPhi = new T[m_N];
			memset (m_oldAnPhi, 0, sizeof (T) * m_N);
			m_oldSynPhi = new T[m_N];
			memset (m_oldSynPhi, 0, sizeof (T) * m_N);
			m_tmpBuff1 = new T[m_NN];
			memset (m_tmpBuff1, 0, sizeof (T) * m_NN);
			m_tmpBuff2 = new T[m_NN];
			memset (m_tmpBuff2, 0, sizeof (T) * m_NN);
			m_tmpBuff3 = new T[m_NN];
			memset (m_tmpBuff3, 0, sizeof (T) * m_NN);
		}
		virtual ~BlockVocoder () {
			delete [] m_peaks;
	
			delete [] m_psi;
			delete [] m_phaseInc;
			delete [] m_oldAnPhi;
			delete [] m_oldSynPhi;
			
	
			delete [] m_tmpBuff1;
			delete [] m_tmpBuff2;
			delete [] m_tmpBuff3;
	
			delete m_mfft;
		}
	
		void phaseMode (int phiMod) {
			m_phiMod = phiMod;
		}
		void crossMode (int crossMod) {
			m_crossMod = crossMod;
		}
		void process (T* wksp, T* wksp2, T P, T F, T D, T I, int C, T threshold, T cross) {
			fftshift<T> (wksp, m_NN);	
	
			transform (wksp);
			rect2pol<T> (wksp, m_N);
	
			// -------------------------------- CROSS SYNTHESIS/MORPHING ---------------------------
			switch (m_crossMod) {
			case 1:
				fftshift<T> (wksp2, m_NN);	
	
				transform (wksp2);
				rect2pol<T> (wksp2, m_N);
				for (int i = 0; i < m_N; ++i) {
					// method 0: Amp(SO) =  Amp(S1) + sqrt (cross* Amp(S1) * Amp(S2)) Pha(SO) = Pha(S2)
					//wksp[2 * i] = (1. - cross) * wksp[2 * i] + cross * sqrt (wksp[2 * i] * wksp2[2 * i]);
					//wksp[2 * i] = wksp[2 * i] + sqrt (cross * wksp[2 * i] * wksp2[2 * i]);
					//wksp[2 * i + 1] = wksp2[2 * i + 1];
	
					// method 1: Amp(SO) = sqrt (Amp(S1)*Amp(S2)) Pha(SO) =  (1 - cross) * Pha(S1)+ cross* Pha(S2)
					wksp[2 * i] = sqrt (wksp[2 * i] * wksp2[2 * i]);
					wksp[2 * i + 1] = (1. - cross) * wksp[2 * i + 1] + cross * wksp2[2 * i + 1];
	
					//method 2: Amp(SO) = Amp(S1)*Amp(S2) Pha(SO) = Pha(S1)
					//wksp[2 * i] = wksp[2 * i] * wksp2[2 * i];
					//wksp[2 * i + 1] = wksp[2 * i + 1];
	
					//method 3: Amp(SO) = Amp(S1) Pha(SO) = Pha(S1) + Pha(S2)
					//wksp[2 * i] = wksp[2 * i];
					//wksp[2 * i + 1] =  wksp[2 * i + 1] + wksp2[2 * i + 1];
				}
			break;
			case 2:
				fftshift<T> (wksp2, m_NN);	
	
				transform (wksp2);
				rect2pol<T> (wksp2, m_N);
	
				// computes both envelopes
				for (int i = 0; i < m_N; ++i) {
					m_tmpBuff1[2 * i] = log ((T) (wksp[2 * i] / m_NN) + EPS);
					m_tmpBuff1[2 * i + 1] = 0;
				}
				cepstralEnvelope (C, m_tmpBuff1, m_tmpBuff2);
				for (int i = 0; i < m_N; ++i) {
					m_tmpBuff1[2 * i] = log ((T) (wksp2[2 * i] / m_NN) + EPS);
					m_tmpBuff1[2 * i + 1] = 0;
				}
				cepstralEnvelope (C, m_tmpBuff1, m_tmpBuff3);
	
				for (int i = 0; i < m_N; ++i) {
					// method 4: using envelopes
					wksp[2 * i] = (1. - cross) * wksp[2 * i] + cross * wksp[2 * i] *
						::exp (2. * (m_tmpBuff3[2 *i] - m_tmpBuff2[2 * i]));
				}
			break;
			case 3: // morphing
				fftshift<T> (wksp2, m_NN);	

				transform (wksp2);
				rect2pol<T> (wksp2, m_N);       
				morph (wksp, wksp2, m_tmpBuff1, (1. - cross), (1. - cross), m_N);
				for (int i = 0; i < m_N; ++i) {    
					wksp[2 * i] = m_tmpBuff1[2 * i];
					wksp[2 * i + 1] = m_tmpBuff1[2 * i + 1];        
				}
				break;
			} 
	
			
	#ifdef NO_PEAKS_ESTIMATION
			double stretch = I / D;
			double omega = 0, phase = 0, delta = 0;
			for (int i = 0; i < m_N; ++i) {
				omega = m_sicvt * (double) i * D;
				phase = wksp[2 * i + 1];
				delta = omega + princarg (phase - m_oldAnPhi[i] - omega);
				m_oldAnPhi[i] = phase;
				m_psi[i] = princarg (m_psi[i] + delta * stretch * P);
				wksp[2 * i + 1] = m_psi[i];
			}
	#else
			// -------------------------------- PHASE SYNCING ---------------------------
			// phase increment computation
			T omega = 0, delta = 0;
			for (int i = 0; i < m_N2; ++i) {
				// T f = parabolic<T> (wksp, i, m_N);
				omega = m_sicvt * (double) i * D;
				delta = princarg<T> (wksp[2 * i + 1] - m_oldAnPhi[i] - omega);
				m_phaseInc[i] = P * (omega + delta) / D;
			}
	
			// peak picking
			int peaksNum = locmax2 (wksp, m_N2, m_peaks);
			if (peaksNum == 0) ++peaksNum;
	
			memset (m_psi, 0, sizeof (T) * m_N);
			for (int i = 0; i < peaksNum; ++i) {
				m_psi[m_peaks[i]] = m_oldSynPhi[m_peaks[i]] + (T) I * m_phaseInc[m_peaks[i]];		
			}
	
			// phase locking
			int start = 0;
			for (int i = 0; i < peaksNum - 1; ++i) {
				int pk = m_peaks[i];
				int next_pk = m_peaks[i + 1];
				int end = (int) round ((T)(pk + next_pk) * .5);
				T angRotation = m_psi[m_peaks[i]] - wksp[2 * m_peaks[i] + 1];
				for (int j = start; j <= end; ++j) {
					if (j != pk) {
						m_psi[j] = angRotation + wksp[2 * j + 1];
					}
				}
				start = end + 1;
			}
	
			// backup current phases for next round
			for (int i =  0; i < m_N2; ++i) {
				m_oldAnPhi[i] = wksp[2 * i + 1];
				m_oldSynPhi[i] = m_psi[i];
			}
	#endif
	
			// -------------------------------- PITCH-SHIFT / ENVELOPE PRESERVATION ---------------------------
			if ((C && P) || (C && F))  {   
				// compute inverse transposition in wksp2
				resample (wksp, wksp2, (1. / P) * F);
				
				// compute cepstrum on the correction
				for (int i = 0; i < m_N; ++i) {
					// NB: the division with m_NN is only for NUMERICAL PROBLEMS since the result
					// should be the same with or without the division (log (a / n) - log (b / n) == log (a) - log (b))
					wksp2[2 * i]  = (log ((T) (wksp2[2 * i] / m_NN) + EPS) -  log ((T) (wksp[2 * i] / m_NN) + EPS));	
					// wksp2[2 * i] = log (EPS + wksp2[2 * i] / (wksp[2 * i] + EPS));
					wksp2[2 * i + 1] = 0.;
				}
	
				// outputs: wksp has the corrected spectrum, wksp2 has the envelope
				cepstralEnvelope (C, wksp2, m_tmpBuff1);
	
				T norm1 = (P >= 1 ? 1 :  1. / pow (P, COMPENSATION_EXPONENT));
				T norm2 = (F <= 1 ? 1 :  1. / pow (1. / F, COMPENSATION_EXPONENT));
	
				// apply correction and amplitude compensation (experimental)
				for (int i = 0; i < m_N; ++i) {
					wksp[2 * i] *= (T) std::exp ((T) (2. * m_tmpBuff1[2 * i])) * norm1 * norm2;
				}
			} 
	
			resample (wksp, wksp2, P, threshold);
			
			// -------------------------------- PHASE-BASED EFFECTS ---------------------------
			// NB:  robotization only works if olap is very high, whisperization only
			//      if window len is small
			switch (m_phiMod) {
				case 1:
					for (int i = 0; i < m_N; ++i) {
						wksp2[2 * i + 1] = (T) 0;
					}
					break;
				case 2:
					for (int i = 0; i < m_N; ++i) {
						wksp2[2 * i + 1] = (T) (rand () % m_N); /// RAND_MAX * TWOPI;
					}
					break;
			}
			
			// synthesis
			pol2rect<T> (wksp2, m_N);
			itransform (wksp2);
			fftshift<T> (wksp2, m_NN); 
		}
	
		void reset () {
			memset (m_oldSynPhi, 0, sizeof (T) * m_N);
			memset (m_oldAnPhi, 0, sizeof (T) * m_N);
	}
	
	private:
		void transform (T* wksp) {
		#ifdef USE_SLOW_FFT	
			fft<T> (wksp, m_N, -1);
		#else
			m_mfft->forward (wksp);
		#endif
					
		}
		void itransform (T* wksp) {
		#ifdef USE_SLOW_FFT
			fft<T> (wksp, m_N, 1);
		#else
			m_mfft->inverse (wksp);
		#endif
		}
		void resample (const T* wksp, T* wksp2, T ratio, T threshold = 0) {
			// frequency domain resampling
			memset (wksp2, 0, sizeof (T) * m_NN);
			int k = 1;
			int pos = (int) ((T) k * ratio);
			while (pos < m_N2 && k < m_N * ratio) {
				//T a = wksp[2 * k];
				wksp2[2 * pos] = wksp[2 * k] <= threshold ? 0 : wksp[2 * k]; // denoise
				//wksp2[2 * pos] = a * a / (a + threshold); // denoise
				wksp2[2 * pos + 1] = m_psi[k];
				++k;
				pos = (int) ((T) k * ratio);
			}
			for (int i = 0; i < m_N2; i++) {
				int pos = (2 * (m_N - i)) - 2;
				wksp2[pos] = wksp2[2 * i];
				wksp2[pos + 1] = -wksp2[2 * i + 1];
			}
		}
		void cepstralEnvelope (int C, const T* wksp, T* wksp2) {
			memset (wksp2, 0, sizeof (T) * m_NN);
			for (int i = 0; i < m_N; ++i) {
				wksp2[2 * i] = wksp[2 * i];
				wksp2[2 * i + 1] = wksp[2 * i + 1];
			}
			// inverse transform

			itransform (wksp2);
			for (int i = 0; i < m_N; ++i) {
				wksp2[2 * i] /= m_N;
				wksp2[2 * i + 1] = 0;
			}
		
			// liftering
			wksp2[0] *= .5;
			for (int i = C + 1; i < m_N; ++i) {
				wksp2[2 * i] = 0;
			}
	
	
			// spectral envelope
			transform (wksp2);
		}
	
		void morph (T* input1, T* input2, T* output, 
					T morpha, T morphfr, int fftsize) {
			
			T amp1, amp2, fr1, fr2;
			T div;
			
			for (int i = 0; i < fftsize; i += 2){
				amp1 = input1[i];
				amp2 = input2[i];
				output[i] = amp1 + (amp2 - amp1) * morpha;
			
				if (i) {
					// interpolate frs
					fr1 = input1[i + 1];
					fr2 = input2[i + 1];
					div = fr1 ? fr2 / fr1 : 0;
					div = div > 0 ? div : -div;
					output[i + 1] = (T)(fr1 * pow (div, morphfr));
				} else {
					// this is the nyquist frequency band
					amp1 = input1[i + 1];
					amp2 = input2[i + 1];
					output[i + 1] = amp1 + (amp2 - amp1) * morpha;
				}
			}
		}
	
	
		int* m_peaks;
	
		T* m_psi;
		T* m_phaseInc;
		T* m_oldAnPhi;
		T* m_oldSynPhi;
		T* m_tmpBuff1;
		T* m_tmpBuff2;
		T* m_tmpBuff3;
		
		AbstractFFT<T>* m_mfft;
		
		T m_sr;
		int m_N;
		int m_NN;
		int m_N2;
		T m_sicvt;
	
		int m_phiMod;
		int m_crossMod;
	};
}

#endif	// BLOCKVOCODER_H 

// EOF
