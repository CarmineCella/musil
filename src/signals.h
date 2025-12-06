// signals.h
// 

#ifndef SIGNALS_H
#define SIGNALS_H 

#include "core.h"
#include "signals/FFT.h"
#include "signals/features.h"

#include <vector>
#include <valarray>
#include <dirent.h>

// support -----------------------------------------------------------------
void gen10 (const std::valarray<Real>& coeff, std::valarray<Real>& values) {
	for (unsigned i = 0; i < values.size () - 1; ++i) {
		values[i] = 0;
		for (unsigned j = 0; j < coeff.size (); ++j) {
			values[i] += coeff[j] * sin (2. * M_PI * (j + 1) * (Real) i / values.size ());
		}
		values[i] /= coeff.size ();
	}
	values[values.size () - 1] = values[0]; // guard point
}	
int next_pow2 (int n) {
	if (n == 0 || ceil(log2(n)) == floor(log2(n))) return n;
	int count = 0;
	while (n != 0) {
		n = n>>1;
		count = count + 1;
	}
	return (1 << count) ;
}
template <typename T>
void complex_mult_replace (
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
template <typename T>
void interleave (T* stereo, const T* l, const T* r, int n) {
	for (int i = 0; i < n; ++i) {
		stereo[2 * i] = l[i];
		stereo[2 * i + 1] = r[i];
	}
}
template <typename T>
void deinterleave (const T* stereo, T* l, T* r, int n) {
	for (int i = 0; i < n / 2; ++i) {
		l[i] = stereo[2 * i];
		r[i] = stereo[2 * i + 1];
	}
}	
std::valarray<Real> to_array (AtomPtr list) {
	int sz = list->tail.size ();
	std::valarray<Real> out (sz);
	for (unsigned i = 0; i < sz; ++i) out[i] = type_check (list->tail.at (i), ARRAY)->array[0];
	return out;
}
// signals --------------------------------------------------------------------------------------
AtomPtr fn_mix (AtomPtr node, AtomPtr env) {
	std::vector<Real> out;
	if (node->tail.size () % 2 != 0) error ("[mix] invalid number of arguments", node);
	for (unsigned i = 0; i < node->tail.size () / 2; ++i) {
		int p = (int) type_check (node->tail.at (i * 2), ARRAY)->array[0];
		AtomPtr l = type_check (node->tail.at (i * 2 + 1), ARRAY);
		int len = (int) (p + l->array.size ());
		if (len > out.size ()) out.resize (len, 0);
		// out[std::slice(p, len, 1)] += l->array;
		for (unsigned t = 0; t < l->array.size (); ++t) {
			out[t + p] += l->array[t];
		}
	}
	std::valarray<Real> v (out.data(), out.size());
	return make_atom (v);
}
AtomPtr fn_gen (AtomPtr node, AtomPtr env) {
	int len = (int) type_check (node->tail.at (0), ARRAY)->array[0];
	std::valarray<Real> coeffs (node->tail.size () - 1);
	for (unsigned i = 1; i < node->tail.size (); ++i) {
		coeffs[i - 1] = ((type_check (node->tail.at (i), ARRAY)->array[0]));
	}
	Real init = 0; 
	std::valarray<Real> table (init, len + 1); 
	gen10 (coeffs, table);
	return make_atom (table);
}
AtomPtr fn_osc (AtomPtr node, AtomPtr env) {
	Real sr = type_check (node->tail.at (0), ARRAY)->array[0];
	std::valarray<Real>& freqs = type_check (node->tail.at (1), ARRAY)->array;
	std::valarray<Real>& table = type_check (node->tail.at (2), ARRAY)->array;
	Real init = 0;
	std::valarray<Real> out (init, freqs.size ());
	int N = table.size () - 1;
	Real fn = (Real) sr / N; // Hz
	Real phi = 0; //rand () % (N - 1);
	for (unsigned i = 0; i < freqs.size (); ++i) {
		int intphi = (int) phi;
		Real fracphi = phi - intphi;
		Real c = (1 - fracphi) * table[intphi] + fracphi * table[intphi + 1];
		out[i] = c;
		phi = phi + freqs[i] / fn;
		if (phi >= N) phi = phi - N;
	}
	return make_atom (out);
}
AtomPtr fn_reson (AtomPtr node, AtomPtr env) {
	std::valarray<Real>& array = type_check (node->tail.at (0), ARRAY)->array;
	Real sr = type_check (node->tail.at (1), ARRAY)->array[0];
	Real freq = type_check (node->tail.at (2), ARRAY)->array[0];
	Real tau = type_check (node->tail.at (3), ARRAY)->array[0];
	
	Real om = 2 * M_PI * (freq / sr);
	Real B = 1. / tau;
	Real t = 1. / sr;
	Real radius = exp (-2. * M_PI * B * t);
	Real a1 = -2 * radius * cos (om);
	Real a2 = radius * radius;
	Real gain = radius * sin (om);

	int samps = (int) (sr * tau);
	std::valarray<Real> out (samps);
	int insize = array.size ();

	Real x1 = 0;
	Real y1 = 0;
	Real y2 = 0;
	for (unsigned i = 0; i < samps; ++i) {
		Real v = gain * x1 - (a1 * y1) - (a2 * y2);
		x1 = i < insize ? array[i] : 0;
		y2 = y1;
		y1 = v;
		out[i] = v;
	}
	return make_atom (out);
}
template <int sign>
AtomPtr fn_fft (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& sig = type_check (n->tail.at (0), ARRAY)->array;
	int d = sig.size ();
	int N = next_pow2 (d);
	int norm = (sign < 0 ? 1 : N / 2);
	std::valarray<Real> inout (N);
	for (unsigned i = 0; i < d; ++i) inout[i] = sig[i];
	fft<Real> (&inout[0], N / 2, sign);
	
	for (unsigned i = 0; i < N; ++i) inout[i] /= norm;	
	return make_atom (inout);
}
AtomPtr fn_car2pol (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& inout = type_check (n->tail.at (0), ARRAY)->array;
	rect2pol (&inout[0], inout.size () / 2);
	return make_atom (inout);
}
AtomPtr fn_pol2car (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& inout = type_check (n->tail.at (0), ARRAY)->array;
	pol2rect (&inout[0], inout.size () / 2);
	return make_atom (inout);
}
AtomPtr fn_conv (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& ir = type_check (n->tail.at (0), ARRAY)->array;
	std::valarray<Real>& sig = type_check (n->tail.at (1), ARRAY)->array;
	Real scale = type_check(n->tail.at (2), ARRAY)->array[0];
	Real mix = 0;
	if (n->tail.size () == 4) mix = type_check(n->tail.at (3), ARRAY)->array[0];
	long irsamps = ir.size ();
	long sigsamps = sig.size ();
	if (irsamps <= 0 || sigsamps <= 0) error ("[conv] invalid lengths", n);
	int max = irsamps > sigsamps ? irsamps : sigsamps;
	int N = next_pow2(max) << 1;
	std::valarray<Real> fbuffir(2 * N);
	std::valarray<Real> fbuffsig(2 * N);
	std::valarray<Real> fbuffconv(2 * N);
	for (unsigned i = 0; i < N; ++i) {
		if (i < irsamps) fbuffir[2 * i] = ir[i];
		else fbuffir[2 * i] = 0;
		if (i < sigsamps) fbuffsig[2 * i] = sig[i];
		else fbuffsig[2 * i] = 0;
		fbuffconv[2 * i] = 0;
		fbuffir[2 * i + 1] = 0;
		fbuffsig[2 * i + 1] = 0;
		fbuffconv[2 * i + 1] = 0;        
	}
	AbstractFFT<Real>* fft = createFFT<Real>(N);
	fft->forward(&fbuffir[0]);
	fft->forward(&fbuffsig[0]);
	complex_mult_replace (&fbuffir[0], &fbuffsig[0], &fbuffconv[0], N);
	fft->inverse(&fbuffconv[0]);
	std::valarray<Real> out  (irsamps + sigsamps - 1);
	for (unsigned i = 0; i < (irsamps + sigsamps) -1; ++i) {
		Real s = scale * fbuffconv[2 * i] / N;
		if (i < sigsamps) s+= sig[i] * mix;
		out[i] = s;
	}
	return make_atom (out);
}
AtomPtr fn_noise (AtomPtr n, AtomPtr env) {
	int len = (int) type_check (n->tail.at (0), ARRAY)->array[0];
	int rows = 1;
	if (n->tail.size () == 2) rows = (int) type_check (n->tail.at (1), ARRAY)->array[0];
	AtomPtr l = make_atom ();
	for (unsigned j = 0; j < rows; ++j) {
		std::valarray<Real> out (len);
		for (unsigned i = 0; i < len; ++i) out[i] = ((Real) rand () / RAND_MAX) * 2. - 1;
		l->tail.push_back (make_atom (out));
	}
	return l->tail.size () == 1 ? l->tail.at (0) : l;
}
AtomPtr fn_window (AtomPtr n, AtomPtr env) {
	int N = type_check (n->tail.at (0), ARRAY)->array[0];
	Real a0 =  type_check (n->tail.at (1), ARRAY)->array[0];
	Real a1 =  type_check (n->tail.at (2), ARRAY)->array[0];
	Real a2 =  type_check (n->tail.at (3), ARRAY)->array[0];

	std::valarray<Real> win (N);
	make_window (&win[0], N, a0, a1, a2);
	return make_atom (win);
}
AtomPtr fn_speccent (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& amps = type_check (n->tail.at (0), ARRAY)->array;
	std::valarray<Real>& freqs = type_check (n->tail.at (1), ARRAY)->array;
	Real c = speccentr<Real>  (&amps[0], &freqs[0], amps.size ());
	return make_atom (c);
}	
AtomPtr fn_specspread (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& amps = type_check (n->tail.at (0), ARRAY)->array;
	std::valarray<Real>& freqs = type_check (n->tail.at (1), ARRAY)->array;
	Real centr = type_check (n->tail.at (2), ARRAY)->array[0];
	Real c = specspread<Real>  (&amps[0], &freqs[0], centr, amps.size ());
	return make_atom (c);
}		
template <int MODE>
AtomPtr fn_specsk_kur (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& amps = type_check (n->tail.at (0), ARRAY)->array;
	std::valarray<Real>& freqs = type_check (n->tail.at (1), ARRAY)->array;
	Real centr = type_check (n->tail.at (2), ARRAY)->array[0];
	Real spread = type_check (n->tail.at (3), ARRAY)->array[0];
	Real f = 0;
	if (MODE == 1) { 
		f = specskew<Real>  (&amps[0], &freqs[0], centr, spread, amps.size ()); 
	}
	else { 
		f = speckurt<Real>  (&amps[0], &freqs[0], centr, spread, amps.size ()); 
	}
	return make_atom (f);
}	
AtomPtr fn_specflux (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& amps = type_check (n->tail.at (0), ARRAY)->array;
	std::valarray<Real>& oamps = type_check (n->tail.at (1), ARRAY)->array;
	Real f = specflux<Real> (&amps[0], &oamps[0], amps.size ());
	return make_atom (f);
}		
AtomPtr fn_specirr (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& amps = type_check (n->tail.at (0), ARRAY)->array;
	Real f = specirr<Real> (&amps[0], amps.size ());
	return make_atom (f);
}	
AtomPtr fn_specdecr (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& amps = type_check (n->tail.at (0), ARRAY)->array;
	Real f = specdecr<Real> (&amps[0], amps.size ());
	return make_atom (f);
}				
AtomPtr fn_acorrf0 (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& sig = type_check (n->tail.at (0), ARRAY)->array;		
	Real sr = type_check (n->tail.at (1), ARRAY)->array[0];
	std::valarray<Real> buff (sig.size ());
	Real f = acfF0Estimate<Real> (sr, &sig[0], &buff[0], sig.size ());
	return make_atom (f);
}				
AtomPtr fn_energy (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& sig = type_check (n->tail.at (0), ARRAY)->array;		
	Real f = energy<Real> (&sig[0], sig.size ());
	return make_atom (f);
}		
AtomPtr fn_zcr (AtomPtr n, AtomPtr env) {
	std::valarray<Real>& sig = type_check (n->tail.at (0), ARRAY)->array;		
	Real f = zcr<Real> (&sig[0], sig.size ());
	return make_atom (f);
}			
AtomPtr add_signals (AtomPtr env) {
	add_op ("mix", fn_mix, 2, env);	
	add_op ("gen", fn_gen, 2, env);
	add_op ("osc", fn_osc, 3, env);
	add_op ("reson", fn_reson, 3, env);
	add_op ("fft", fn_fft<1>, 1, env);
	add_op ("ifft", fn_fft<-1>, 1, env);
	add_op ("car2pol", fn_car2pol, 1, env);
	add_op ("pol2car", fn_pol2car, 1, env);
	add_op ("conv", fn_conv, 3, env);
	add_op ("noise", fn_noise, 1, env);
	add_op ("window", fn_window, 4, env);
	add_op ("speccent", fn_speccent, 2, env);
	add_op ("specspread", fn_specspread, 3, env);
	add_op ("specskew", fn_specsk_kur<1>, 4, env);
	add_op ("speckurt", fn_specsk_kur<2>, 4, env);
	add_op ("specflux", fn_specflux, 2, env);
	add_op ("specirr", fn_specirr, 1, env);
	add_op ("specdecr", fn_specdecr, 1, env);						
	add_op ("acorrf0", fn_acorrf0, 2, env);
	add_op ("energy", fn_energy, 1, env);
	add_op ("zcr", fn_zcr, 1, env);		
	return env;
}
#endif	// ARRAY_H 

// EOF
