#ifndef QBITSTREAM_H
#define QBITSTREAM_H

#include "flavori.h"
#include <stdint.h>

#include <sstream>
#include <iostream>
#include <string>
#include <smallvector.h>

void flerror(const char* fmt, ...);

const int BS_BUF_LEN=1024;  // buffer size, in bytes

// Bitstream class
class QBitstream : public IBitstream
{
private:
    Bitstream_t _type;       // type of bitstream (input/output)
    std::istream * _input_device;
    std::ostream * _output_device;

    flavor::SmallVector<uint8_t> * _vector;
    size_t _vpos;

    bool        _ownDevice;

    unsigned char *buf;     // buffer
    int buf_len;		    // usable buffer size (for partially filled buffers)
    int cur_bit;            // current bit position in buf
    uint64_t tot_bits;      // total bits read/written
    unsigned char end;      // end of data flag

    Error_t err_code;       // error code (useful when exceptions are not supported)

    unsigned int _zcount;   // number of zeros counted on most recent _countZero
private:
    // functions
    void fill_buf();        // fills buffer
    void flush_buf();       // flushes buffer

    // sets error code
    void seterror(Error_t err) { err_code=err; }

    // jl - count up to maxz zeros from the current bit position
    int _countZero(int maxz);
public:
    // convert error code to text message
    static char* const err2msg(Error_t code);

public:
    explicit QBitstream(std::istream * device, bool ownDevice = false);
    explicit QBitstream(std::ostream * device, bool ownDevice = false);
    QBitstream(flavor::SmallVector<uint8_t> * device, Bitstream_t mode, bool ownDevice = false);

    // default destructor, does not explicitly close the QIODevice
    ~QBitstream();

    // get mode
    Bitstream_t getmode() { return _type; }

    bool isWriteable() {
        return ((int)_type & (int)BS_OUTPUT) == BS_OUTPUT;
    }

    bool isReadable() {
        return ((int)_type & (int)BS_INPUT) == BS_INPUT;
    }
    // bitstream operations

    ////////////////
    // Big endian //
    ////////////////

    // probe next 'n' bits, do not advance
    uint64_t nextbits(int n);

    // probe next 'n' bits with sign extension, do not advance (sign extension only if n>1)
    uint64_t snextbits(int n);

    // get next 'n' bits, advance
    uint64_t getbits(int n);

    // get next 'n' bits with sign extension, advance (sign extension only if n>1)
    uint64_t sgetbits(int n);

    // float
    float nextfloat(void);
    float getfloat(void);

    // double
    double nextdouble(void);
    double getdouble(void);

    // long double
    long double nextldouble(void) { return nextdouble(); }
    long double getldouble(void) { return getdouble(); }

    // put 'n' bits
    int putbits(uint64_t value, int n);

    // float
    float putfloat(float value);

    // double
    double putdouble(double value);

    // long double
    long double putldouble(double value) { return putdouble(value); }

    // jl - get/put buffers
    uint64_t getBuffer(uint8_t * buffer, uint64_t size);
    uint64_t putBuffer(uint8_t * buffer, uint64_t size);

    // jl - support of seeking on QIODevice
    bool canSeek();
    void seek(int64_t pos);

    // returns the current read/write position in the io device in BITS or -1 if not seekable.
    int64_t tell();

    // jltd - true if underlying io is eof and there are no available bits in the buffer
    bool eof();

    ///////////////////
    // Little endian //
    ///////////////////

    uint64_t little_nextbits(int n);
    uint64_t little_snextbits(int n);
    uint64_t little_getbits(int n);
    uint64_t little_sgetbits(int n);
    float little_nextfloat(void);
    float little_getfloat(void);
    double little_nextdouble(void);
    double little_getdouble(void);
    long double little_nextldouble(void) { return little_nextdouble(); }
    long double little_getldouble(void) { return little_getdouble(); }
    int little_putbits(uint64_t value, int n);
    float little_putfloat(float value);
    double little_putdouble(double value);
    long double little_putldouble(double value) { return little_putdouble(value); }


    // skip next 'n' bits (both input/output); n>=0
    void skipbits(int n);

    // align bitstream (n must be multiple of 8, both input/output); returns bits skipped
    int align(int n);

    // probe next 'n' bits (input)
    uint64_t next(int n, int big, int sign, int alen);

    // search for a specified code; returns the number of bits skipped/written (both input/output)
    uint64_t nextcode(uint64_t code, int n, int alen);

    // get current position in bits (both input/output)
    uint64_t getpos(void) { return (canSeek() ? tell() : tot_bits); }


    // flush buffer; left-over bits are also output with zero padding (output only)
    void flushbits();

    // returns 1 if reached end of data
    inline int atend() { return end; }

    // get last error
    inline int geterror(void) { return err_code; }
    
    // get last error in text form
    char* const getmsg(void) { return err2msg(err_code); }

    ///////////////////
    // Exp Golomb    //
    ///////////////////

    uint64_t nextbits_expgolomb(int32_t n);
    uint64_t snextbits_expgolomb(int32_t n);
    uint64_t getbits_expgolomb(int32_t n);
    uint64_t sgetbits_expgolomb(int32_t n);
    int putbits_expgolomb(uint64_t value, int32_t n);
    int putbits_sexpgolomb(uint64_t value, int32_t n);
};

#endif // QBITSTREAM_H
