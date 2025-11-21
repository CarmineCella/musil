// Biquad.h
//

#ifndef BIQUAD_H
#define BIQUAD_H 

#include "AbstractFilter.h"
#include "filterType.h"
#include <cmath>

namespace soundmath {
	template <typename T>
	//! Two-poles two-zeros filter (biquad): for each buffer returns a buffer of same size
	class Biquad : public AbstractFilter<T> {
	public:
		Biquad () {
			m_b0a0 = m_b1a0 = m_b2a0 = m_a1a0 = m_a2a0 = 0.;
			m_out1 = m_out2 = m_in1 = m_in2 = 0.;
		};	
		
		Biquad (const double& sr, filterType type, const double& frequency,
			const double& q, const double& dbGain, bool qIsBandwidth) {

			m_b0a0 = m_b1a0 = m_b2a0 = m_a1a0 = m_a2a0 = 0.;
			m_out1 = m_out2 = m_in1 = m_in2 = 0.;
			
			reset (sr, type, frequency, q, dbGain, qIsBandwidth);
		};
	
		T* process (const T* in, T* out, int len) {

			for (int i = 0; i < len; ++i) {
				// filter
				out[i] = m_b0a0 * in[i] + m_b1a0 * m_in1 + m_b2a0 * m_in2 - m_a1a0 * m_out1 - m_a2a0 * m_out2;
		
				m_in2 = m_in1;
				m_in1 = in[i];
				m_out2 = m_out1;
				m_out1 = out[i];
			}
			return out;
		};
	
		void reset (const double& sr, filterType type, const double& frequency,
			const double& q, const double& dbGain, bool qIsBandwidth) {
			double const temp_pi=3.1415926535897932384626433832795;
			double alpha,a0,a1,a2,b0,b1,b2;
	
			// peaking, lowshelf and hishelf
			if (type >= PEAKING) {
				double const A		= pow (10.0, (dbGain / 40.0));
				double const omega	= 2. * temp_pi * frequency / sr;
				double const tsin	= sin (omega);
				double const tcos	= cos (omega);
	
				if (qIsBandwidth) {
					alpha = tsin * sinh (log (2.) / 2. * q * omega / tsin);
				}
				else {
					alpha = tsin / (2. * q);
				}
				double const beta = sqrt (A) / q;
	
				// peaking
				if (type == PEAKING) {
					b0 = T (1.0 + alpha * A);
					b1 = T (-2.0 * tcos);
					b2 = T (1.0 - alpha * A);
					a0 = T (1.0 + alpha / A);
					a1 = T (-2.0 * tcos);
					a2 = T (1.0 - alpha / A);
				}
	
				// lowshelf
				if (type == LOSHELF) {
					b0 = T (A*((A+1.0)-(A-1.0)*tcos+beta*tsin));
					b1 = T (2.0*A*((A-1.0)-(A+1.0)*tcos));
					b2 = T (A*((A+1.0)-(A-1.0)*tcos-beta*tsin));
					a0 = T ((A+1.0)+(A-1.0)*tcos+beta*tsin);
					a1 = T (-2.0*((A-1.0)+(A+1.0)*tcos));
					a2 = T ((A+1.0)+(A-1.0)*tcos-beta*tsin);
				}
	
				// hishelf
				if (type == HISHELF) {
					b0 = T (A*((A+1.0)+(A-1.0)*tcos+beta*tsin));
					b1 = T (-2.0*A*((A-1.0)+(A+1.0)*tcos));
					b2 = T (A*((A+1.0)+(A-1.0)*tcos-beta*tsin));
					a0 = T ((A+1.0)-(A-1.0)*tcos+beta*tsin);
					a1 = T (2.0*((A-1.0)-(A+1.0)*tcos));
					a2 = T ((A+1.0)-(A-1.0)*tcos-beta*tsin);
				}
			} else {
				// other filters
				double const omega = 2. * temp_pi * frequency / sr;
				double const tsin = sin (omega);
				double const tcos = cos (omega);
	
				if (qIsBandwidth) {
					alpha = tsin * sinh (log (2.) / 2. * q * omega / tsin);
				}
				else {
					alpha = tsin / (2. * q);
				}
	
	
				// lowpass
				if (type == LOPASS) {
					b0 = (1.0-tcos)/2.0;
					b1 = 1.0-tcos;
					b2 = (1.0-tcos)/2.0;
					a0 = 1.0+alpha;
					a1 = -2.0*tcos;
					a2 = 1.0-alpha;
				}
	
				// hipass
				if (type == HIPASS) {
					b0 = (1.0+tcos)/2.0;
					b1 = -(1.0+tcos);
					b2 = (1.0+tcos)/2.0;
					a0 = 1.0+ alpha;
					a1 = -2.0*tcos;
					a2 = 1.0-alpha;
				}
	
				// bandpass csg
				if (type == BANDPASS1) {
					b0 = tsin/2.0;
					b1 = 0.0;
					b2 = -tsin/2;
					a0 = 1.0+alpha;
					a1 = -2.0*tcos;
					a2 = 1.0-alpha;
				}
	
				// bandpass czpg
				if (type == BANDPASS2) {
					b0 = alpha;
					b1 = 0.0;
					b2 = -alpha;
					a0 = 1.0+alpha;
					a1 = -2.0*tcos;
					a2 = 1.0-alpha;
				}
	
				// notch
				if (type == NOTCH) {
					b0 = 1.0;
					b1 = -2.0*tcos;
					b2 = 1.0;
					a0 = 1.0+alpha;
					a1 = -2.0*tcos;
					a2 = 1.0-alpha;
				}
	
				// allpass
				if (type == ALLPASS) {
					b0 = 1.0-alpha;
					b1 = -2.0*tcos;
					b2 = 1.0+alpha;
					a0 = 1.0+alpha;
					a1 = -2.0*tcos;
					a2 = 1.0-alpha;
				}
			}
	
			// set filter coeffs
			m_b0a0 = T (b0/a0);
			m_b1a0 = T (b1/a0);
			m_b2a0 = T (b2/a0);
			m_a1a0 = T (a1/a0);
			m_a2a0 = T (a2/a0);
		};
	
	private:
		T m_b0a0, m_b1a0, m_b2a0, m_a1a0, m_a2a0;
		T m_out1, m_out2, m_in1, m_in2;
	};
}

#endif	// BIQUAD_H

// EOF

