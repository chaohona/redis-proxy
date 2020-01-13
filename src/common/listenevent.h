#ifndef _GR_LISTEN_EVENT_H__
#define _GR_LISTEN_EVENT_H__

#include "include.h"
#include "event.h"
#include "clientevent.h"

class GR_ListenEvent;

// 服务管理类
class GR_ListenMgr
{
public:
    GR_ListenMgr();
    virtual ~GR_ListenMgr();

public:
    virtual int Accept(int iFD);
    virtual int Listen(string strIP, uint16 usPort, int iTcpBack, bool bSystemBa=false);
    virtual GR_Event* CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen);
    virtual GR_ListenEvent* CreateListenEvent();

public:
    int                 m_iListenFD = 0;    // 监听端口句柄
    GR_ListenEvent      *m_pListenEvent = nullptr;// 监听事件管理器

    int                 m_iClientNum = 0;
};

// 监听事件处理器
class GR_ListenEvent : public GR_Event
{
public:
    GR_ListenEvent();
    GR_ListenEvent(GR_ListenMgr *mListenMgr);
    virtual ~GR_ListenEvent();
public:
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();

private:
    GR_ListenMgr *m_pListenMgr;
};

#endif
