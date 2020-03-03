#ifndef _GR_REDIS_MGR_H__
#define _GR_REDIS_MGR_H__

#include "accesslayer.h"
#include "redisevent.h"
#include "route.h"
#include "sentinelevent.h"
#include "replicaevent.h"

#define GR_REDISMSG_INSTANCE()\
GR_RedisMgr::m_pInstance

#define GR_ROUTE_GROUP()\
GR_RedisMgr::m_pInstance->m_pRouteGroup


class GR_RedisMgr
{
public:    
    ~GR_RedisMgr();
    static GR_RedisMgr* Instance();
    // 根据配置连接后端的redis
    int Init(GR_Config *pConfig);
    int TransferMsgToRedis(GR_AccessEvent *pEvent, GR_MsgIdenty *pIdenty);
    int ReplicateMsgToRedis(GR_ReplicaEvent *pEvent, GR_MsgIdenty *pIdenty);
    int RedisConnected(uint16 uiPort, char *szAddr);// 和后端redis建立连接成功
    int SentinelConnected(GR_SentinelEvent *pEvent);
    int LoopCheck();

    
    GR_RouteGroup       *m_pRouteGroup = nullptr;
    static GR_RedisMgr  *m_pInstance;
private:
    GR_RedisMgr();
    int TwemConnectToRedis();

    int                 m_iConnectedNum;    // 已经连接上的redis总数
    GR_RedisEvent       *m_pTempRedis;

    GR_SentinelMgr      *m_pSentinelMgr;
    GR_Config           *m_pConfig;
};

#endif
