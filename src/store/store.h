#ifndef _GR_STORE_H__
#define _GR_STORE_H__

#include "define.h"


class GR_Store
{
public:
    ~GR_Store();
    static GR_Store* Instance();

    int Init();
private:
    GR_Store();
    static GR_Store *m_pInstance;
};

#endif
