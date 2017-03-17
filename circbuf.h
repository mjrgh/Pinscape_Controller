// Circular buffer for incoming reports.  We write reports in the IRQ
// handler, and we read reports in the main loop in normal application
// (non-IRQ) context.  
// 
// The design is organically safe for IRQ threading; there are no critical 
// sections.  The IRQ context has exclusive access to the write pointer, 
// and the application context has exclusive access to the read pointer, 
// so there are no test-and-set or read-and-modify race conditions.

#ifndef _CIRCBUF_H_
#define _CIRCBUF_H_

// Circular buffer with a fixed buffer size
template<class T, int cnt> class CircBuf
{
public:
    CircBuf() 
    {
        iRead = iWrite = 0;
    }

    // Read an item from the buffer.  Returns true if an item was available,
    // false if the buffer was empty.  (Called in the main loop, in application
    // context.)
    bool read(T &result) 
    {
        if (iRead != iWrite)
        {
            //{uint8_t *d = buf[iRead].data; printf("circ read [%02x %02x %02x %02x %02x %02x %02x %02x]\r\n", d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7]);}
            memcpy(&result, &buf[iRead], sizeof(T));
            iRead = advance(iRead);
            return true;
        }
        else
            return false;
    }
    
    // is an item ready to read?
    bool readReady() const { return iRead != iWrite; }
    
    // Write an item to the buffer.  (Called in the IRQ handler, in interrupt
    // context.)
    bool write(const T &item)
    {
        int nxt = advance(iWrite);
        if (nxt != iRead)
        {
            memcpy(&buf[iWrite], &item, sizeof(T));
            iWrite = nxt;
            return true;
        }
        else
            return false;
    }
    
private:
    int advance(int i)
    {
        ++i;
        return i < cnt ? i : 0;
    } 
    
    int iRead;
    int iWrite;
    T buf[cnt];
};

// Circular buffer with a variable buffer size
template<class T> class CircBufV
{
public:
    CircBufV(int cnt)
    {
        buf = new T[cnt];
        this->cnt = cnt;
        iRead = iWrite = 0;
    }
    
    ~CircBufV()
    {
        delete[] buf;
    }

    // Read an item from the buffer.  Returns true if an item was available,
    // false if the buffer was empty.  (Called in the main loop, in application
    // context.)
    bool read(T &result) 
    {
        if (iRead != iWrite)
        {
            //{uint8_t *d = buf[iRead].data; printf("circ read [%02x %02x %02x %02x %02x %02x %02x %02x]\r\n", d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7]);}
            memcpy(&result, &buf[iRead], sizeof(T));
            iRead = advance(iRead);
            return true;
        }
        else
            return false;
    }
    
    // is an item ready to read?
    bool readReady() const { return iRead != iWrite; }
    
    // Write an item to the buffer.  (Called in the IRQ handler, in interrupt
    // context.)
    bool write(const T &item)
    {
        int nxt = advance(iWrite);
        if (nxt != iRead)
        {
            memcpy(&buf[iWrite], &item, sizeof(T));
            iWrite = nxt;
            return true;
        }
        else
            return false;
    }

private:
    int advance(int i)
    {
        ++i;
        return i < cnt ? i : 0;
    } 
    
    int iRead;
    int iWrite;
    int cnt;
    T *buf;
};

#endif
