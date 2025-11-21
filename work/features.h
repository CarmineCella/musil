// features.h
// 

// short term:	spectral centroid*, spectral spread*, spectral skewness*, spectral kurtosis*
// 				spectral flux*, spectral irregularity*, spectral decrease*,  spectral slope*
// 				high frequency content*, total energy*, zero-crossings rate*, f0*

#ifndef FEATURES_H
#define FEATURES_H 

#include "FFT.h"
#include "algorithms.h"
#include <cmath>
#include <cstring>

namespace nanoSound { 
	typedef struct {
		double freq;
		double amp;
	} Peak;
	
// spectral descriptors ------------------------------------------------- //
	
	template <typename T>
	inline T speccentr (
		const T* amplitudes,
		const T* frequencies, 
		int N) {
		return centroid (amplitudes, frequencies, N);
	}
	
	template <typename T>
	inline T specspread (
		const T* amplitudes,
		const T* frequencies, 
		int N,
		T centroid) { 
		return std::sqrt (moment<T> (amplitudes, frequencies, N, 2, centroid)); 
	}
	
	template <typename T>
	inline T specskew (
		const T* amplitudes,
		const T* frequencies, 
		int N,
		T centroid, 
		T spread) { 
	
		T delta = std::pow (spread, 3);
		T tmp = moment<T>(amplitudes, frequencies, N, 3, centroid);
		if (delta != 0) {
			tmp /= delta;
		}	
		
		return tmp;
	}
	
	template <typename T>
	inline T speckurt (
		const T* amplitudes,
		const T* frequencies, 
		int N,
		T centroid, 
		T spread) { 
	
		T delta = std::pow (spread, 4);
		T tmp = moment<T>(amplitudes, frequencies, N, 4, centroid);
		if (delta != 0) {
			tmp /= delta;
		}	
		
		return tmp;
	}
	
	template <typename T>
	inline T specflux (T* amplitudes, T* oldAmplitudes, int N) {
		T sf = 0; // spectral flux
		T a = 0;
		for (int i = 0; i < N; ++i) {
			a = (amplitudes[i] - oldAmplitudes[i]);
			oldAmplitudes[i] = amplitudes[i];
			sf += a < 0 ? 0 : a; // rectification
			//sf += a;
		}
		
		return sf;
	}

	template <typename T>
	inline T specflux2 (T* amplitudes, T* oldAmplitudes, int N) {
		T sf = 0; // spectral flux
		T a = 0;
		for (int i = 0; i < N; ++i) {
			a = i * i * (amplitudes[i] - oldAmplitudes[i]);
			oldAmplitudes[i] = amplitudes[i];
			sf += a < 0 ? 0 : a; // rectification
			//sf += a;
		}
		
		return sf;
	}
		
	template <typename T>
	inline T specirr (double* amplitudes, int N) { 
		if (1 > N) return 0;
		T si = 0; // spectral irregularity
		T a = 0;
		for (int i = 1; i < N; ++i) {
			a = fabs (amplitudes[i] - amplitudes[i - 1]);
			si += a;
		}
		
		return si;
	}
	
	template <typename T>
	inline T specdecr (
		const T* amplitudes,
		int N) { 
		T decs = 0;
		T den = 0;
		if (N <= 1) return 0;
		
		for (int index = 1; index < N; ++index) {
			decs += (amplitudes[index] - amplitudes[0])	/ (T) (index);
			den  += amplitudes[index];
		}
	
		if (den != 0.0) {
			decs /= den;
		}
		
		return decs;
	}
	
	template <typename T>
	inline T specslope (
		const T* amplitudes,
		const T* frequencies,
		int N) { 
		double step = 0;
		double sum = 0;
		for (int i = 0; i < N; ++i) {
			sum += amplitudes[i];
		}
		
		T sl = linreg (amplitudes, frequencies, N, step);
	
		if (sum != 0.0) {
			sl /= sum;
		}
	
		return sl;
	}
	
	template <typename T>
	inline T hfc (double* amplitudes, int N) {
		double hfc = 0; // high frequency content
		double a = 0;
		double n = 0;
		for (int i = 0; i < N; ++i) {
			a = amplitudes[i];
			hfc += (a * a) * i;
			n += i;
		}
		
		return hfc / n;
	}
	
	template <typename T>
	double inharmonicity (T* amp, T* freq, int size, T f0, T R, double& sumAmpl) {
		if (f0 <= 0) return 0;

		sumAmpl = 0.0;
		T sumVariation = 0.0;
		for (int i = 0; i < size / 2; ++i) {
			T harmo       = (i + 1) * f0;
			T variation   = std::fabs (freq[i] - harmo);
			sumAmpl += amp[i];
			sumVariation += variation * amp[i];
		}
	   
		T estInharm = 0.;				
		if (sumAmpl != 0) {
			estInharm =  (2 / f0) * (sumVariation / sumAmpl) / R;
		}
		
		return estInharm;
	}
 
	template <typename T>
	T fftF0Estimate (const T* amp, const T* freq, int size) {
		static const int MAXPEAKS = 8;
		int maxima2[size];
		int size2 = size >> 1;
		int numPeaks = locmax2 (amp, size2, maxima2);
		if (numPeaks == 0) return 0;
										
		Peak peaks[MAXPEAKS];
		for (int i = 0; i < MAXPEAKS; ++i) {
			peaks[i].freq = 0.;
			peaks[i].amp = 0;
		}
		
		// estimate fundamental frequency
		for (int i = 0; i < numPeaks; ++i) {
			T magnitude = (amp[maxima2[i]] / size);
			T rfreq = freq[maxima2[i]];
			if (rfreq > 0.0 && magnitude > peaks[0].amp) {
				memmove (peaks + 1, peaks, sizeof (Peak) * (MAXPEAKS - 1));
				peaks[0].freq = rfreq;
				peaks[0].amp = magnitude;
			}
		}
	
		int l;
		int maxharm = 0;
		int kpos = 0;
		for (l = 1; l < MAXPEAKS && peaks[l].freq > 0.0; l++) {
			int harmonic;
		
			for (harmonic = 5; harmonic > 1; harmonic--) {
				T ratio = peaks[0].freq / peaks[l].freq;
				if (ratio < harmonic + .02 &&
					ratio > harmonic - .02) {
					if (harmonic > maxharm &&
						peaks[0].amp < (peaks[l].amp * .5)) {
						maxharm = harmonic;
						kpos = l;
					}
				}
			}
		}
		
		return peaks[kpos].freq;	
	}

	template <typename T>
	T cepF0Estimate (T* amp, const T* freq, int size, T R) {
		static const int MAXPEAKS = 8;
		int maxima2[size];
		int size2 = size >> 1;
		int numPeaks = locmax2 (amp, size2, maxima2);
		if (numPeaks == 0) return 0;
									
		Peak peaks[MAXPEAKS];
		for (int i = 0; i < MAXPEAKS; ++i) {
			peaks[i].freq = 0.;
			peaks[i].amp = 0.;		
		}

		T mixed[2 * size];
		for (int i = 0; i < size; ++i) {
			mixed[i * 2] = amp[i];
			mixed[i * 2 + 1] = 0.;
		}

		fft (mixed, size, 1);	
		T cepmax = 0;
		int kpos = 0;
		for (int i = 4; i < size2; ++i) {
			amp[i] = 2. * sqrt (mixed[2 * i] * mixed[2 * i]
								+ mixed[2 * i + 1] * mixed[2 * i + 1]);      
			if (cepmax < amp[i]) {
				cepmax = amp[i];
				kpos = i;	
			}
		}	
		std::cout << kpos << std::endl;
		return R / (T) kpos;	
	}

	template <typename T>
	T acfF0Estimate (T sr, const T* data, T* result, int size) {
		long i, j, k;
		T temp, norm;
		
		int size2 = size >> 1;
	
		for (i = 0; i < size2; i++) {
			result[i] = 0.0;
			for (j = 0; j < size - i - 1; j++) {
				result[i] += data[i+j] * data[j];
			}
		}
		temp = result[0];
		j = (long) size * .02;
		while (result[j] < temp && j < size2) {
			temp = result[j];
			j += 1;
		}
		temp = 0.;
		for (i = j; i < size2; i++) {
			if (result[i]>temp) {
				j = i;
				temp = result[i];
			}
		}
		norm = 1. / size;
		k = size2;
		for (i = 0; i < size2; i++) {
			result[i] *=  (k - i) * norm;
		}
		std::cout << j << " " << sr << std::endl;
		if (result[j] == 0) j = 0;
		else if ((result[j] / result[0]) < .1) j = 0;
		else if (j > size / 4) j = 0;		

		T f0 = j > 0 ? sr / j : 0;
		return (T) f0;
	}

// temporal descriptors ------------------------------------------------- //
	
	template <typename T>
	inline T energy (const T* samples, int N, T winEnergy) {
		T sum = 0.;
		T en = 0.;
		T a = 0.;
		for (int index = 0; index < N; ++index) {
			a = samples[index];
			sum += a * a;
		}
	
		if (winEnergy > 0) {
			en = std::sqrt (sum / winEnergy);
		}
		
		return en;
	}
	
	template <typename T>
	static inline T zcr (const T* samples, int N) {
		T nb_zcr = 0.0;
		int sign1, sign2;
	
		if (1 > N) return 0;
	
		sign1 = (samples[0] < 0 ? -1 : 1);
		if (samples[0] == 0) sign1 = 0;
	
		for (int index = 1; index < N; ++index) {
			sign2 = (samples[index] < 0 ? -1 : 1);
			if (samples[index] == 0) sign2 = 0;
	
			if (sign1 != sign2) {
				++nb_zcr;
			}
	
			sign1 = sign2;
		}
	
		return (T)(nb_zcr / N);
	}
}	
#endif	// FEATURES_H

// EOF
