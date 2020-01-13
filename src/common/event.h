#ifndef _GR_EVENT_H__
#define _GR_EVENT_H__

#include "define.h"
#include "include.h"
#include "sys/epoll.h"
#include "mempool.h"
#include "redismsg.h"
#include "gr_timer.h"

#define MAX_EVENT_POOLS 2048

#define GR_EVENT_UN_ACTIVE_MS 1000*60*30    // 30分钟

#define GR_EVNET_NONE     0       /* No events registered. */
#define GR_EVNET_READABLE 1   /* Fire when descriptor is readable. */
#define GR_EVENT_WRITABLE 2   /* Fire when descriptor is writable. */
#define GR_EVENT_RW       GR_EVNET_READABLE | GR_EVENT_WRITABLE
#define GR_EVENT_ERROR    4

#define GR_MAX_ACCEPTS_PER_CALL 1000 // 每次最多接收1000个连接

enum
GR_EVENT_TYPE{
  GR_LISTEN_EVENT,
  GR_REDIS_EVENT,
  GR_CLIENT_EVENT,
  GR_TIMER_EVENT,
};

class GR_Event;
class GR_MsgIdenty;

struct GR_EventFlag
{
    GR_Event    *pEvent;
    uint32      uiEventId;
};

enum GR_CONNECT_STATUS{
    GR_CONNECT_INIT,
    GR_CONNECT_CONNECTING,
    GR_CONNECT_CONNECTED,
    GR_CONNECT_CLOSED
};

// 代理的事件类
class GR_Event
{
public:
    GR_Event(int iFD);
    GR_Event();
    virtual ~GR_Event();

    bool Reset();
    bool operator == (const GR_Event& event);
public:
    virtual int AddToTimer();
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();
    virtual int GetReply(int iErr, GR_MsgIdenty *pIdenty);
    virtual int GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    virtual int DoReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    int DoSendv(iovec *iov, int iovcnt);

    int ConnectCheck();
    bool ConnectOK();
public:
    uint16                  m_uiPort = 0;
    char                    m_szAddr[NET_IP_STR_LEN];

public:
    static uint32   global_event_id;    // 事件自增id
    uint32          m_uiEventId = 0;        // 本事件id
    int             m_iFD = 0;              // 事件句柄
    int             m_iMask = GR_EVNET_NONE;            // 被触发的事件类型
    GR_CONNECT_STATUS       m_Status = GR_CONNECT_INIT;
    GR_TimerMeta*          m_pTimerMeta = nullptr;       // 定时任务索引
    uint64          m_ulActiveMS = 0;       // 活跃时间
    uint64          m_ulNextTry = 0;
    uint64          m_ulMaxTryConnect = 0;
    GR_EVENT_TYPE   m_eEventType;
};

class GR_Epoll
{
public:
    ~GR_Epoll();
    static GR_Epoll *Instance();

    bool Init(int iEventNum);
    int AddEventRead(GR_Event *pEvent);         // 增加事件读触发
    int AddEventWrite(GR_Event *pEvent);        // 增加事件写触发
    int DelEventRead(GR_Event *pEvent);         //  删除事件读触发
    int DelEventWrite(GR_Event *pEvent);        // 删除事件写触发
    int AddEventRW(GR_Event *pEvent);           // 增加读写
    int DelEventRW(GR_Event *pEvent);           // 删除读写

    int EventLoopProcess(uint64 ulMS);
    int ProcEvent(GR_EVENT_TYPE iType);
    int ProcEventNotType(GR_EVENT_TYPE iType);
    int ProcAllEvents();
    int DelEvent(int iFD);
private:
    GR_Epoll();
private:
    int AddEvent(GR_Event *pEvent, int iMask);// 根据iMask增加触发
    int DelEvent(GR_Event *pEvent, int iMask);// 根据iMask删除触发
    inline GR_Event *GetExistEvent(int iFD);
    GR_Event* SetEvent(GR_Event *pEvent);

public:
    static GR_Epoll* m_pInstance;
    int         m_iEpFd = 0;            // epoll句柄
    epoll_event *m_aEpollEvents;    // 接收系统的事件数据
    GR_Event    **m_aFiredEvents;   // epoll被触发事件缓存,保存被触发的事件的指针
    int         m_iEventNum = 0;        // epoll被触发事件缓存数组大小
    int         m_iNumEventsActive;

    GR_Event    **m_aNetEvents;                         // 管理的事件缓存，以fd作为下标
    unordered_map<int, GR_Event*>   m_mapNetEvents;     //  句柄大于0xFFF的时候存这儿
};


#define GR_EPOLL_INSTANCE()\
GR_Epoll::m_pInstance

#define GR_EPOLL_DELEVENT(pEVENT, iMASK)                                                \
{                                                                                       \
    if (pEVENT->m_iFD>0)                                                                \
    {                                                                                   \
        struct epoll_event ee = {0};                                                    \
        pEVENT->m_iMask = pEVENT->m_iMask & (~iMASK);                                   \
        ee.events = 0;                                                                  \
        if (pEVENT->m_iMask & GR_EVNET_READABLE) ee.events |= EPOLLIN;                  \
        if (pEVENT->m_iMask & GR_EVENT_WRITABLE) ee.events |= EPOLLOUT;                 \
        ee.data.ptr = pEVENT;                                                           \
        if (pEVENT->m_iMask != GR_EVNET_NONE) {                                         \
            epoll_ctl(GR_Epoll::m_pInstance->m_iEpFd,EPOLL_CTL_MOD,pEVENT->m_iFD,&ee);  \
        } else {                                                                        \
            epoll_ctl(GR_Epoll::m_pInstance->m_iEpFd,EPOLL_CTL_DEL,pEVENT->m_iFD,&ee);  \
        }                                                                               \
    }                                                                                   \
}

#define GR_EPOLL_ADDEVENT(pEVENT, iMASK)                                                \
{                                                                                       \
    if (pEVENT->m_iFD>0)                                                                \
    {                                                                                   \
        epoll_event ee = {0};                                                           \
        int op = pEVENT->m_iMask == GR_EVNET_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;      \
        pEVENT->m_iMask |= iMASK;                                                       \
        if (pEVENT->m_iMask & GR_EVNET_READABLE) {                                      \
            ee.events |= (uint32_t)(EPOLLIN);                                           \
        }                                                                               \
        if (pEVENT->m_iMask & GR_EVENT_WRITABLE) {                                      \
            ee.events |= (uint32_t)(EPOLLIN | EPOLLOUT);                                \
        }                                                                               \
        ee.data.ptr = pEVENT;                                                           \
        epoll_ctl(GR_Epoll::m_pInstance->m_iEpFd, op, pEVENT->m_iFD, &ee);       \
    }                                                                                   \
}



#define GR_EPOLL_ADDREAD(pEvent)\
GR_EPOLL_ADDEVENT(pEvent, GR_EVNET_READABLE);

#define GR_EPOLL_ADDWRITE(pEvent)\
GR_EPOLL_ADDEVENT(pEvent, GR_EVENT_WRITABLE);

#define GR_EPOLL_DELREAD(pEvent)\
GR_EPOLL_DELEVENT(pEvent, GR_EVNET_READABLE);
    
#define GR_EPOLL_DELWRITE(pEvent)\
GR_EPOLL_DELEVENT(pEvent, GR_EVENT_WRITABLE);


#endif
