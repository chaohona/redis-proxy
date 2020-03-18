#ifndef _GR_REDIS_EVENT_H__
#define _GR_REDIS_EVENT_H__

#include "include.h"
#include "event.h"
#include "mempool.h"
#include "accesslayer.h"
#include "redismsg.h"

#define MAX_REDIS_MSG_POOL  1024

class GR_RedisServer;
// 和redis的连接事件
class GR_RedisEvent : public GR_Event
{
public:
    GR_RedisEvent(int iFD, sockaddr &sa, socklen_t &salen);
    GR_RedisEvent(GR_RedisServer *pServer);
    GR_RedisEvent();
    virtual ~GR_RedisEvent();

    // 和redis建立连接
    int ConnectToRedis(uint16 uiPort, const char *szIP);
    // 连接建立成功的后处理
    virtual int ConnectResult();
    virtual int ReConnect(GR_TimerMeta *pMeta=nullptr);
    virtual int ConnectSuccess();
    virtual int ConnectFailed();
    // 向redis转发前端过来的消息
    int SendMsg(GR_RedisMsg *pMsg, GR_MsgIdenty *pIdenty);
    int SendMsg(const char *szData, int iLen, GR_MsgIdenty *pIdenty);
    int SendMsg(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    int TTCheck();
public:
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close(bool bForceClose = false);
    virtual int ResultCheck(GR_MsgIdenty* pIdenty);

private:
    int ProcessMsg(int &iNum);
    int StartNext(bool bReleaseData = false);
    int Init();
    int ReInit();
    bool SlotPending(GR_MsgIdenty *pIdenty);
    int Sendv(iovec * iov, int nsend);

public:
    GR_MsgBufferMgr         m_ReadCache;        // 客户端读消息缓存初始化大小为1K
    iovec                   *m_vCiov = nullptr;
    uint64                  m_lCiovReadIdx = 0;
    uint64                  m_lCiovWriteIdx = 0;
    GR_RedisMsg             m_ReadMsg;          // 消息解析器

    RingBuffer              *m_pWaitRing = nullptr;
    GR_RedisServer          *m_pServer = nullptr;
    GR_REDIS_TYPE           m_iRedisType = REDIS_TYPE_NONE;
    uint64                  m_ulFirstMsgMS = 0; // 最早的等待redis响应的消息的发送时间
    int                     m_iRedisRspTT = 200;
    GR_TimerMeta            *pConnectingMeta = nullptr;
    GR_Route                *m_pRoute = nullptr;
    uint64                  m_ulSendMsgNum = 0;
    uint64                  m_ulRecvMsgNum = 0;
};

class GR_RedisServer
{
public:
    GR_RedisServer();
    virtual ~GR_RedisServer();

    virtual int Connect(GR_Route *pRoute);
    string  strSentinelName = string("");       // 在sentinel里面的名字
    string  strInfo = string("");               /* server: as "hostname:port:weight name" */
    string  strPName = string("");              /* server: as "hostname:port:weight" */
    string  strAddr = string("");               // hostname:port
    string  strHostname = string("");           // hostname   ip地址
    string  strName = string("");               /* name */
    int     iPort = 0;                          /* port */
    int     iWeight = 0;                        /* weight */
    int     index = 0;
    int     value = 0;
    uint64  ulIdenty = 0;                       // ip:port转换成的长整数
    uint64  ulIdx = 0;                          // 这组redis的唯一标识符
    int     iInPool = 0;                        // 是否在路由池子中提供服务
    
    GR_RedisEvent *pEvent = nullptr;
    bool operator == (GR_RedisServer &other);
    GR_RedisServer *m_vSlaves[MAX_REDIS_SLAVES]; // 从通过cluster slots获取
};
#endif
