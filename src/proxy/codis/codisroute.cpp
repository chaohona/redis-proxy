#include "codisroute.h"

GR_CodisRoute::GR_CodisRoute()
{
}

GR_CodisRoute::~GR_CodisRoute()
{
}

int GR_CodisRoute::Init(const char *szArg)
{
    return GR_OK;
}

GR_RedisEvent* GR_CodisRoute::Route(GR_RedisEvent* vEventList, int iNum, GR_AccessEvent *pEvent)
{
    return nullptr;
}


