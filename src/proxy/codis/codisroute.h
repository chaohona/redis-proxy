#ifndef _GR_CODIS_ROUTE_H__
#define _GR_CODIS_ROUTE_H__

#include "route.h"

class GR_CodisRoute: public GR_Route
{
public:
    GR_CodisRoute();
    virtual ~GR_CodisRoute();

    virtual int Init(const char *szArg);

    virtual GR_RedisEvent* Route(GR_RedisEvent* vEventList, int iNum, GR_AccessEvent *pEvent);
};

#endif
