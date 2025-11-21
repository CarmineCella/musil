// MFCC.h
//

#ifndef MFCC_H
#define MFCC_H

#include <cmath>

namespace soundmath {
	template <typename T>
	class MFCC {
	private:
		MFCC& operator= (MFCC&);
		MFCC (const MFCC&);
	public:
		MFCC (T samplingRate, unsigned int numFilters, int fftSize) {
			m_samplingRate = samplingRate;
			m_numFilters = numFilters;
			m_fftSize = fftSize;
			
			// the matrix has one element more if both directions (safety, not used)
			m_dctMatrix = new T*[m_fftSize];
			for (int i = 0; i < m_fftSize; ++i) {
				m_dctMatrix[i] = new T[m_numFilters + 1];
				memset (m_dctMatrix[i], 0, sizeof (T) * (m_numFilters + 1));
			}
		
			for (unsigned int l = 1; l <= m_numFilters; ++l) {
				for (int k = 0; k < m_fftSize - 1; ++k) {
					m_dctMatrix[k][l] = getFilterParam (k, l);
				}
			}
	
			m_normFactor0 = sqrt (1.f / numFilters);
			m_normFactorM = sqrt (2.f / numFilters);
		}
		virtual ~MFCC () {
			for (int i = 0; i < m_fftSize; ++i) {
				delete [] m_dctMatrix[i];
			}
			delete [] m_dctMatrix;
		}
	
		T getCoeff (T* spectralData, unsigned int m) {
			T result = 0.0f;
			T outerSum = 0.0f;
			T innerSum = 0.0f;
	
			// 0 <= m < L
			if (m >= m_numFilters) {
				return 0.0f;
			}
	
			result = m == 0 ? m_normFactor0 : m_normFactorM;
	
			// FIXME: optimize for multiple coefficients request
			for (unsigned int l = 1; l <= m_numFilters; l++) {
				// Compute inner sum
				innerSum = 0.0f;
				for (int k = 0; k < m_fftSize - 1; k++) {
					innerSum += fabs (spectralData[k] * m_dctMatrix[k][l]);
				}
	
				if (innerSum > 0.f) {
					innerSum = log (innerSum); // The log of 0 is undefined, so don't use it
				}
	
				innerSum = innerSum * cos (((m * M_PI) / m_numFilters) * (l - 0.5f));
	
				outerSum += innerSum;
			}
	
			result *= outerSum;

			return result;
		}
	private:
		T getFilterParam (unsigned int frequencyBand, unsigned int filterBand) {
			T filterParameter = 0.0f;
	
			T boundary = (T) frequencyBand * m_samplingRate / m_fftSize;	// k * Fs / N
			T prevCenterFrequency = getCenterFreq (filterBand - 1);			// fc(l - 1) etc.
			T thisCenterFrequency = getCenterFreq (filterBand);
			T nextCenterFrequency = getCenterFreq (filterBand + 1);
				
			if (boundary >= 0 && boundary < prevCenterFrequency) {
				filterParameter = 0.0f;
			} else if (boundary >= prevCenterFrequency && boundary < thisCenterFrequency) {
				filterParameter = (boundary - prevCenterFrequency) / (thisCenterFrequency - prevCenterFrequency);
				filterParameter *= getMagnitudeFactor (filterBand);
			} else if (boundary >= thisCenterFrequency && boundary < nextCenterFrequency) {
				filterParameter = (boundary - nextCenterFrequency) / (thisCenterFrequency - nextCenterFrequency);
				filterParameter *= getMagnitudeFactor (filterBand);
			} else if (boundary >= nextCenterFrequency && boundary < m_samplingRate) {
				filterParameter = 0.0f;
			}

			return filterParameter;
		}
	
		T getMagnitudeFactor (unsigned int filterBand) {
			T magnitudeFactor = 0.0f;
	
			if (filterBand >= 1 && filterBand <= 14) {
				magnitudeFactor = 0.015;
			} else if (filterBand >= 15 && filterBand <= 48) {
				magnitudeFactor = 2.0f / (getCenterFreq (filterBand + 1) - getCenterFreq (filterBand - 1));
			}
	
			return magnitudeFactor;
		}
	
		T getCenterFreq (unsigned int filterBand) {
			T centerFrequency = 0.0f;
			T exponent;
	
			if (filterBand == 0) {
				centerFrequency = 0;
			} else if (filterBand >= 1 && filterBand <= 14) {
				centerFrequency = (200.0f * filterBand) / 3.0f;
			} else {
				exponent = filterBand - 14.0f;
				centerFrequency = pow (1.0711703, exponent);
				centerFrequency *= 1073.4;
			}
	
			return centerFrequency;
		}
		
		T** m_dctMatrix;
		T m_samplingRate;
		unsigned int m_numFilters;
		int m_fftSize;
        
        T m_normFactor0;
        T m_normFactorM;
	};
}

#endif	// MFCC_H

// EOF
