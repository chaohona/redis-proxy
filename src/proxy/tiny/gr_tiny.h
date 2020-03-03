#ifndef _GR_TINY_H__
#define _GR_TINY_H__

#include "route.h"
#include "dispatch.h"
#include "hash.h"
#include "redismsg.h"
#include "redisevent.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <yaml-cpp/yaml.h>

class GR_TinyRedisServer: public GR_RedisServer
{
public:
    GR_TinyRedisServer();
    virtual ~GR_TinyRedisServer();

    virtual int Connect(GR_Route *pRoute);

    int m_iDB = 0;
};

class GR_TinyRedisEvent: public GR_RedisEvent
{
public:
    GR_TinyRedisEvent();
    GR_TinyRedisEvent(GR_RedisServer *pServer);
    virtual ~GR_TinyRedisEvent();

    virtual int ConnectSuccess();
    virtual int ResultCheck(GR_MsgIdenty* pIdenty);
    virtual int Close(bool bForceClose = false);

public:
    uint64  m_lConnectSuccessTimes = 0;
};

class GR_TinyConfig
{
};

class GR_TinyRoute: public GR_Route
{
public:
    GR_TinyRoute();
    virtual ~GR_TinyRoute();

    virtual int Init(string &strGroup, YAML::Node &node);
    virtual int AddRedis(string &strRedisInfo);
    virtual GR_RedisEvent* Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError);
    virtual int Broadcast(GR_AccessEvent* pAccessEvent,GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData);
    int ConnectToServers();

public:
    bool m_bListenFlag = false;
    continuum   *m_vContinuum = nullptr;
    int         ncontinuum = 0;
};

class GR_TinyRouteGroup: public GR_RouteGroup
{
public:
    GR_TinyRouteGroup();
    virtual ~GR_TinyRouteGroup();
    
    virtual int Init(GR_Config *pConfig);
};
#endif
