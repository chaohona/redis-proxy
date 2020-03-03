#ifndef _GR_CONFIG_H__
#define _GR_CONFIG_H__
#include <yaml-cpp/yaml.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <list>

#include "define.h"
#include "log.h"

#define CONFIG_MAX_LINE     1024
#define DEFAULT_LISTEN_PORT 6379

enum GR_PROXY_BA_TYPE{
    GR_PROXY_BA_SYSTEM,
    GR_PROXY_BA_CONNECT,
    GR_PROXY_BA_IP,
};

struct GR_MasterAuth
{
    string strAddr = string("");
    string strIP = string("");
    uint16 usPort = 0;
    string strUser = string("");
    string strAuth = string("");
};

class GR_ReplicateInfo
{
public:
    int Check();

public:
    string                  m_strCfgFile = string("./conf/masterinfo.data");
    vector<GR_MasterAuth>   m_vMasters; // 
    vector<string>          m_vFilters;
};

class GR_Config
{
public:
    GR_Config();
    ~GR_Config();
public:
    int Load(char* szCfgPath);
    // 打印配置信息
    void Print();
private:
    int ParseListen();
    int ParseAofFiles(YAML::Node &node);
    int ParseRdbFiles(YAML::Node &node);
    int ParseLBPolicy(YAML::Node &node);
    int ParseReplicate(YAML::Node &node);
    int ValidCheck();

private:
    char* m_szCfgPath;

public:
    GR_REDIS_ROUTE_TYPE m_iRouteMode = PROXY_ROUTE_INVALID;              // 后端redis的集群模式
    PROXY_SVR_TYPE      m_iSvrType = PROXY_SVR_TYPE_MASTER;         // 进程类型
    string              m_strTwemCfg = string("./conf/nutcracker.yml");     // twemproxy配置文件路径
    string              m_strCodisDashboard = string("./conf/dashboard.toml");                   // codis dashboard配置文件
    string              m_strCodisProxy = string("./conf/proxy.toml");                       // codis proxy配置文件
    int                 m_iDaemonize = 0;                           // 是否以后台进程运行
    string              m_strLogFile = string("./log/gredis-proxy.log");
    int                 m_iWorkProcess = 0;
    int                 m_iWorkConnections = 200;
    int                 m_iWarningConnects = 180;
    int                 m_iTimePrecision = 1;
    GR_PROXY_BA_TYPE    m_iBAType = GR_PROXY_BA_SYSTEM;
    GR_LOG_LEVEL        m_iLogLevel = GR_LOG_LEVEL_NONE;
    string              m_strClusterConfig = string("./conf/cluster.yml");
    string              m_strTinyConfig = string("./conf/tiny.yml");
    string              m_strClusterSeedAddr = string("");
    string              m_strClusterSeedIP = string("0.0.0.0");
    uint16              m_usClusterSeedPort = 0;
    string              m_strClusterListen = string("0.0.0.0:6379");   
    int                 m_iRedisRspTT = 200;        // redis响应超时时间，超时则和redis断线重连
    int64               m_lLongestReq = 2 * 1024 * 1024;

    //通用配置，通过子配置文件返回，或者gredis配置自己配置
    string  m_strIP = string("0.0.0.0");
    uint16  m_usPort = 6379;    // 监听端口号
    int     m_iTcpBack = 1024;

    string  m_strAdminIP = string("0.0.0.0");
    uint16  m_usAdminPort = 8000;
    string  m_strAdminAddr = string("0.0.0.0:8000");

    vector<string>      m_listAofFiles;
    vector<string>      m_listRdbFiles;
    list<string>        m_listIndependIP;
    list<string>        m_listLBIP;

    GR_ReplicateInfo    m_ReplicateInfo;

    bool                m_bSupportClusterSlots = false;
};

#endif
