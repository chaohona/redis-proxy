#ifndef _GR_SENTINEL_EVENT_H__
#define _GR_SENTINEL_EVENT_H__

#include <list>
#include "redisevent.h"
#include "twemroute.h"

using namespace std;

class GR_SentinelEvent: public GR_RedisEvent
{
public:
    GR_SentinelEvent(int iFD, sockaddr &sa, socklen_t &salen);
    GR_SentinelEvent(GR_RedisServer *pServer);
    ~GR_SentinelEvent();
public:
    virtual int GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    virtual int ConnectResult();
    virtual int Close(bool bForceClose=false);
    virtual int ReConnect(GR_TimerMeta *pMeta);
};

class GR_SentinelMgr
{
public:
    GR_SentinelMgr(GR_TwemRoute *pRoute);
    ~GR_SentinelMgr();

public:
    // 和sentinel建立连接
    int ConnectToSentinel();
    int SentinelConnected(GR_SentinelEvent *pEvent);

private:
    GR_TwemRoute *m_pRoute = nullptr;
    int          m_iInitFlag = 0;
};

#endif
