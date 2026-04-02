// features.h
// 

// short term:	spectral centroid*, spectral spread*, spectral skewness*, spectral kurtosis*
// 				spectral flux*, spectral irregularity*, spectral decrease*,  spectral slope*
// 				high frequency content*, total energy*, zero-crossings rate*, f0*

#ifndef FEATURES_H
#define FEATURES_H 

#include "FFT.h"
#include <cmath>
#include <cstring>

// spectral descriptors ------------------------------------------------- //
template <typename T>
inline T moment(
		const T* weights,
		const T* values,
		int N,
		int order,
		T centroid) {
	T sumWeigth = 0;
	T sumWeigthDistance = 0;
	T distance = 0;
	T weigth = 0;
	T weigthDistance = 0;

	for (int index = 0; index < N; ++index) {
		distance = values[index];
		distance -= centroid;
		distance = std::pow(distance, order);
		weigth = weights[index];
		weigthDistance = weigth * distance;
		sumWeigth += weigth;
		sumWeigthDistance += weigthDistance;
	}

	return (sumWeigth != 0 ? sumWeigthDistance / sumWeigth : 0);
}
template <typename T>
inline T speccentr (
	const T* amplitudes,
	const T* frequencies, 
	int N) {
	return moment<T> (amplitudes, frequencies, N, 1, 0);
}

template <typename T>
inline T specspread (
	const T* amplitudes,
	const T* frequencies, 
	T centroid,		
	int N) { 
	return std::sqrt (moment<T> (amplitudes, frequencies, N, 2, centroid)); 
}

template <typename T>
inline T specskew (
	const T* amplitudes,
	const T* frequencies, 
	T centroid, 
	T spread,
	int N) { 

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
	T centroid, 
	T spread,
	int N) { 

	T delta = std::pow (spread, 4);
	T tmp = moment<T>(amplitudes, frequencies, N, 4, centroid);
	if (delta != 0) {
		tmp /= delta;
	}	
	return tmp;
}

template <typename T>
inline T specflux (T* amplitudes, T* oldAmplitudes, int N) {
	T sf = 0;
	T a = 0;
	for (int i = 0; i < N; ++i) {
		a = (amplitudes[i] - oldAmplitudes[i]);
		oldAmplitudes[i] = amplitudes[i];
		sf += a < 0 ? 0 : a; // rectification
	}
	
	return sf;
}
	
template <typename T>
inline T specirr (T* amplitudes, int N) { 
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
	if (result[j] == 0) j = 0;
	else if ((result[j] / result[0]) < .1) j = 0;
	else if (j > size / 4) j = 0;		

	T f0 = j > 0 ? sr / j : 0;
	return (T) f0;
}

// temporal descriptors ------------------------------------------------- //	
template <typename T>
inline T energy (const T* samples, int N) {
	T sum = 0.;
	for (int index = 0; index < N; ++index) {
		T a = samples[index];
		sum += a * a;
	}
	
	return std::sqrt (sum / N);
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
	
#endif	// FEATURES_H

// EOF
