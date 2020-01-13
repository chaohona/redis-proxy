#ifndef _GR_TWEM_ROUTE_H__
#define _GR_TWEM_ROUTE_H__

#include "route.h"
#include "dispatch.h"
#include "hash.h"
#include "redismsg.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <yaml-cpp/yaml.h>

#define GR_TWEM_DEFAULT_HASH "fnv1a_64"
#define GR_TWEM_DEFAULT_DIST "ketama"

typedef uint32 (*FUNC_HASH)(const char *, size_t);
typedef GR_RedisEvent* (*FUNC_DISPATCH)(GR_RedisServer **, continuum *, int, uint32);


class GR_TwemRoute: public GR_Route
{
public:
    GR_TwemRoute();
    virtual ~GR_TwemRoute();

    virtual int Init(GR_Config *pConfig);

    virtual GR_RedisEvent* Route(char *szKey, int iLen, int &iError);
    virtual GR_RedisEvent* Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError);
    virtual bool GetListenInfo(string &strIP, uint16 &uiPort, int &iBackLog);
    virtual int DelRedis(GR_RedisEvent *pEvent);
private:
    int ConfigParse(YAML::Node& node);
    int ParseListen();
    int ParseYamlRedisInfo(YAML::Node &&node);
    int AddYamlRedis(string &strRedisInfo);
    // 解析sentinel信息
    int ParseYamlSentinelInfo(YAML::Node &&node);
    int AddSentinel(string &&strSentinelInfo);
    int AddSentinelRedis(string &&strRedis);
private:
    string m_strListen = "0.0.0.0:6379";
    string m_strHash = GR_TWEM_DEFAULT_HASH;
    string m_strHashTag = "";
    string m_strDist = GR_TWEM_DEFAULT_DIST;
    bool    m_bRedis = true;
    bool    m_bAutoEject = false;
    Listen  m_Listen;               // 监听信息

    bool m_bHasHashTag = false;
    char m_cLeftTag;
    char m_cRightTag;
    FUNC_HASH m_FuncHash = GR_Hash::Fnv1a32;
    FUNC_DISPATCH m_FuncDispatch = GR_Dispatch::Ketama;
    continuum   *m_vContinuum;
    int         ncontinuum;
};

#endif
