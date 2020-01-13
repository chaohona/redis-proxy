#ifndef _GR_CLUSTER_ROUTE_H__
#define _GR_CLUSTER_ROUTE_H__
#include "include.h"
#include "redisevent.h"
#include "route.h"

#define CLUSTER_SLOTS 16384
#define CLUSTER_OK 0          /* Everything looks ok */
#define CLUSTER_FAIL 1        /* The cluster can't work */
#define CLUSTER_NAMELEN 40    /* sha1 hex length */
#define CLUSTER_PORT_INCR 10000 /* Cluster port = baseport + PORT_INCR */

/* Cluster node flags and macros. */
#define CLUSTER_NODE_MASTER 1     /* The node is a master */
#define CLUSTER_NODE_SLAVE 2      /* The node is a slave */
#define CLUSTER_NODE_PFAIL 4      /* Failure? Need acknowledge */
#define CLUSTER_NODE_FAIL 8       /* The node is believed to be malfunctioning */
#define CLUSTER_NODE_MYSELF 16    /* This node is myself */
#define CLUSTER_NODE_HANDSHAKE 32 /* We have still to exchange the first ping */
#define CLUSTER_NODE_NOADDR   64  /* We don't know the address of this node */
#define CLUSTER_NODE_MEET 128     /* Send a MEET message to this node */
#define CLUSTER_NODE_MIGRATE_TO 256 /* Master elegible for replica migration. */
#define CLUSTER_NODE_NOFAILOVER 512 /* Slave will not try to failver. */

class GR_ClusterRoute;

class GR_ClusterInfo
{
public:
    struct Slots
    {
        int iBegin = -1;
        int iEnd = -1;
    };
public:
    char            *szClusterID = nullptr;
    char            *szIP = nullptr;
    uint16          usPort = 0;
    int             iFlags = 0;
    list<Slots>     listSlots;
};

class GR_ClusterSeedEvent: public GR_RedisEvent
{
public:
    GR_ClusterSeedEvent();
    virtual ~GR_ClusterSeedEvent();
public:
    virtual int GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
    virtual int ConnectResult();
    virtual int Close(bool bForceClose);
    virtual int ReConnect(GR_TimerMeta *pMeta);
    int ReqClusterInfo(); // 请求集群信息

private:
    int ParseClusterNodes(GR_RedisMsgResults &Results);
    GR_ClusterInfo GetClusterInfo(vector<GR_String> &vNodeData, int &iResult);
public:
    GR_ClusterRoute *m_pRoute = nullptr;
};


class GR_ClusterRoute: public GR_Route
{
public:
    GR_ClusterRoute();
    virtual ~GR_ClusterRoute();

    virtual int Init(GR_Config *pConfig);
    virtual int ReInit();
    int ReInitFinish();
    virtual GR_RedisEvent* Route(char *szKey, int iLen, int &iError);
    virtual GR_RedisEvent* Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError);
    int SlotRedirect(GR_MsgIdenty *pIdenty, GR_RedisMsg &msg, bool bAsking = false);
    int SlotChanged(GR_MsgIdenty *pIdenty, GR_RedisEvent *pEvent);
    int AddRedisServer(GR_ClusterInfo &info, GR_RedisServer*& pServer);

    int ExpandStart();
    int ReadyCheck(bool &bReady);
    virtual int DelRedis(GR_RedisEvent *pEvent);
public:
    int ParseNativeClusterCfg(string &strClusterCfg);
    uint32 KeyHashSlot(char *szKey, int iKeyLen);

public:
    int ConnectToSeedRedis();
    int m_iReInitFlag = 0;                                  // 和Redis断线重连标记，防止和多个Redis同时断开，同时触发多次重连
    
    GR_RedisServer          *m_vRedisSlot[CLUSTER_SLOTS];   // 分片和Redis的映射关系
    
    char                    m_szSeedIP[NET_IP_STR_LEN];     // SeedRedis的IP
    uint16                  m_usPort = 0;                   // SeedRedis的Port
    GR_ClusterSeedEvent     *m_pClusterSeedRedis = nullptr; // 用来获取集群信息的Redis
    int64                   m_lNextGetClustesMS = 0;
};

#endif
