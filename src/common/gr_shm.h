#ifndef _GR_SHM_H__
#define _GR_SHM_H__

#include "include.h"

class GR_Shm
{
public:
    GR_Shm();
    ~GR_Shm();

    int Alloc(size_t size);
    int ShmAt();
    void Free();
public:
    char *m_szAddr;
    size_t m_iSize;
    string m_strName;
    int m_iShmId;
};

#endif
