#ifndef _GR_STORE_LISTEN_H__
#define _GR_STORE_LISTEN_H__

#include "define.h"
#include "event.h"
#include "listenevent.h"
#include "socket.h"
#include "config.h"


// 代理的接入层管理，管理代理和前端的通信
class GR_StoreListenMgr : public GR_ListenMgr
{
public:
    int Init(GR_Route  *pRoute);
public:
    GR_StoreListenMgr();
    virtual ~GR_StoreListenMgr();

    virtual int Close();

    void ReleaseEvent(GR_AccessEvent *pEvent);
    virtual GR_Event* CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen);
    virtual GR_ListenEvent* CreateListenEvent();
    int LoopCheck();
public:
    uint64                  m_ulLockValue = 0;
    int                     m_iAcceptDisabled = 0;

    unordered_map<uint64, GR_AccessEvent*> m_mapAccess;
    uint64              m_ulAccessIdx = 0;
};


#endif
