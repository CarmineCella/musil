// BPF.h
//

#ifndef BPF_H
#define BPF_H

#include <valarray>
#include <vector>

template <typename T>
class Processor {
public:
    Processor (int len) {
        _len = len;
    }
    virtual ~Processor () {}
    virtual void process (std::valarray<T>& out) = 0;
    virtual int len () const {
        return _len;
    }
protected:
    T _len;
};
template <typename T>
class Segment : public Processor<T> {
public:
    Segment (T init, int len, T end) : Processor <T> (len) {
        set (init, len, end);
    }
    void set (T init_val, int len, T end_val) {
        _init_val = init_val;
        Processor<T>::_len = len;
        _end_val = end_val;
    }
    void process (std::valarray<T>& out) {
        int s = Processor<T>::len ();
        if (out.size () < s) out.resize (s);
        T val = _init_val;
        T incr = (_end_val - _init_val) / s;
        for (int i = 0; i < s; ++i) {
            out[i] = val;
            val += incr;
        }
    }
private:
    T _init_val, _end_val;
};
template <typename T>
class BPF : public Processor<T> {
public:
    BPF (int len) : Processor <T> (len) {}
    ~BPF () {
        for (unsigned i = 0; i < _segments.size (); ++i) delete _segments[i];
    }
    void add_segment (T init_val, int len, T end_val) {
        Segment<T>* s = new Segment<T> (init_val, len, end_val);
        _segments.push_back (s);
    }
    int len () const {
        int s = 0;
        for (unsigned i = 0; i < _segments.size (); ++i) {
            s += _segments[i]->len ();
        }
        return s;
    }
    void process (std::valarray<T>& out) {
        int s = len ();
        if (out.size () < s) out.resize (s, 0);
        int offset = 0;
        for (unsigned i = 0; i < _segments.size (); ++i) {
            std::valarray<T> tmp;
            _segments[i]->process (tmp);
            for (unsigned k = 0; k < tmp.size (); ++k) {
                out[k + offset] = tmp[k];
            }
            offset += _segments[i]->len ();
        }
    }
private:
    std::vector<Segment<T>* > _segments;
};

#endif	// BPF_H 

// EOF
