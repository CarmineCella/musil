// DifferentialEstimator.h
// 

#ifndef DIFFERENTIALESTIMATOR_H
#define DIFFERENTIALESTIMATOR_H 

#include "AbstractEstimator.h"
#include "OnePole.h"
#include "EnvelopeFollower.h"
#include "algorithms.h"
#include <stdexcept>
#include <cstring>

	
namespace soundmath {
	// comment/uncomment this to disable/enable median filtering 
	// to the given order (or change the order ;) )
	#define MEDIAN 3

	// comment/uncomment this to disable/enable moving average 
	// to the given length (or change the length ;) )	
	#define WEIGHTED_MOVING_AVERAGE 3

	template <typename T>
	//! Fundamental frequency estimator based on average magnitude difference function: for each buffer returns a single f0
	class DifferentialEstimator : public AbstractEstimator {
	public:
		DifferentialEstimator (T sr, int buffer, T analysisFreq, T min, T max,
							   T threshold, int order) {
			m_sr = sr;
			m_buffer = buffer;
			m_min = min;
			m_max = max;
			if (m_max < m_min) {
				throw std::runtime_error ("invalid range specified");
			}
			m_mid = (m_max - m_min) * .5; //- m_min);
			m_threshold = threshold;

			m_tmp1 = new T[m_buffer];

			m_maxFreq = analysisFreq;
 			m_bandLimiter = new OnePole<T> (m_sr, m_maxFreq, order, true);
    	    m_follower = new EnvelopeFollower<T> ();
		}
		
		virtual ~DifferentialEstimator () {	   
			delete m_bandLimiter;
 			delete m_follower;
		 	   
			delete [] m_tmp1;
		}

		void threshold (T t) { m_threshold = t; }
		void setAnalysisFreq (T analysisFreq) {
			m_maxFreq = analysisFreq;
			reset ();
		}			
		void reset () {
			m_old = m_mid;
			memset (m_tmp1, 0, sizeof (T) * m_buffer);			
 			m_bandLimiter->cutoff (m_maxFreq);
			m_follower->reset();
   
			m_avgPos = 0;
			
#ifdef WEIGHTED_MOVING_AVERAGE		
			for (int i = 0; i < WEIGHTED_MOVING_AVERAGE; ++i) {
				m_avgF[i] = m_avgA[i] = 0.;	 			
			}
#else
			m_avgF[m_avgPos] = m_avgA[m_avgPos] = 0.;	
#endif

			m_medianPos = 0;			
#ifdef MEDIAN
			for (int i = 0; i < MEDIAN; ++i) {
				m_val[i] = 0;	 			
			}
#else
			m_val[m_medianPos] = 0;
#endif
			m_old = m_mid;
		}
		T process (const T* in, T& amp) {			
			// pre-processing
   			//m_bandLimiter->process (in, m_tmp1, m_buffer);

			// amp following
			amp = m_follower->process (in, m_buffer);			
			if (amp > m_threshold) {
				const register T* ntmp3 = in; //m_tmp1;
				
				int s2 = m_buffer >> 1;
//				int s2 = m_buffer / 2;
				T prevDiff = 0;
				T prevDx = 0;
				T maxDiff = 0;
	
				int sampleLen = 0;
	
				for (int i = 0; i < s2; ++i) {
					T diff = 0;
					// compute average magnitude difference function
					for (int j = 0; j < s2; ++j) {
						diff += fabs (ntmp3[j] - ntmp3[i + j]);
					}

					T dx = prevDiff - diff;
	
					// look for change of sign in dx
					if (dx < 0 && prevDx > 0) {
						// locate minima
						if (diff < (0.1 * maxDiff)) { // && sampleLen  == 0) {
							sampleLen = i - 1;
							break; // found period
						}
					}
	
					prevDx = dx;
					prevDiff = diff;
					maxDiff = (diff < maxDiff) ? maxDiff : diff; 
//					maxDiff = std::max (diff, maxDiff);
				}
	
				if (sampleLen > 0) {
					T f = (m_sr / (T) sampleLen);
					if (f > m_min && f < m_max) {
						 m_val[m_medianPos] = f;
					} else return -1; //m_val[m_medianPos] = m_old;
				}		
			} else {
				return -1;
				//m_val[m_medianPos] = m_old;
			}
			
#ifdef MEDIAN
			// median filtering
			for (int i = 0; i < MEDIAN; ++i) {
				m_copy[i] = m_val[i];
			}
			T v = median (m_copy, MEDIAN);
			++m_medianPos;
			m_medianPos %= MEDIAN;			
#else			
			T v = m_val[m_medianPos]; 
#endif

#ifdef WEIGHTED_MOVING_AVERAGE
			T ratio = v / m_old;
			if (ratio < 0.94 || ratio > 1.059) {
				for (int i = 0; i < WEIGHTED_MOVING_AVERAGE; ++i) {
					m_avgA[i] = 0.;	
				}	
			}
			m_avgA[m_avgPos] = amp;
			m_avgF[m_avgPos] = v;
			++m_avgPos;
			m_avgPos %= WEIGHTED_MOVING_AVERAGE;
			m_old = centroid (m_avgA, m_avgF, WEIGHTED_MOVING_AVERAGE);	
#else
			m_old = v;
#endif

			return m_old;
		}	
	private:
        OnePole<T>* m_bandLimiter;
 	 	EnvelopeFollower<T>* m_follower;
		long m_buffer;
		T* m_tmp1;
		
		T m_maxFreq;
		T m_min;
		T m_max;
		T m_mid;
		T m_old;

		T m_sr;
		T m_threshold;
		
#ifdef MEDIAN		
		T m_val[MEDIAN];
		T m_copy[MEDIAN];
#else
		T m_val[1];
#endif
		int m_medianPos;
		
#ifdef WEIGHTED_MOVING_AVERAGE
		T m_avgF[WEIGHTED_MOVING_AVERAGE];
		T m_avgA[WEIGHTED_MOVING_AVERAGE];		
#else
		T m_avgF[1];
		T m_avgA[1];		
#endif
		int m_avgPos;
	};
}

#endif	// DIFFERENTIALESTIMATOR_H 

// EOF
