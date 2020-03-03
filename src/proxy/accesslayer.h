#ifndef _GR_ACCESS_LAYER_H__
#define _GR_ACCESS_LAYER_H__

#include "define.h"
#include "event.h"
#include "listenevent.h"
#include "socket.h"
#include "config.h"
#include "mempool.h"
#include "ringbuffer.h"
#include "redismsg.h"
#include "msgbuffer.h"
#include "gr_proxy_global.h"

class GR_AccessMgr;
class GR_Route;
class GR_RouteGroup;

// TODO 内存池怎么设计，是使用拷贝的方式，还是使用twen的零内存拷贝方式，需要测试
class GR_AccessEvent : public GR_Event
{
public:
    GR_AccessEvent(int iFD, sockaddr &sa, socklen_t &salen, GR_AccessMgr* pMgr);
    GR_AccessEvent();
    virtual ~GR_AccessEvent();

    // 发生错误
    virtual int GetReply(int iErr, GR_MsgIdenty *pIdenty);
    virtual int GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    int DoReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    int ReInit();
    int SetAddr(int iFD, sockaddr &sa, socklen_t &salen);
    int ProcPendingMsg(bool &bFinish);
    int Init();
public:
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();

private:
    int Sendv(iovec * iov, int nsend);
    // 开始解析下一条
    int StartNext(bool bRealseReq = true);
    int ProcessMsg(int &iNum);
    int ClusterReqMsg(); // 集群模式

public:
    GR_MsgBufferMgr     m_ReadCache; // 客户端读消息缓存初始化大小为128K
    GR_RedisMsg         m_ReadMsg;  // 消息解析器

    RingBuffer          *m_pWaitRing = nullptr;
    iovec               *m_vCiov = nullptr;
    uint8               m_cPendingFlag = 0;  // 阻塞标记
    int                 m_iPendingIndex = 0;// 在管理列表中的下标
    uint64              m_ulPendingTime = 0;// 被阻塞时间，超过一定时间直接断开客户端连接

    uint64              m_ulMsgIdenty = 0;

    GR_AccessMgr        *m_pMgr = nullptr;
    uint64              m_ulIdx = 0;
    GR_Route            *m_pRoute = nullptr;
};

// 代理的接入层管理，管理代理和前端的通信
class GR_AccessMgr : public GR_ListenMgr
{
public:
    int Init(GR_Route  *pRoute);
public:
    GR_AccessMgr();
    virtual ~GR_AccessMgr();

    virtual int Close();

    void ReleaseEvent(GR_AccessEvent *pEvent);
    int EnableAcceptEvent(bool &bSuccess); // 尝试将accept加入事件触发队列中
    void DisableAcceptEvent();
    virtual GR_Event* CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen);
    virtual GR_ListenEvent* CreateListenEvent();
    int ProcPendingEvents(); // 处理读事件
    int AddPendingEvent(GR_AccessEvent *pEvent);
    int DelPendingEvent(GR_AccessEvent *pEvent);
    int LoopCheck();
public:
    uint64                  m_ulLockValue = 0;
    int                     m_iAcceptDisabled = 0;

    unordered_map<uint64, GR_AccessEvent*> m_mapAccess;
    uint64              m_ulAccessIdx = 0;
    int                 m_iPendingFlag = 0;
    GR_AccessEvent**    m_pPendingClients = nullptr; // 有阻塞的数据没有发送出去的客户端
    GR_ProxyShareInfo*  m_pgShareInfo = nullptr;
    GR_Route            *m_pRoute = nullptr;
};

#endif
