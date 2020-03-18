#include "config.h"
#include "utils.h"
#include "log.h"

#include <iostream>

using namespace std;

int GR_ReplicateInfo::Check()
{
    for(auto itr=this->m_vFilters.begin(); itr!=m_vFilters.end(); ++itr)
    {
        if ((*itr).length() > 256)
        {
            cout << "pre-filters: length should less then 257. " << *itr << endl;
            return GR_ERROR;
        }
    }
    return GR_OK;
}

GR_Config::GR_Config()
{
}

GR_Config::~GR_Config()
{
}

int GR_Config::Load(char* szCfgPath)
{
    try
    {
        YAML::Node node = YAML::LoadFile(szCfgPath);
        string content;
        for(auto c=node.begin(); c!=node.end(); c++)
        {
            content = c->first.as<string>();
            if (content == "work-mode")
            {
                content = c->second.as<string>();
                if (content == "proxy")
                {
                    this->m_iSvrType = PROXY_SVR_TYPE_MASTER;
                }
                else if (content == "replica")
                {
                    this->m_iSvrType = PROXY_SVR_TYPE_REPLICA;
                }
                else if (content == "aof-load")
                {
                    this->m_iSvrType = PROXY_SVR_TYPE_AOF;
                }
                else if (content == "rdb-load")
                {
                    this->m_iSvrType = PROXY_SVR_TYPE_RDB;
                }
                else
                {
                    cout << "invalid work-mode:" << content.c_str() << endl;
                    return GR_ERROR;
                }
            }
            else if (content == "route-mode")
            {
                content = c->second.as<string>();
                if (content == "twemproxy")
                {
                    this->m_iRouteMode = PROXY_ROUTE_TWEM;
                }
                else if (content == "codis")
                {
                    this->m_iRouteMode = PROXY_ROUTE_CODIS;
                }
                else if (content == "cluster")
                {
                    this->m_iRouteMode = PROXY_ROUTE_CLUSTER;
                }
                else if (content == "tiny")
                {
                    this->m_iRouteMode = PROXY_ROUTE_TINY;
                }
                else
                {
                    cout << "invalid mode" << content.c_str() << endl;
                    return GR_ERROR;
                }
            }
            else if (content == "twemproxy-conf")
            {
                this->m_strTwemCfg = c->second.as<string>();
            }
            else if (content == "codis-dashboard")
            {
                this->m_strCodisDashboard = c->second.as<string>();
            }
            else if (content == "codis-proxy")
            {
                this->m_strCodisProxy = c->second.as<string>();
            }
            else if (content == "daemonize")
            {
                content = c->second.as<string>();
                if (content == "yes")
                {
                    this->m_iDaemonize = 1;
                }
            }
            else if (content == "loglevel")
            {
                content = c->second.as<string>();
                if (content == "debug")
                {
                    this->m_iLogLevel = GR_LOG_LEVEL_DEBUG;
                }
                else if (content == "info")
                {
                    this->m_iLogLevel = GR_LOG_LEVEL_INFO;
                }
                else if (content == "error")
                {
                    this->m_iLogLevel = GR_LOG_LEVEL_ERROR;
                }
                else if (content == "warning")
                {
                    this->m_iLogLevel = GR_LOG_LEVEL_WARNING;
                }
                else if (content == "none")
                {
                    this->m_iLogLevel = GR_LOG_LEVEL_NONE;
                }
                else
                {
                    cout << "invalid log level:" << content.c_str() << endl;
                    return GR_ERROR;
                }

                GR_Log::Instance()->SetLogLevel(this->m_iLogLevel);
            }
            else if (content == "logfile")
            {
                this->m_strLogFile = c->second.as<string>();
            }
            else if (content == "work-proceses")
            {
                this->m_iWorkProcess = c->second.as<int>();
                if (this->m_iWorkProcess < 0)
                {
                    cout << "invalid work-proceses:" << content << endl;;
                    return GR_ERROR;
                }
                if (this->m_iWorkProcess > MAX_WORK_PROCESS_NUM)
                {
                    GR_STDERR("work-process is %d, force set as %d", this->m_iWorkProcess, MAX_WORK_PROCESS_NUM);
                    this->m_iWorkProcess = MAX_WORK_PROCESS_NUM;
                }
            }
            else if (content == "work-connections")
            {
                this->m_iWorkConnections = c->second.as<int>();
                if (this->m_iWorkConnections < 1)
                {
                    GR_STDERR("invalid work-connections:%s", content.c_str());
                    return GR_ERROR;
                }
                this->m_iWarningConnects = this->m_iWorkConnections/8*7;
            }
            else if(content == "time-precision")
            {
                this->m_iTimePrecision = c->second.as<int>();
                if (this->m_iTimePrecision < 1)
                {
                    this->m_iTimePrecision = 10;
                }
            }
            else if (content == "balance-mode")
            {
                string strMode = c->second.as<string>();
                if (strMode == "system")
                {
                    this->m_iBAType = GR_PROXY_BA_SYSTEM;
                }
                else if (strMode == "connects")
                {
                    this->m_iBAType = GR_PROXY_BA_CONNECT;
                }
                else if (strMode == "ip")
                {
                    this->m_iBAType = GR_PROXY_BA_IP;
                }
                else
                {
                    GR_STDERR("invalid ba type:%s", strMode.c_str());
                    return GR_ERROR;
                }
                cout << "balance-mode is " << strMode << endl;
            }
            else if (content == "admin-addr")
            {
                this->m_strAdminAddr = c->second.as<string>();
                if (GR_OK != this->ParseListen())
                {
                    cout << "invalid admin-addr:" << this->m_strAdminAddr << endl;
                    return GR_ERROR;
                }
                cout << "admin address is " << this->m_strAdminAddr << endl;
            }
            else if (content == "cluster-config-file")
            {
                this->m_strClusterConfig = c->second.as<string>();
            }
            else if (content == "cluster-listen")
            {
                this->m_strClusterListen = c->second.as<string>();
                if (GR_OK != ParseAddr(this->m_strClusterListen, this->m_strIP, this->m_usPort))
                {
                    cout << "invalid cluster-listen:" << this->m_strClusterListen << endl;
                    return GR_ERROR;
                }
                cout << "cluster listen address is " << this->m_strClusterListen << endl;
            }
            else if (content == "aof-files")
            {
                if (GR_OK != this->ParseAofFiles(c->second))
                {
                    cout << "parse aof-files failed." << endl;
                    return GR_ERROR;
                }
            }
            else if (content == "rdb-files")
            {
                if (GR_OK != this->ParseRdbFiles(c->second))
                {
                    cout << "parse rdb-files failed." << endl;
                    return GR_ERROR;
                }
            }
            else if (content == "lb-policy")
            {
                if (GR_OK != this->ParseLBPolicy(c->second))
                {
                    cout << "parse load balance policy failed." << endl;
                    return GR_ERROR;
                }
            }
            else if (content == "replicate")
            {
                if (GR_OK != this->ParseReplicate(c->second))
                {
                    cout << "parse replicate failed." << endl;
                    return GR_ERROR;
                }
            }
            else if (content == "redis-rsp-tt")
            {
                this->m_iRedisRspTT = c->second.as<int>();
                if (this->m_iRedisRspTT < 10)
                {
                    this->m_iRedisRspTT = 10;
                }
                cout << "redis response time out ms:" << this->m_iRedisRspTT << endl;
            }
            else if (content == "support-cluster-slots")
            {
                this->m_bSupportClusterSlots = c->second.as<bool>();
                if (this->m_bSupportClusterSlots)
                {
                    cout << "redis proxy support cluster nodes command" << endl;
                }
            }
            else if (content == "longest-requst")
            {
                // 配置区间在1k-10M之间
                int64 lConfig = c->second.as<int>();
                if (lConfig < 1)
                {
                    lConfig = 1;
                }
                if (lConfig > 10 * 1024)
                {
                    lConfig = 10*1024;
                }
                this->m_lLongestReq = lConfig*1024;
                cout << "longest requst is:"<< this->m_lLongestReq << endl;
            }
            else if (content == "tiny-conf")
            {
                this->m_strTinyConfig = c->second.as<string>();
            }
            else if (content == "store-server")
            {
                if (GR_OK != this->ParseStore(c->second))
                {
                    GR_STDERR("parse store-server failed.");
                    return GR_ERROR;
                }
            }
            else if (content == "aof-loads")
            {
                if (GR_OK != this->ParseAof(c->second))
                {
                    GR_STDERR("parse aof info failed.");
                    return GR_ERROR;
                }
            }
        }

        if (GR_OK != this->ValidCheck())
        {
            GR_STDERR("invalid config file.");
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        cout << "parse config file:" << szCfgPath <<", got exception:" << e.what() << endl;
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Config::ValidCheck()
{
    // 检查进程工作模式
    switch (this->m_iSvrType)
    {
        case PROXY_SVR_TYPE_REPLICA:
        {
            if (GR_OK != this->m_ReplicateInfo.Check() )
            {
                GR_STDERR("replicate: parse failed.");
                return GR_ERROR;
            }
            break;
        }
        case PROXY_SVR_TYPE_AOF:
        {
            if (this->m_listAofFiles.size() == 0)
            {
                GR_STDERR("aof-files: should has 1 aof file path at least.");
                return GR_ERROR;
            }
            break;
        }
        case PROXY_SVR_TYPE_RDB:
        {
            if (this->m_listRdbFiles.size() == 0)
            {
                GR_STDERR("rdb-files: should has 1 rdb file path at least.");
                return GR_ERROR;
            }
            break;
        }
    }
    return GR_OK;
}

int GR_Config::ParseListen()
{
    auto results = split(this->m_strAdminAddr, ":");
    if (results.size() != 2)
    {
        return GR_ERROR;
    }
    this->m_strAdminIP = results[0];
    this->m_usAdminPort = stoi(results[1], 0, 10);
    return GR_OK;
}

void GR_Config::Print()
{
    
}

int GR_Config::ParseAofFiles(YAML::Node &node)
{
    if (node.size() == 0)
    {
        GR_LOGW("replicaof has no redis.");
        return GR_OK;
    }

    for(int i=0; i<node.size(); i++)
    {
        string  aofFile = node[i].as<string>();
        this->m_listAofFiles.push_back(aofFile);
    }
    return GR_OK;
}

int GR_Config::ParseRdbFiles(YAML::Node &node)
{
    if (node.size() == 0)
    {
        GR_LOGW("replicaof has no redis.");
        return GR_OK;
    }

    for(int i=0; i<node.size(); i++)
    {
        string  rdbFile = node[i].as<string>();
        this->m_listRdbFiles.push_back(rdbFile);
    }
    return GR_OK;
}

int GR_Config::ParseLBPolicy(YAML::Node &node)
{
    return GR_OK;
    if (node["independence-ip"])
    {
        auto iplist = node["independence-ip"];
        for(int i=0; i<iplist.size(); i++)
        {
            string ip = iplist[i].as<string>();
            this->m_listIndependIP.push_back(ip);
        }
    }
    if (node["load-balance-ip"])
    {
        auto iplist = node["load-balance-ip"];
        for(int i=0; i<iplist.size(); i++)
        {
            string ip = iplist[i].as<string>();
            this->m_listLBIP.push_back(ip);
        }
    }
    return GR_OK;
}

int GR_Config::ParseReplicate(YAML::Node &node)
{
    if (node["mediacy-file"])
    {
        this->m_ReplicateInfo.m_strCfgFile = node["mediacy-file"].as<string>();
    }
    if (node["masters"])
    {
        auto masters = node["masters"];
        for(int i=0; i<masters.size(); i++)
        {
            auto info = masters[i]; // replicate.masters.info
            if (!info["addr"])
            {
                GR_LOGE("replicate.masters.info.addr is empty, index:%d", i+1);
                return GR_ERROR;
            }
            GR_MasterAuth redisInfo;
            redisInfo.strAddr = info["addr"].as<string>();
            if (GR_OK != ParseAddr(redisInfo.strAddr, redisInfo.strIP, redisInfo.usPort))
            {
                GR_LOGE("replicate.masters.info.addr is invalid, index:%d", i+1);
                return GR_ERROR;
            }
            if (info["user"])
            {
                redisInfo.strUser = info["user"].as<string>();
            }
            if (info["auth"])
            {
                redisInfo.strAuth = info["auth"].as<string>();
            }
            this->m_ReplicateInfo.m_vMasters.push_back(redisInfo);
        }
    }
    if (node["pre-filters"])
    {
        auto filters = node["pre-filters"];
        for(int i=0; i<filters.size(); i++)
        {
            this->m_ReplicateInfo.m_vFilters.push_back(filters[i].as<string>());
        }
    }
    return GR_OK;
}

int GR_Config::ParseStore(YAML::Node &node)
{
    if (!node["dbpath"])
    {
        GR_LOGE("there should has dbpath.");
        return GR_ERROR;
    }
    this->m_storeInfo.strDBPath = node["dbpath"].as<string>();
    return GR_OK;
}

int GR_Config::ParseAof(YAML::Node &node)
{
    if (node["unix-time"])
    {
        this->m_aofInfo.lUnixTime = node["unix-time"].as<long>();
    }
    return GR_OK;
}

