#ifndef _GR_STORE_CLIENT_H__
#define _GR_STORE_CLIENT_H__

#include "event.h"

class GR_StoreClient: public GR_Event
{
public:
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();
}

#endif
