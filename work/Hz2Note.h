// Hz2Note.h
// 

#ifndef HZ2NOTE_H
#define HZ2NOTE_H 

#include "algorithms.h"
#include <cmath>

namespace soundmath {
    extern const double D_NOTE = 1.059463094359; 		// pow (2.0, 1.0 / 12.0) == 100 cents == 1 half-tone
    extern const double LOG_D_NOTE = .057762265047; 	// log (pow (2.0, 1.0 / 12.0))
    extern const double D_NOTE_SQRT = 1.029302236643; 	// pow(2.0,1.0/24.0) == 50 cents
    
    //! Basic conversion between frequencies and tempered notes
    template <typename T>
    class Hz2Note {
    private:
        Hz2Note& operator= (Hz2Note&);
        Hz2Note (const Hz2Note&);
    public: 
        Hz2Note () {
            m_freqs[0] = 440.;
            m_lfreqs[0] = log (m_freqs[0]);
            for (int i = 1; i < 12; ++i) {
                m_freqs[i] = m_freqs[i - 1] * D_NOTE;
                m_lfreqs[i] = m_lfreqs[i - 1] + LOG_D_NOTE;
            }
        }
        void convert (T freq, T& nfreq, int& oct, int& note, int& cents) {
            T ldf, mldf;
            T lfreq;
        
            note = 0;
            oct = 4;
            if (freq < 1E-15) freq = 1E-15;
            lfreq = log (freq);
            while (lfreq < m_lfreqs[0] - LOG_D_NOTE / 2.) lfreq += LOG2;
            while (lfreq >= m_lfreqs[0] + LOG2 - LOG_D_NOTE / 2.) lfreq -= LOG2;
            mldf = LOG_D_NOTE;
            for (int i = 0; i < 12; ++i) {
                ldf = fabs (lfreq - m_lfreqs[i]);
                if (ldf < mldf) {
                    mldf = ldf;
                    note = i;
                }
            }
            nfreq = m_freqs[note];
            while (nfreq / freq > D_NOTE_SQRT) {
                nfreq /= 2.0;
                --oct;
            }
            while (freq / nfreq > D_NOTE_SQRT) {
                nfreq *= 2.0;
                ++oct;
            }
        
            cents =  1200 * (log (freq / nfreq) / LOG2);
        }
                
    private:
        T m_freqs[12];
        T m_lfreqs[12];
    };
}

#endif	// HZ2NOTE_H 

// EOF
