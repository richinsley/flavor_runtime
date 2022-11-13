// Bitstream IO implementation
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <algorithm>
#include <stdarg.h>

#include "fbitstream.h"

// This is our standard implementation in case it is not overriden by the user
void flerror(const char* fmt, ...)
{
    fprintf(stderr, "Bitstream syntax error: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    // TODO - implement a more complete error system
    throw "Flavor boo-boo";
}

#define BSHIFT      3

static const uint8_t charbitmask[8] = {
    0x80,
    0x40,
    0x20,
    0x10,
    0x08,
    0x04,
    0x02,
    0x01
};

// masks for bitstring manipulation
static const uint64_t mask[65] = {
    0x0000000000000000, 0x0000000000000001, 0x0000000000000003, 0x0000000000000007,
    0x000000000000000f, 0x000000000000001f, 0x000000000000003f, 0x000000000000007f,
    0x00000000000000ff, 0x00000000000001ff, 0x00000000000003ff, 0x00000000000007ff,
    0x0000000000000fff, 0x0000000000001fff, 0x0000000000003fff, 0x0000000000007fff,
    0x000000000000ffff, 0x000000000001ffff, 0x000000000003ffff, 0x000000000007ffff,
    0x00000000000fffff, 0x00000000001fffff, 0x00000000003fffff, 0x00000000007fffff,
    0x0000000000ffffff, 0x0000000001ffffff, 0x0000000003ffffff, 0x0000000007ffffff,
    0x000000000fffffff, 0x000000001fffffff, 0x000000003fffffff, 0x000000007fffffff,

    0x00000000ffffffff, 0x00000001ffffffff, 0x00000003ffffffff, 0x00000007ffffffff,
    0x0000000fffffffff, 0x0000001fffffffff, 0x0000003fffffffff, 0x0000007fffffffff,
    0x000000ffffffffff, 0x000001ffffffffff, 0x000003ffffffffff, 0x000007ffffffffff,
    0x00000fffffffffff, 0x00001fffffffffff, 0x00003fffffffffff, 0x00007fffffffffff,
    0x0000ffffffffffff, 0x0001ffffffffffff, 0x0003ffffffffffff, 0x0007ffffffffffff,
    0x000fffffffffffff, 0x001fffffffffffff, 0x003fffffffffffff, 0x007fffffffffffff,
    0x00ffffffffffffff, 0x01ffffffffffffff, 0x03ffffffffffffff, 0x07ffffffffffffff,
    0x0fffffffffffffff, 0x1fffffffffffffff, 0x3fffffffffffffff, 0x7fffffffffffffff,
    0xffffffffffffffff
};

// complement masks (used for sign extension)
static const uint64_t cmask[65] = {
    0xffffffffffffffff, 0xfffffffffffffffe, 0xfffffffffffffffc, 0xfffffffffffffff8,
    0xfffffffffffffff0, 0xffffffffffffffe0, 0xffffffffffffffc0, 0xffffffffffffff80,
    0xffffffffffffff00, 0xfffffffffffffe00, 0xfffffffffffffc00, 0xfffffffffffff800,
    0xfffffffffffff000, 0xffffffffffffe000, 0xffffffffffffc000, 0xffffffffffff8000,
    0xffffffffffff0000, 0xfffffffffffe0000, 0xfffffffffffc0000, 0xfffffffffff80000,
    0xfffffffffff00000, 0xffffffffffe00000, 0xffffffffffc00000, 0xffffffffff800000,
    0xffffffffff000000, 0xfffffffffe000000, 0xfffffffffc000000, 0xfffffffff8000000,
    0xfffffffff0000000, 0xffffffffe0000000, 0xffffffffc0000000, 0xffffffff80000000,

    0xffffffff00000000, 0xfffffffe00000000, 0xfffffffc00000000, 0xfffffff800000000,
    0xfffffff000000000, 0xffffffe000000000, 0xffffffc000000000, 0xffffff8000000000,
    0xffffff0000000000, 0xfffffe0000000000, 0xfffffc0000000000, 0xfffff80000000000,
    0xfffff00000000000, 0xffffe00000000000, 0xffffc00000000000, 0xffff800000000000,
    0xffff000000000000, 0xfffe000000000000, 0xfffc000000000000, 0xfff8000000000000,
    0xfff0000000000000, 0xffe0000000000000, 0xffc0000000000000, 0xff80000000000000,
    0xff00000000000000, 0xfe00000000000000, 0xfc00000000000000, 0xf800000000000000,
    0xf000000000000000, 0xe000000000000000, 0xc000000000000000, 0x8000000000000000,
    0x0000000000000000
};

// sign masks (used for sign extension)
static const uint64_t smask[65] = {
    0x0000000000000000, 0x0000000000000001, 0x0000000000000002, 0x0000000000000004,
    0x0000000000000008, 0x0000000000000010, 0x0000000000000020, 0x0000000000000040,
    0x0000000000000080, 0x0000000000000100, 0x0000000000000200, 0x0000000000000400,
    0x0000000000000800, 0x0000000000001000, 0x0000000000002000, 0x0000000000004000,
    0x0000000000008000, 0x0000000000010000, 0x0000000000020000, 0x0000000000040000,
    0x0000000000080000, 0x0000000000100000, 0x0000000000200000, 0x0000000000400000,
    0x0000000000800000, 0x0000000001000000, 0x0000000002000000, 0x0000000004000000,
    0x0000000008000000, 0x0000000010000000, 0x0000000020000000, 0x0000000040000000,

    0x0000000080000000, 0x0000000100000000, 0x0000000200000000, 0x0000000400000000,
    0x0000000800000000, 0x0000001000000000, 0x0000002000000000, 0x0000004000000000,
    0x0000008000000000, 0x0000010000000000, 0x0000020000000000, 0x0000040000000000,
    0x0000080000000000, 0x0000100000000000, 0x0000200000000000, 0x0000400000000000,
    0x0000800000000000, 0x0001000000000000, 0x0002000000000000, 0x0004000000000000,
    0x0008000000000000, 0x0010000000000000, 0x0020000000000000, 0x0040000000000000,
    0x0080000000000000, 0x0100000000000000, 0x0200000000000000, 0x0400000000000000,
    0x0800000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000000000,
    0x8000000000000000
};

QBitstream::QBitstream(std::istream * device, bool ownDevice)
{
    // get the mode the device was openend in
    _input_device = device;
    _ownDevice = ownDevice;
    _type = BS_INPUT;

    cur_bit = 0;
    tot_bits = 0;
    buf_len = BS_BUF_LEN;
    buf = new unsigned char[buf_len];
    memset(buf, 0, BS_BUF_LEN);
    end = 0;
    err_code = E_NONE;

    // read some
    cur_bit = BS_BUF_LEN << BSHIFT;  // fake that we are at the end of buffer
    fill_buf();
}

QBitstream::QBitstream(std::ostream * device, bool ownDevice)
{
    // get the mode the device was openend in
    _output_device = device;
    _ownDevice = ownDevice;
    _type = BS_OUTPUT;

    cur_bit = 0;
    tot_bits = 0;
    buf_len = BS_BUF_LEN;
    buf = new unsigned char[buf_len];
    memset(buf, 0, BS_BUF_LEN);
    end = 0;
    err_code = E_NONE;
}

QBitstream::QBitstream(flavor::SmallVector<uint8_t> * device, Bitstream_t mode, bool ownDevice)
{
    _output_device = NULL;
    _input_device = NULL;
    _vector = device;
    _vpos = 0;
    
    if(mode == BS_OUTPUT)
    {
         // get the mode the device was openend in
        _ownDevice = ownDevice;
        _type = BS_OUTPUT;

        cur_bit = 0;
        tot_bits = 0;
        buf_len = BS_BUF_LEN;
        buf = new unsigned char[buf_len];
        memset(buf, 0, BS_BUF_LEN);
        end = 0;
        err_code = E_NONE;
    }
    else
    {
        // get the mode the device was openend in
        _input_device = NULL;
        _output_device = NULL;
        _ownDevice = ownDevice;
        _type = BS_INPUT;

        cur_bit = 0;
        tot_bits = 0;
        buf_len = BS_BUF_LEN;
        buf = new unsigned char[buf_len];
        memset(buf, 0, BS_BUF_LEN);
        end = 0;
        err_code = E_NONE;

        // read some
        cur_bit = BS_BUF_LEN << BSHIFT;  // fake that we are at the end of buffer
        fill_buf();
    }
}

// standard destructor
QBitstream::~QBitstream()
{
    // make sure all data is out
    if (buf)
    {
        // prevent double flush, which caused the WriteFailed exception before when Destructor is called after Destroy()
        if (_type == BS_OUTPUT)
        {
            flushbits();
        }
        delete[] buf;
        buf = (unsigned char*)0;
    }

    if(_ownDevice)
    {
        if(_input_device)
        {
            delete _input_device;
        }

        if(_output_device)
        {
            delete _output_device;
        }

        if(_vector)
        {
            delete _vector;
        }
    }
}

// convert error code to text message
char* const QBitstream::err2msg(Error_t code)
{
    switch (code) {
    case E_NONE:				return (char*)"<None>";
    case E_END_OF_DATA:			return (char*)"End of data";
    case E_INVALID_ALIGNMENT:	return (char*)"Invalid alignment";
    case E_READ_FAILED:			return (char*)"Read failed";
    case E_WRITE_FAILED:		return (char*)"Write failed";
    case E_SEEK_FAILED:         return (char*)"Seek failed";
    default:					return (char*)"Unknown error";
    }
}

////////////////
// Big endian //
////////////////

// returns 'n' bits as unsigned int; does not advance bit pointer
uint64_t QBitstream::nextbits(int n)
{
    uint64_t x;                 // the value we will return
    unsigned char *v;           // the byte where cur_bit points to
    int s;             // number of bits to shift

    // make sure we have enough data
    if (cur_bit + n > (buf_len << BSHIFT))
    {
        fill_buf();
    }

    // starting byte in buffer
    v = buf + (cur_bit >> BSHIFT);

    // load 8 bytes, a byte at a time - this way endianess is automatically taken care of
    x = ((uint64_t)v[0] << 56) |
            ((uint64_t)v[1] << 48) |
            ((uint64_t)v[2] << 40) |
            ((uint64_t)v[3] << 32) |
            ((uint64_t)v[4] << 24) |
            ((uint64_t)v[5] << 16) |
            ((uint64_t)v[6] << 8) |
            (uint64_t)v[7];

    // figure out how much shifting is required
    s = 64 - ((cur_bit % 8) + n);

    if (s >= 0)
    {
        // need right adjust
        x = (x >> s);
    }
    else
    {
        // shift left and read an extra byte
        x = x << -s;
        x |= v[8] >> (8 + s);
    }
    return (x & mask[n]);
}

int QBitstream::_countZero(int maxz)
{
    // make sure we have enough data
    if ((err_code != E_END_OF_DATA) && cur_bit + maxz > (buf_len << BSHIFT))
    {
        fill_buf();
    }

    if((buf_len << BSHIFT) - cur_bit < maxz) maxz = (buf_len << BSHIFT) - cur_bit;

    // count up to maxz zeros without advancing.
    // if maxz is reached, _zcount will be -1 indicating error
    _zcount = -1;
    for(int i = 0; i < maxz; i++)
    {
        int byteindex = (i + cur_bit) >> BSHIFT;
        int bitindex = (i + cur_bit) % 8;
        unsigned char vv = buf[byteindex];
        if(vv & charbitmask[bitindex])
        {
            // this is a one
            _zcount = i;
            break;
        }
    }
    return _zcount;
}

// Read unsigned Exp-Golomb code from bitstream
uint64_t QBitstream::nextbits_expgolomb(int n)
{
    int zcount = _countZero(n + 1);

    // make sure we have zcount * 2 + 1 bits available
    // if after a fillbuf, and we are at E_END_OF_DATA, this is a failure
    if ((err_code != E_END_OF_DATA) && cur_bit + (zcount * 2 + 1) > (buf_len << BSHIFT))
    {
        fill_buf();
        if(err_code == E_END_OF_DATA)
        {
            if (cur_bit + (zcount * 2 + 1) > (buf_len << BSHIFT))
            {
                // we won't have enough data
                return 0;
            }
        }
    }

    // from this point, this is essentially the same as nextbits
    uint64_t x;                 // the value we will return
    unsigned char *v;           // the byte where cur_bit points to
    int s;                      // number of bits to shift

    // starting byte in buffer
    int abit = cur_bit + zcount;
    v = buf + (abit >> BSHIFT);

    // load 8 bytes, a byte at a time - this way endianess is automatically taken care of
    x = ((uint64_t)v[0] << 56) |
            ((uint64_t)v[1] << 48) |
            ((uint64_t)v[2] << 40) |
            ((uint64_t)v[3] << 32) |
            ((uint64_t)v[4] << 24) |
            ((uint64_t)v[5] << 16) |
            ((uint64_t)v[6] << 8) |
            (uint64_t)v[7];

    // figure out how much shifting is required
    s = 64 - ((abit % 8) + zcount + 1);

    if (s >= 0)
    {
        x = (x >> s);   // need right adjust
    }
    else
    {                      // shift left and read an extra byte
        x = x << -s;
        x |= v[8] >> (8+s);
    }
    return (x & mask[zcount + 1]) - 1;
}

// Read signed Exp-Golomb code from bitstream
uint64_t QBitstream::snextbits_expgolomb(int n)
{
    uint64_t res = nextbits_expgolomb(n);
    int64_t retv = (res / 2 + (res % 2 ? 1 : 0)) * (res % 2 ? 1 : -1);
    return retv;
}

uint64_t QBitstream::getbits_expgolomb(int32_t n)
{
    uint64_t retv = nextbits_expgolomb(n);

    // _zcount will hold the count of zeros
    cur_bit += _zcount * 2 + 1;
    tot_bits += _zcount * 2 + 1;
    return retv;
}

uint64_t QBitstream::sgetbits_expgolomb(int32_t n)
{
    int64_t retv = snextbits_expgolomb(n);

    // _zcount will hold the count of zeros
    cur_bit += _zcount * 2 + 1;
    tot_bits += _zcount * 2 + 1;
    return retv;
}

// write unsigned exp golomb
int QBitstream::putbits_expgolomb(uint64_t value, int n)
{
    uint64_t M = 0;
    uint64_t val = value & mask[n];
    uint64_t val2 = val;

    val2++;
    while (val2 > 1)
    {
        //get the log base 2 of val.
        val2 >>= 1;
        M++;
    }

    if(M > 63)
    {
        // we can't go over 64 zeros for our implementation.
        seterror(E_WRITE_FAILED);
    }
    putbits(0, M);
    putbits(val + 1, M + 1);
    return val;
}

// write signed exp golomb
int QBitstream::putbits_sexpgolomb(uint64_t value, int n)
{
    int64_t tval = (int64_t)value;
    uint64_t aval = abs(tval);
    int64_t s = ((value == 0 ) | (aval != tval)) ? 1 : 0;
    uint64_t scaled = aval * 2 + s;
    putbits_expgolomb(scaled - 1, n);
    return value;
}

// returns 'n' bits as unsigned int with sign extension; does not advance bit pointer (sign extension only if n>1)
uint64_t QBitstream::snextbits(int n)
{
    uint64_t x = nextbits(n);
    if (n>1 && (x & smask[n])) return x | cmask[n];
    else return x;
}

// returns 'n' bits as unsigned int; advances bit pointer
uint64_t QBitstream::getbits(int n)
{
    uint64_t x = nextbits(n);
    cur_bit += n;
    tot_bits += n;
    return (x & mask[n]);
}

// returns 'n' bits as unsigned int with sign extension; advances bit pointer (sign extension only if n>1)
uint64_t QBitstream::sgetbits(int n)
{
    uint64_t x = getbits(n);
    if (n>1 && (x & smask[n]))
    {
        x = x | cmask[n];
    }
    return x;
}

// probe a float
float QBitstream::nextfloat(void)
{
    float f;
    uint32_t nb = nextbits(32);
    *(uint32_t *)&f = nb;
    return f;
}

// get a float
float QBitstream::getfloat(void)
{
    float f;
    uint32_t nb = getbits(32);
    *(uint32_t *)&f = nb;
    return f;
}

// probe a double
double QBitstream::nextdouble(void)
{
    double d;
    // make sure we have enough data (so that we can go back)
    if (cur_bit + 64 > (buf_len << BSHIFT)) fill_buf();
    *(((uint32_t *)&d)+1) = nextbits(32);
    cur_bit += 32;
    *(uint32_t *)&d = nextbits(32);
    cur_bit -= 32;
    return d;
}

// get a double
double QBitstream::getdouble(void)
{
    double d;
    *(((uint32_t *)&d)+1) = getbits(32);
    *(uint32_t *)&d = getbits(32);
    return d;
}

// can only write at least one byte to a file at a time; returns the output value
int QBitstream::putbits(uint64_t value, int n)
{
    int delta;          // required input shift amount
    unsigned char *v;   // current byte
    uint64_t tmp;   // temp value for shifted bits
    uint64_t val;   // the n-bit value

    val = value & mask[n];

    if (cur_bit + n > (buf_len << BSHIFT)) flush_buf();

    delta = 64 - n - (cur_bit % 8);
    v = buf + (cur_bit >> BSHIFT);
    if (delta >= 0) {
        tmp = val << delta;
        int write_bytes_count = (64 - delta - 1) >> BSHIFT;
        switch (write_bytes_count) {
            case 7:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                v[2] = (unsigned char)(tmp >> 40);
                v[3] = (unsigned char)(tmp >> 32);
                v[4] = (unsigned char)(tmp >> 24);
                v[5] = (unsigned char)(tmp >> 16);
                v[6] = (unsigned char)(tmp >> 8);
                v[7] = (unsigned char)(tmp);
                break;
            case 6:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                v[2] = (unsigned char)(tmp >> 40);
                v[3] = (unsigned char)(tmp >> 32);
                v[4] = (unsigned char)(tmp >> 24);
                v[5] = (unsigned char)(tmp >> 16);
                v[6] = (unsigned char)(tmp >> 8);
                break;
            case 5:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                v[2] = (unsigned char)(tmp >> 40);
                v[3] = (unsigned char)(tmp >> 32);
                v[4] = (unsigned char)(tmp >> 24);
                v[5] = (unsigned char)(tmp >> 16);
                break;
            case 4:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                v[2] = (unsigned char)(tmp >> 40);
                v[3] = (unsigned char)(tmp >> 32);
                v[4] = (unsigned char)(tmp >> 24);
                break;
            case 3:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                v[2] = (unsigned char)(tmp >> 40);
                v[3] = (unsigned char)(tmp >> 32);
                break;
            case 2:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                v[2] = (unsigned char)(tmp >> 40);
                break;
            case 1:
                v[0] |= (unsigned char)(tmp >> 56);
                v[1] = (unsigned char)(tmp >> 48);
                break;
            default:
                v[0] |= (unsigned char)(tmp >> 56);
                break;
        }
    }
    else {
        tmp = val >> (-delta);
        v[0] |= (unsigned char)(tmp >> 56);
        v[1] |= (unsigned char)(tmp >> 48);
        v[2] |= (unsigned char)(tmp >> 40);
        v[3] |= (unsigned char)(tmp >> 32);
        v[4] |= (unsigned char)(tmp >> 24);
        v[5] |= (unsigned char)(tmp >> 16);
        v[6] |= (unsigned char)(tmp >> 8);
        v[7] |= (unsigned char)tmp;
        v[8] |= (unsigned char)(value << (8+delta));
    }
    cur_bit += n;
    tot_bits += n;
    return value;
}

// put a float
float QBitstream::putfloat(float value)
{
    uint64_t v = *((uint32_t*)&value);
    putbits(v, 32);
    return value;
}

// put a double
double QBitstream::putdouble(double value)
{
//    putbits(*(((int *)&value)+1), 32);
//    putbits(*(int *)&value, 32);

    uint64_t v = *((uint64_t*)&value);
    putbits(v, 64);
    return value;
}

uint64_t QBitstream::getBuffer(uint8_t * buffer, uint64_t size)
{
    if(!size)
    {
        return 0;
    }

    // uint64_t retv = size;
    uint64_t total_bytes_read = 0;
    if(cur_bit % 8)
    {
        // we're not bit aligned, so we want to read in 1 byte at a time with getbits
        for(int i = 0; i < size; i++)
        {
            buffer[i] = getbits(8);
            total_bytes_read++;
        }
    }
    else
    {
        // see if we have any available bytes in our buffer
        uint64_t mbufsize = std::min((uint64_t)(buf_len - (cur_bit >> BSHIFT)), size);
        if(mbufsize)
        {
            // starting byte in buffer
            uint8_t * v = buf + (cur_bit >> BSHIFT);
            memcpy(buffer, v, mbufsize);
            cur_bit += mbufsize << BSHIFT;
            size -= mbufsize;
            buffer += mbufsize;
            total_bytes_read += mbufsize;
        }

        if(size)
        {
            if(_input_device)
            {
                // try to read the rest from the io stream
                uint64_t br = _input_device->readsome((char*)buffer, (int64_t)size);
                size -= br;
                total_bytes_read += br;
            }
            else
            {
                size_t br = std::min((size_t)(_vector->size() - _vpos), (size_t)size);
                if(br)
                {
                    memcpy(buffer, &_vector->data()[_vpos], br);
                }
                _vpos += br;
                size -= br;
                total_bytes_read += br;
            }
        }

        uint64_t tot_bits_read = total_bytes_read << BSHIFT;
        tot_bits += tot_bits_read;
    }

    return total_bytes_read;
}

uint64_t QBitstream::putBuffer(uint8_t * buffer, uint64_t size)
{
    if(!size)
    {
        return 0;
    }

    if(tot_bits % 8)
    {
        for(uint64_t i = 0; i < size; i++)
        {
            putbits(buffer[i], 8);
        }
    }
    else
    {
        // we're aligned, just flush the internal buffer and write the entire given buffer directly to the device
        flush_buf();
        if(_output_device)
        {
            _output_device->write((char*)buffer, (uint64_t)size);
        }
        else
        {
            _vector->append(buffer, buffer + size);
            _vpos += size;
        }

        cur_bit = 0;
        tot_bits += size >> BSHIFT;
    }

    return size;
}

bool QBitstream::canSeek()
{
    return true;
}

void QBitstream::seek(int64_t pos)
{
    if(!canSeek())
    {
        seterror(E_SEEK_FAILED);
        return;
    }

    // reset end
    end = 0;
    seterror(E_NONE);

    if(_type == BS_INPUT)
    {
        // to seek on input, we'll reload the buffer at new stream position
        if(_input_device)
        {
            if(!_input_device->seekg(pos >> BSHIFT))
            {
                seterror(E_SEEK_FAILED);
                return;
            }
        }
        else
        {
            _vpos = pos >> BSHIFT;
        }
               
        // clear the buffer
        memset(buf, 0, BS_BUF_LEN);

        int64_t l = 0;

        if(_input_device)
        {
            l = _input_device->readsome((char*)buf, BS_BUF_LEN);
        }
        else
        {
            size_t br = std::min((size_t)(_vector->size() - _vpos), (size_t)BS_BUF_LEN);
            if(br)
            {
                memcpy(buf, &_vector->data()[_vpos], br);
            }
            _vpos += br;
            l = (int64_t)br;
        }

        // check for end of data
        if (l == 0) {
            end = 1;
            seterror(E_END_OF_DATA);
            cur_bit = pos & 7;
            return;
        }
        else if (l < 0) {
            end = 1;
            seterror(E_READ_FAILED);
            return;
        }
        else if (l < BS_BUF_LEN) {
            end = 1;
            buf_len = l;
        }

        cur_bit = pos & 7;
    }
    else
    {
        // flush and seek
        flushbits();

        // clear the buffer
        memset(buf, 0, BS_BUF_LEN);

        if(_output_device)
        {
            if(!_output_device->seekp(pos >> BSHIFT))
            {
                seterror(E_SEEK_FAILED);
                return;
            }
        }
        else
        {
            _vpos = pos >> BSHIFT;
        }
        cur_bit = pos & 7;
    }
}

int64_t QBitstream::tell()
{
    if(!canSeek())
        return -1;

    if(_type == BS_INPUT)
    {
        if(_input_device)
        {
            return _input_device->tellg() * 8 - ((buf_len << BSHIFT) - cur_bit);
        }
        else
        {
            return _vpos * 8 - ((buf_len << BSHIFT) - cur_bit);
        }
    }

    if(_output_device)
    {
        return _output_device->tellp() * 8 + cur_bit;
    }
    else
    {
        return _vpos * 8 + cur_bit;
    }
}

bool QBitstream::eof()
{
    int	n;	// how many bytes we must fetch (already read)
    int	u;	// how many are still unread

    n = (cur_bit >> BSHIFT);
    u = buf_len - n;
    if(_output_device) {
        return _output_device->eof() && !u;
    } else if(_vector && _type == BS_OUTPUT) {
        // we can go on and on in vector output mode
        return false;
    } else if(_vector && _type == BS_INPUT) {
        return _vpos < _vector->size();
    }
    return _input_device->eof() && !u;
}

///////////////////
// Little endian //
///////////////////

// returns 'n' bits as unsigned int; does not advance bit pointer
uint64_t QBitstream::little_nextbits(int n)
{
    uint64_t x = 0;    // the value we will return
    int bytes = n >> BSHIFT;             // number of bytes to read
    int leftbits = n % 8;           // number of left-over bits to read
    uint64_t byte_x = 0;
    int i = 0;
    for (; i < bytes; i++) {
        byte_x = nextbits(8);
        cur_bit += 8;
        byte_x <<= (8*i);
        x |= byte_x;
    }

     // Note that it doesn't make much sense to have a number in little-endian
     // byte-ordering, where the number of bits used to represent the number is
     // not a multiple of 8.  Neverthless, we provide a way to take care of
     // such case.
    if (leftbits > 0) {
        byte_x = nextbits(leftbits);
        byte_x <<= (8*i);
        x |= byte_x;
    }
    cur_bit -= i*8;
    return x;
}

// returns 'n' bits as unsigned int with sign extension; does not advance bit pointer (sign extension only if n>1)
uint64_t QBitstream::little_snextbits(int n)
{
    uint64_t x = little_nextbits(n);
    if (n>1 && (x & smask[n])) return x | cmask[n];
    else return x;
}

// returns 'n' bits as unsigned int; advances bit pointer
uint64_t QBitstream::little_getbits(int n)
{
    uint64_t x = 0;             // the value we will return
    int bytes = n >> BSHIFT;    // number of bytes to read
    int leftbits = n % 8;       // number of bits to read
    uint64_t byte_x = 0;
    int i = 0;
    for (; i < bytes; i++) {
        byte_x = getbits(8);
        byte_x <<= (8*i);
        x |= byte_x;
    }
    if (leftbits > 0) {
        byte_x = getbits(leftbits);
        byte_x <<= (8*i);
        x |= byte_x;
    }
    return x;
}

// returns 'n' bits as unsigned int with sign extension; advances bit pointer (sign extension only if n>1)
uint64_t QBitstream::little_sgetbits(int n)
{
    uint64_t x = little_getbits(n);
    if (n>1 && (x & smask[n])) return x | cmask[n];
    else return x;
}

// probe a float
float QBitstream::little_nextfloat(void)
{
    float f;
    *(int *)&f = little_nextbits(32);
    return f;
}

// get a float
float QBitstream::little_getfloat(void)
{
    float f;
    *(int *)&f = little_getbits(32);
    return f;
}

// probe a double
double QBitstream::little_nextdouble(void)
{
    double d;
    // make sure we have enough data (so that we can go back)
    if (cur_bit + 64 > (buf_len << BSHIFT)) fill_buf();
    *(int *)&d = little_nextbits(32);
    cur_bit += 32;
    *(((int *)&d)+1) = little_nextbits(32);
    cur_bit -= 32;
    return d;
}

// get a double
double QBitstream::little_getdouble(void)
{
    double d;
    *(int *)&d = little_getbits(32);
    *(((int *)&d)+1) = little_getbits(32);
    return d;
}

// can only write at least one byte to a file at a time; returns the output value
int QBitstream::little_putbits(uint64_t value, int n)
{
    int bytes = n >> BSHIFT;         // number of bytes to write
    int leftbits = n % 8;           // number of bits to write
    uint64_t byte_x = 0;
    int i = 0;
    for (; i < bytes; i++) {
        byte_x = (value >> (8*i)) & mask[8];
        putbits(byte_x, 8);
    }
    if (leftbits > 0) {
        byte_x = (value >> (8*i)) & mask[leftbits];
        putbits(byte_x, leftbits);
    }
    return value;
}

// put a float
float QBitstream::little_putfloat(float value)
{
    little_putbits(*((int *)&value), 32);
    return value;
}

// put a double
double QBitstream::little_putdouble(double value)
{
    little_putbits(*(int *)&value, 32);
    little_putbits(*(((int *)&value)+1), 32);
    return value;
}

// advance by some bits ignoring the value
void QBitstream::skipbits(int n)
{
     int x = n;
     int buf_size = buf_len << BSHIFT;

    // make sure we have enough data
    while (cur_bit + x > buf_size) {
        x -= buf_size - cur_bit;
        cur_bit = buf_size;
        if (_type == BS_INPUT) fill_buf();
        else flush_buf();
    }
    cur_bit += x;
    tot_bits += n;
    return;
}

// align bitstream - returns the number of bits skipped to reach alignment
int QBitstream::align(int n)
{
    int s = 0;
    // we only allow alignment on multiples of bytes
    if (n % 8) {
        seterror(E_INVALID_ALIGNMENT);
        return 0;
    }
    // align on next byte
    if (tot_bits % 8) {
        s = 8-(tot_bits % 8);
        skipbits(s);
    }
    while (tot_bits % n) {
        s += 8;
        skipbits(8);
    }
    return s;
}

//Probe next n bits (input) or return 0 (output); in either case, the bitstream is (alen-bit) aligned.
//If big=0, then the number is represented using the little-endian method; otherwise, big-endian byte-ordering.
//If sign=0, then no sign extension; otherwise, sign extension.
//If alen>0, then the number is probed at alen-bit boundary (alen must be multiple of 8).
uint64_t QBitstream::next(int n,  int big, int sign, int alen)
{
    if (alen > 0) align(alen);
    if (_type == BS_INPUT) {
        if (big) {
            if (sign) return snextbits(n);
            else return nextbits(n);
        }
        else {
            if (sign) return little_snextbits(n);
            else return little_nextbits(n);
        }
    }
    else return 0;
}

//Search for a specified code (input); returns number of bits skipped, excluding the code.
//If alen > 0, then output bits up to the specified alen-bit boundary (output); returns number of bits written.
//The code is represented using n bits at alen-bit boundary.
uint64_t QBitstream::nextcode(uint64_t code, int n, int alen)
{
    uint64_t s = 0;

    if (_type == BS_INPUT) {
        if (!alen) {
            while (code != nextbits(n)) {
                if(err_code != E_NONE)
                    break;
                s += 1;
                skipbits(1);
            }
        }
        else {
            s += align(alen);
            while (code != nextbits(n)) {
                if(err_code != E_NONE)
                    break;
                s += alen;
                skipbits(alen);
            }
        }
    }
    else if (_type == BS_OUTPUT) s += align(alen);

    return s;
}

// flush buffer; left-over bits are also output with zero padding
void QBitstream::flushbits()
{
    flush_buf();

    if (cur_bit == 0) return;

    if(_output_device)
    {
        try {
            _output_device->write((const char *)buf, 1);
        }
        catch(std::ostream::failure &writeErr) {
            seterror(E_WRITE_FAILED);
            return;
        }
    }
    else
    {
        (*_vector)[_vpos] = (uint8_t)buf[0];
        _vpos++;
    }

    buf[0] = 0;
    cur_bit = 0;    // now only the left-over bits
}

// get the next chunk of data from whatever the source is
void QBitstream::fill_buf()
{
    int	n;	// how many bytes we must fetch (already read)
    int	l;	// how many bytes we will fetch (available)
    int	u;	// how many are still unread

    n = (cur_bit >> BSHIFT);
    u = buf_len - n;

    // move unread contents to the beginning of the buffer
    if(u)
    {
        memmove(buf, buf+n, u);
    }

    // clear the rest of buf
    memset(buf + u, 0, n);

    if(_input_device)
    {
        l = _input_device->readsome((char *)(buf + u), n);
    }
    else
    {
        size_t br = std::min((size_t)(_vector->size() - _vpos), (size_t)n);
        if(br)
        {
            memcpy((char *)(buf + u), &_vector->data()[_vpos], br);
        }
        _vpos += br;
        l = (int64_t)br;
    }

    // check for end of data
    if (l == 0) {
        end = 1;
        seterror(E_END_OF_DATA);
        cur_bit &= 7;
        return;
    }
    else if (l < 0) {
        end = 1;
        seterror(E_READ_FAILED);
        return;
    }
    else if (l < n) {
        end = 1;
        buf_len = u+l;
    }
    // now we are at the first byte
    cur_bit &= 7;
}

// output the buffer excluding the left-over bits.
void QBitstream::flush_buf()
{
    int l = (cur_bit >> BSHIFT);     // number of bytes written already

    if(_output_device)
    {
        try {
            _output_device->write((const char *)buf, l);
        }
        catch(std::ostream::failure &writeErr) {
            seterror(E_WRITE_FAILED);
            return;
        }
    }
    else
    {
        for(int i = 0; i < l; i++)
        {
            (*_vector)[_vpos] = (uint8_t)buf[i];
            _vpos += 1;
        }
    }

    // are there any left-over bits?
    if (cur_bit & 0x7) {
        buf[0] = buf[l];                // copy the left-over bits
        memset(buf+1, 0, BS_BUF_LEN-1); // zero-out rest of buffer
    }
    else memset(buf, 0, BS_BUF_LEN);    // zero-out entire buffer
    // keep left-over bits only
    cur_bit &= 7;
}
