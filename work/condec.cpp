// condec.cpp

#include <iostream>
#include <fstream>


#include <cstring>
#include <sstream>
#include <stdexcept>
#include <complex>

#include "dsp.h"
#include "utils.h"

using namespace std;

std::complex<double>* process (std::complex<double>* in_spectrum, std::complex<double>* deconv_signal, 
    std::complex<double>* result_spectrum, int N, double threshold, int op) {

    int mpos = 0;
    double mm = max_val_cplx (deconv_signal, N, mpos);

    for (int i = 0; i < N; ++i) {
        double denom_mag = std::abs (deconv_signal[i]);
        if (denom_mag > threshold * mm) {
            result_spectrum[i] = op >= 0 ? in_spectrum[i] * deconv_signal[i] : in_spectrum[i] / deconv_signal[i];
        } else {
            result_spectrum[i] = 0.0;
        }
    }

    return result_spectrum;
}

// ------------------------------
int main (int argc, char** argv) {
    cout << "[condec, ver. 0.1]" << endl << endl;
    cout << "(c) 2024 Carmine-Emanuele Cella" << endl << endl;
    try {
        if (argc != 8) {
            stringstream err;
            err << "syntax is '" << argv[0] << " <x_wav> <y_wav> <output_wav> op threshold scale mix'" << endl << endl
                << "where: " << endl
                << "op > 0 for convolution; op < 0 for deconvolution" << endl
                << "threshold is used for filtering in the frequency domain" << endl
                << "scale changes the amplification for <output_wav>" << endl
                << "mix is used to sum <x_wav> in conv or remove <y_wav> in deconv (residual)" << endl;

            throw runtime_error (err.str ());
        }

        const char* x_file = argv[1];
        const char* y_file = argv[2];
        const char* output_file = argv[3];
        const int op = atol (argv[4]);
        double threshold = atof (argv[5]);
        double scale = atof (argv[6]);
        double mix = atof (argv[7]);

        cout << "operation = " << (op > 0 ? "conv" : "deconv") << endl;
        cout << "threshold = " << threshold << endl;
        cout << "scale     = " << scale << endl;
        cout << "mix       = " << mix << endl;

        WAVHeader x_header, y_header;
        vector<vector<double>> x_data = read_wav (x_file, x_header);
        vector<vector<double>> y_data = read_wav (y_file, y_header);

        int max_ch = std::max (x_data.size (), y_data.size ());          
        int max_size = std::max (x_data[0].size(), y_data[0].size());
        int N = next_power_of_two (max_size);
        complex<double>* x_FFT = new complex<double>[N];
        complex<double>* y_FFT = new complex<double>[N];
        complex<double>* result_FFT = new complex<double>[N];
        
        int out_sz = N; //op >= 0 ? x_data[0].size () + y_data[0].size () : N;
        vector<vector<double>> output_data(max_ch, vector<double> (out_sz));
        vector<int> maxes (max_ch);

        cout << endl << "processing..."; cout.flush ();
        for (int i = 0; i < max_ch; ++i) {
            int ich = i >= (int) x_data.size () ? x_data.size () - 1 : i;
            int dch = i >= (int)  y_data.size () ? y_data.size () - 1 : i;
            x_data[ich].resize (N, 0.0);
            y_data[dch].resize (N, 0.0);

            for (int i = 0; i < N; ++i) {
                x_FFT[i] = x_data[ich][i];
                y_FFT[i] = y_data[dch][i];
            }

            fft (x_FFT, N);
            fft (y_FFT, N);
            process (x_FFT, y_FFT, result_FFT, N, threshold, op);
            ifft (result_FFT, N);

            max_val_cplx (result_FFT, N,  maxes[i]);
            for (int s = 0; s <  out_sz; ++s) {
                output_data[i][s] = result_FFT[s].real() * scale;
                if (s < (int) x_data[0].size () && op >= 0) output_data[i][s] += x_data[ich][s] * mix;
            }
        }
        cout << "done" << endl << endl;

        WAVHeader output_header = x_header;
        output_header.numChannels = output_data.size ();
        write_wav (output_file, output_data, output_header);
        cout << "output saved to  : " << output_file << endl;

        if (op < 0) {
            for (int i = 0; i < max_ch; ++i) {
                int ich = i >= (int) x_data.size () ? x_data.size () - 1 : i;
                int dch = i >= (int) y_data.size () ? y_data.size () - 1 : i;                
                for (int s = 0; s <  out_sz; ++s) {
                    if (s + maxes[i] < (int) x_data[ich].size ()) x_data[ich][s + maxes[i]] -= y_data[dch][s] * mix;
                }
            }

            stringstream res_file;
            res_file << remove_extension ((string) output_file) << ".residual.wav";
            write_wav (res_file.str ().c_str (), x_data, x_header);
            cout << "residual saved to: " << res_file.str () << endl << endl;
        }

        delete [] x_FFT;
        delete [] y_FFT;
        delete [] result_FFT;

        return 0;
    } catch (exception& err) {
        cout << "error: " << err.what () << endl;
    } catch (...) {
        cout << "fatal error; quitting..." << endl;
    }
    return 0;
}

// eof

 