#ifndef _GR_CLIENT_EVENT_H__
#define _GR_CLIENT_EVENT_H__

#include "include.h"
#include "event.h"
#include "mempool.h"

// 客户端管理类,管理自己作为服务器连接到自己的客户端
class GR_ClientEvent : public GR_Event
{
public:
    GR_ClientEvent(int iFD, sockaddr &sa, socklen_t &salen);
    GR_ClientEvent();
    virtual ~GR_ClientEvent();

    int SetInfo();
public:
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();

public:
    int     m_iPort;
    char    m_szIp[NET_IP_STR_LEN];

private:
    int     m_iValidClient:1; // 是否是有效的客户端

    GR_MemPoolData *m_pReadMemData; // 客户端读消息缓存初始化大小为128K
    GR_MemPoolData *m_pWriteMemData;// 客户端写消息缓存初始化大小为128K
};

#endif
