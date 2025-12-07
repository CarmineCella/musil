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
    Real norm = 0;
    for (unsigned j = 0; j < coeff.size (); ++j) {
        norm += coeff[j];
    }
    if (norm == 0) norm = 1;

    for (unsigned i = 0; i < values.size () - 1; ++i) {
        Real acc = 0;
        for (unsigned j = 0; j < coeff.size (); ++j) {
            acc += coeff[j] * sin (2. * M_PI * (j + 1) * (Real) i / values.size ());
        }
        values[i] = acc / norm;
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
inline std::valarray<Real> conv_one_channel(
    const std::valarray<Real>& x,
    const std::valarray<Real>& y) {
    int x_sz = (int)x.size();
    int y_sz = (int)y.size();
    if (x_sz == 0 || y_sz == 0) { return std::valarray<Real>(); }

    int conv_len = x_sz + y_sz - 1;
    int N = next_pow2(conv_len);
    int n_complex = N;
    int len = 2 * n_complex;

    std::valarray<Real> X(Real(0), len);
    std::valarray<Real> Y(Real(0), len);
    std::valarray<Real> R(Real(0), len);
    for (int i = 0; i < N; ++i) {
        Real xv = (i < x_sz ? x[i] : Real(0));
        Real yv = (i < y_sz ? y[i] : Real(0));
        X[2 * i]     = xv;
        X[2 * i + 1] = Real(0);
        Y[2 * i]     = yv;
        Y[2 * i + 1] = Real(0);
    }

	fft<Real>(&X[0], n_complex,  -1);
    fft<Real>(&Y[0], n_complex,  -1);

    // complex multiplication: R = X * Y
    for (int i = 0; i < n_complex; ++i) {
        Real xr = X[2 * i];
        Real xi = X[2 * i + 1];
        Real yr = Y[2 * i];
        Real yi = Y[2 * i + 1];

        R[2 * i]     = xr * yr - xi * yi;
        R[2 * i + 1] = xr * yi + xi * yr;
    }

    fft<Real>(&R[0], n_complex, 1);

    std::valarray<Real> out(Real(0), conv_len);
	for (int s = 0; s < conv_len; ++s) {
		out[s] = R[2 * s] / N;  // normalize by N
	}	

    return out;
}
inline std::valarray<Real> fd_resample(const std::valarray<Real>& x, Real factor) {
    int in_len = (int)x.size();
    if (in_len == 0) return std::valarray<Real>();

    if (factor <= (Real)0) {
        return std::valarray<Real>(); // or error
    }

    int out_len = (int)std::floor((Real)in_len * factor + (Real)0.5);
    if (out_len <= 0) out_len = 1;

    int N1 = next_pow2(in_len);
    int N2 = next_pow2(out_len);

    // forward FFT of input (size N1)
    std::valarray<Real> X(Real(0), 2 * N1);
    for (int i = 0; i < N1; ++i) {
        Real xv = (i < in_len ? x[i] : (Real)0);
        X[2 * i]     = xv;
        X[2 * i + 1] = (Real)0;
    }
    fft<Real>(&X[0], N1, -1); // forward

    // build new spectrum Y (size N2) with copied low-frequency bins
    std::valarray<Real> Y(Real(0), 2 * N2);

    int N1_half = N1 / 2;
    int N2_half = N2 / 2;
    int Ncopy   = std::min(N1_half, N2_half);

    // DC
    Y[0] = X[0];
    Y[1] = X[1];

    // positive freqs, excluding DC and Nyquist
    for (int k = 1; k < Ncopy; ++k) {
        Y[2 * k]     = X[2 * k];
        Y[2 * k + 1] = X[2 * k + 1];

        // negative side (Hermitian)
        int k1 = N1 - k;
        int k2 = N2 - k;
        Y[2 * k2]     = X[2 * k1];
        Y[2 * k2 + 1] = X[2 * k1 + 1];
    }

    // Nyquist (if both even)
    if ((N1 % 2 == 0) && (N2 % 2 == 0)) {
        Y[2 * N2_half]     = X[2 * N1_half];
        Y[2 * N2_half + 1] = X[2 * N1_half + 1];
    }

    // inverse FFT at size N2
    fft<Real>(&Y[0], N2, +1);

    std::valarray<Real> out(Real(0), out_len);
    for (int n = 0; n < out_len; ++n) {
        // normalize by N2 because FFT is unnormalized
        out[n] = Y[2 * n] / (Real)N2;
    }

    return out;
}

// signals --------------------------------------------------------------------------------------
// basic, gen, ...
AtomPtr fn_mix (AtomPtr node, AtomPtr env) {
	std::vector<Real> out;
	if (node->tail.size () % 2 != 0) error ("[mix] invalid number of arguments", node);
	for (unsigned i = 0; i < node->tail.size () / 2; ++i) {
		int p = (int) type_check (node->tail.at (i * 2), ARRAY)->array[0];
		if (p < 0) error ("[mix] invalid mix point", node);
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
	if (len <= 0) error ("[gen] invalid length", node);
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

// freq domain
AtomPtr fn_fft_forward (AtomPtr n, AtomPtr env) {
    std::valarray<Real>& sig = type_check (n->tail.at (0), ARRAY)->array;
    int d = sig.size ();
    int N = next_pow2 (d);
    int n_complex = N;
    int len = 2 * n_complex;

    std::valarray<Real> buf (Real(0), len);
    for (int i = 0; i < N; ++i) {
        Real x = (i < d ? sig[i] : 0);
        buf[2 * i]     = x;
        buf[2 * i + 1] = 0;
    }

    fft<Real>(&buf[0], n_complex, -1);  // forward, sign=-1

    return make_atom (buf); // complex spectrum
}
AtomPtr fn_fft_inverse (AtomPtr n, AtomPtr env) {
    std::valarray<Real>& spec = type_check (n->tail.at (0), ARRAY)->array;
    int len = spec.size ();
    if (len % 2 != 0) error("[ifft] spectrum length must be even", n);
    int n_complex = len / 2;

    std::valarray<Real> buf = spec;  // copy
    fft<Real>(&buf[0], n_complex, +1); // inverse, sign=+1

    std::valarray<Real> out (Real(0), n_complex);
    for (int i = 0; i < n_complex; ++i) {
        out[i] = buf[2 * i] / n_complex; // normalize
    }

    return make_atom (out);
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
AtomPtr fn_conv(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    std::valarray<Real>& y = type_check(node->tail.at(1), ARRAY)->array;

    if (x.size() == 0 || y.size() == 0) {
        error("[conv] empty input signals", node);
    }

    std::valarray<Real> out = conv_one_channel(x, y);
    return make_atom(out);
}
AtomPtr fn_convmc(AtomPtr node, AtomPtr env) {
    AtomPtr matx = node->tail.at(0);
    AtomPtr maty = node->tail.at(1);

    if (matx->type != LIST || maty->type != LIST) {
        error("[convmc] arguments must be lists of arrays", node);
    }

    int nx = (int)matx->tail.size();
    int ny = (int)maty->tail.size();

    if (nx == 0 || ny == 0) error("[convmc] empty channel list", node);

    int max_ch = std::max(nx, ny);
    AtomPtr out_channels = make_atom ();
    out_channels->tail.reserve(max_ch);

    for (int i = 0; i < max_ch; ++i) {
        int ich = (i >= nx ? nx - 1 : i);
        int dch = (i >= ny ? ny - 1 : i);

        std::valarray<Real>& x_ch = type_check(matx->tail.at(ich), ARRAY)->array;
        std::valarray<Real>& y_ch = type_check(maty->tail.at(dch), ARRAY)->array;

        if (x_ch.size() == 0 || y_ch.size() == 0) {
            error("[convmc] empty channel signal", node);
        }

        std::valarray<Real> out_ch = conv_one_channel(x_ch, y_ch);
        out_channels->tail.push_back(make_atom(out_ch));
    }
    return out_channels;
}

// filters
AtomPtr fn_dcblock(AtomPtr node, AtomPtr env) {
    if (node->tail.size() < 1 || node->tail.size() > 2) {
        error("[dcblock] requires 1 or 2 arguments: sig [R]", node);
    }

    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    Real R = (node->tail.size() == 2)
           ? type_check(node->tail.at(1), ARRAY)->array[0]
           : (Real)0.995; // default pole

    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);
    if (N == 0) return make_atom(y);

    // simplest: y[0] = x[0]
    y[0] = x[0];
    for (int n = 1; n < N; ++n) {
        Real xn  = x[n];
        Real xm1 = x[n - 1];
        Real ym1 = y[n - 1];
        y[n] = xn - xm1 + R * ym1;
    }

    return make_atom(y);
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

	Real y1 = 0;
	Real y2 = 0;
	for (unsigned i = 0; i < samps; ++i) {
		Real x = i < insize ? array[i] : 0;
		Real v = gain * x - (a1 * y1) - (a2 * y2);
		y2 = y1;
		y1 = v;
		out[i] = v;
	}
	return make_atom (out);
}
AtomPtr fn_filter(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    std::valarray<Real>& b = type_check(node->tail.at(1), ARRAY)->array;
    std::valarray<Real>& a = type_check(node->tail.at(2), ARRAY)->array;

    int N  = (int)x.size();
    int Nb = (int)b.size();
    int Na = (int)a.size();

    if (Nb == 0) error("[filter] b must be non-empty", node);
    if (Na == 0) error("[filter] a must be non-empty", node);

    Real a0 = a[0];
    if (a0 == (Real)0) error("[filter] a[0] cannot be zero", node);

    // normalize so a[0] = 1
    std::valarray<Real> bnorm = b / a0;
    std::valarray<Real> anorm = a / a0;

    std::valarray<Real> y(Real(0), N);

    for (int n = 0; n < N; ++n) {
        Real acc = 0;
        // feedforward
        for (int k = 0; k < Nb; ++k) {
            int idx = n - k;
            if (idx >= 0) {
                acc += bnorm[k] * x[idx];
            }
        }
        // feedback (skip k=0)
        for (int k = 1; k < Na; ++k) {
            int idx = n - k;
            if (idx >= 0) {
                acc -= anorm[k] * y[idx];
            }
        }
        y[n] = acc;
    }

    return make_atom(y);
}
AtomPtr fn_filtdesign(AtomPtr node, AtomPtr env) {
    if (node->tail.size() != 5) {
        error("[filtdesign] requires 5 arguments: type, Fs, f0, Q, gain_db", node);
    }

    std::string type = type_check (node->tail.at(0), SYMBOL)->lexeme;

    Real Fs  = type_check(node->tail.at(1), ARRAY)->array[0];
    Real f0  = type_check(node->tail.at(2), ARRAY)->array[0];
    Real Q   = type_check(node->tail.at(3), ARRAY)->array[0];
    Real dBg = type_check(node->tail.at(4), ARRAY)->array[0];

    if (Fs <= 0 || f0 <= 0 || f0 >= Fs / 2) {
        error("[filtdesign] invalid Fs or f0", node);
    }
    if (Q <= 0) {
        error("[filtdesign] Q must be > 0", node);
    }

    Real w0   = (Real)(2.0 * M_PI * (f0 / Fs));
    Real cosw = cos(w0);
    Real sinw = sin(w0);
    Real alpha = sinw / (2 * Q);
    Real A     = pow((Real)10.0, dBg / (Real)40.0);

    Real b0 = 0, b1 = 0, b2 = 0, a0 = 0, a1 = 0, a2 = 0;

    if (type == "lowpass") {
        b0 = (1 - cosw) / 2;
        b1 = 1 - cosw;
        b2 = (1 - cosw) / 2;
        a0 = 1 + alpha;
        a1 = -2 * cosw;
        a2 = 1 - alpha;
    } else if (type == "highpass") {
        b0 = (1 + cosw) / 2;
        b1 = -(1 + cosw);
        b2 = (1 + cosw) / 2;
        a0 = 1 + alpha;
        a1 = -2 * cosw;
        a2 = 1 - alpha;
    } else if (type == "notch") {
        b0 = 1;
        b1 = -2 * cosw;
        b2 = 1;
        a0 = 1 + alpha;
        a1 = -2 * cosw;
        a2 = 1 - alpha;
    } else if (type == "peak" || type == "peaking") {
        b0 = 1 + alpha * A;
        b1 = -2 * cosw;
        b2 = 1 - alpha * A;
        a0 = 1 + alpha / A;
        a1 = -2 * cosw;
        a2 = 1 - alpha / A;
    } else if (type == "lowshelf" || type == "loshelf") {
        Real sqrtA = sqrt(A);
        b0 =    A * ((A + 1) - (A - 1) * cosw + 2 * sqrtA * alpha);
        b1 =  2*A * ((A - 1) - (A + 1) * cosw);
        b2 =    A * ((A + 1) - (A - 1) * cosw - 2 * sqrtA * alpha);
        a0 =        (A + 1) + (A - 1) * cosw + 2 * sqrtA * alpha;
        a1 =   -2 * ((A - 1) + (A + 1) * cosw);
        a2 =        (A + 1) + (A - 1) * cosw - 2 * sqrtA * alpha;
    } else if (type == "highshelf" || type == "hishelf") {
        Real sqrtA = sqrt(A);
        b0 =    A * ((A + 1) + (A - 1) * cosw + 2 * sqrtA * alpha);
        b1 = -2*A * ((A - 1) + (A + 1) * cosw);
        b2 =    A * ((A + 1) + (A - 1) * cosw - 2 * sqrtA * alpha);
        a0 =        (A + 1) - (A - 1) * cosw + 2 * sqrtA * alpha;
        a1 =    2 * ((A - 1) - (A + 1) * cosw);
        a2 =        (A + 1) - (A - 1) * cosw - 2 * sqrtA * alpha;
    } else {
        error("[filtdesign] unknown filter type", node);
    }

    // normalize so a0 = 1
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;
    a0 = 1;

    // build b and a arrays: [b0 b1 b2], [1 a1 a2]
    std::valarray<Real> b_arr((Real)0, 3);
    b_arr[0] = b0; b_arr[1] = b1; b_arr[2] = b2;

    std::valarray<Real> a_arr((Real)0, 3);
    a_arr[0] = (Real)1;
    a_arr[1] = a1;
    a_arr[2] = a2;

    AtomPtr b_atom = make_atom(b_arr);
    AtomPtr a_atom = make_atom(a_arr);

    AtomPtr out_list = make_atom();
    out_list->tail.push_back(b_atom);
    out_list->tail.push_back(a_atom);

    return out_list;
}
AtomPtr fn_delay(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    Real D = type_check(node->tail.at(1), ARRAY)->array[0];

    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);
    if (N == 0) return make_atom(y);

    for (int n = 0; n < N; ++n) {
        Real pos = (Real)n - D;
        if (pos <= (Real)0) {
            y[n] = 0;
        } else {
            int i0 = (int)floor(pos);
            Real frac = pos - (Real)i0;
            if (i0 >= N - 1) {
                y[n] = x[N - 1];
            } else {
                Real x0 = x[i0];
                Real x1 = x[i0 + 1];
                y[n] = (Real)1.0 * ((Real)1.0 - frac) * x0 + frac * x1;
            }
        }
    }

    return make_atom(y);
}
AtomPtr fn_comb(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    Real Df = type_check(node->tail.at(1), ARRAY)->array[0];
    Real g  = type_check(node->tail.at(2), ARRAY)->array[0];

    int D = (int)Df;
    if (D < 0) error("[comb] delay must be non-negative", node);

    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);

    for (int n = 0; n < N; ++n) {
        Real fb = 0;
        if (n - D >= 0) fb = g * y[n - D];
        y[n] = x[n] + fb;
    }

    return make_atom(y);
}
AtomPtr fn_allpass(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    Real Df = type_check(node->tail.at(1), ARRAY)->array[0];
    Real g  = type_check(node->tail.at(2), ARRAY)->array[0];

    int D = (int)Df;
    if (D < 0) error("[allpass] delay must be non-negative", node);

    int N = (int)x.size();
    std::valarray<Real> y(Real(0), N);

    for (int n = 0; n < N; ++n) {
        Real xmD = 0;
        Real ymD = 0;
        if (n - D >= 0) {
            xmD = x[n - D];
            ymD = y[n - D];
        }
        y[n] = -g * x[n] + xmD + g * ymD;
    }

    return make_atom(y);
}
AtomPtr fn_resample(AtomPtr node, AtomPtr env) {
    if (node->tail.size() != 2) {
        error("[resample] requires 2 arguments: sig, factor", node);
    }

    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    Real factor = type_check(node->tail.at(1), ARRAY)->array[0];

    std::valarray<Real> y = fd_resample(x, factor);
    return make_atom(y);
}

AtomPtr add_signals (AtomPtr env) {
	add_op ("mix", fn_mix, 2, env);	
	add_op ("gen", fn_gen, 2, env);
	add_op ("osc", fn_osc, 3, env);

	add_op ("fft", fn_fft_forward, 1, env);
	add_op ("ifft", fn_fft_inverse, 1, env);
	add_op ("car2pol", fn_car2pol, 1, env);
	add_op ("pol2car", fn_pol2car, 1, env);
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
  	add_op("conv",   fn_conv,   2, env);
	add_op("convmc", fn_convmc, 2, env);	

	add_op ("dcblock",   fn_dcblock,   2, env);
	add_op ("reson", 	 fn_reson, 4, env);	
    add_op ("filter",    fn_filter,    3, env);
    add_op ("filtdesign",fn_filtdesign,5, env);	
    add_op ("delay",     fn_delay,     2, env);
    add_op ("comb",      fn_comb,      3, env);
    add_op ("allpass",   fn_allpass,   3, env);
    add_op ("resample",  fn_resample,  2, env);

	return env;
}
#endif	// SIGNALS_H 

// EOF
