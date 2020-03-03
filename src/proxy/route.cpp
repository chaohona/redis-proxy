#include "route.h"
#include "proxy.h"
#include "config.h"

GR_Route::GR_Route():m_iSrvNum(0), m_vServers(nullptr)
{
}

GR_Route::~GR_Route()
{
}

int GR_Route::Init()
{
    this->m_vServers = new GR_RedisServer*[MAX_REDIS_GROUP];
    this->m_pAccessMgr = new GR_AccessMgr();
    if (GR_OK != this->m_pAccessMgr->Init(this))
    {
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Route::Init(string &strGroup, YAML::Node& node)
{
    return this->Init();
}

int GR_Route::Init(GR_Config *pConfig)
{
    return this->Init();
}

int GR_Route::ReInit()
{
    return GR_OK;
}

GR_RedisEvent* GR_Route::Route(char *szKey, int iLen, int &iError)
{
    return nullptr;
}

GR_RedisEvent* GR_Route::Route(GR_RedisEvent* vEventList, int iNum, GR_AccessEvent *pEvent)
{
    return nullptr;
}

GR_RedisEvent* GR_Route::Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError)
{
    return nullptr;
}

int GR_Route::Broadcast(GR_AccessEvent* pAccessEvent,GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData)
{
    return GR_OK;
}

bool GR_Route::GetListenInfo(string &strIP, uint16 &uiPort, int &iBackLog)
{
    return false;
}

GR_RedisServer* GR_Route::AddRedis(char *szIP, uint16 usPort, int &iResult)
{
    uint64 ulIdenty = 0;
    try
    {
        AddrToLong(szIP, usPort, ulIdenty);
        GR_RedisServer *pServer;
        pServer = this->m_mapServers[ulIdenty];
        if (pServer != nullptr && pServer->pEvent != nullptr && pServer->pEvent->ConnectOK())
        {
            return pServer;
        }
        iResult = GR_OK;
        if (this->m_iSrvNum+1 == MAX_REDIS_GROUP)
        {
            GR_LOGE("too many redis configed");
            iResult = GR_ERROR;
            return nullptr;
        }
        pServer = new GR_RedisServer();
        this->m_mapServers[ulIdenty] = pServer;
        pServer->ulIdx = this->m_iSrvNum;
        this->m_vServers[this->m_iSrvNum] = pServer;
        this->m_iSrvNum+=1;
        pServer->strHostname = szIP;
        pServer->iPort = usPort;
        pServer->ulIdenty = ulIdenty;
        pServer->iInPool = 1;
        return pServer;
    }
    catch(exception &e)
    {
        GR_LOGE("add redis got exception %s:%d, msg:%s", szIP, usPort, e.what());
        iResult = GR_ERROR;
        return nullptr;
    }
}

GR_RedisServer* GR_Route::AddRedis(int &iResult)
{
    try
    {
        GR_RedisServer *pServer = new GR_RedisServer();
        this->AddRedis(pServer, iResult);
        return pServer;
    }
    catch(exception &e)
    {
        iResult = GR_ERROR;
        GR_LOGE("add redis got exception:%s", e.what());
        return nullptr;
    }
}

GR_RedisServer* GR_Route::AddRedis(GR_RedisServer* pServer, int &iResult)
{
    iResult = GR_OK;
    try
    {
        if (this->m_iSrvNum+1 == MAX_REDIS_GROUP)
        {
            GR_LOGE("too many redis configed");
            iResult = GR_ERROR;
            return nullptr;
        }
        pServer->ulIdx = this->m_iSrvNum;
        this->m_vServers[this->m_iSrvNum] = pServer;
        this->m_iSrvNum+=1;
        return pServer;
    }
    catch(exception &e)
    {
        GR_LOGE("add redis got exception:%s", e.what());
        iResult = GR_ERROR;
        return nullptr;
    }
}


GR_RedisServer* GR_Route::GetRedis(char *szIP, uint16 usPort)
{
    uint64 ulValue;
    GR_RedisServer *pServer;
    AddrToLong(szIP, usPort, ulValue);
    pServer = this->m_mapServers[ulValue];
    if (pServer != nullptr)
    {
        return pServer;
    }
    
    for (int i=0; i<this->m_iSrvNum; i++)
    {
        pServer = m_vServers[i];
        if (pServer == nullptr)
        {
            continue;
        }
        if (pServer->ulIdenty == ulValue)
        {
            return pServer;
        }
    }
    return nullptr;
}

GR_RedisServer* GR_Route::GetRedis(char *szAddr)
{
    char *port = strchr(szAddr,',');
    if (port) *port = '\0';
    else return nullptr;
    port++;
    int iPort = atoi(port);
    
    return this->GetRedis(szAddr, iPort);
}

int GR_Route::DelRedis(GR_RedisEvent *pEvent)
{
    if (pEvent->m_iRedisType != REDIS_TYPE_MASTER)
    {
        return GR_OK;
    }
    try
    {
        if (pEvent == nullptr || pEvent->m_pServer == nullptr || pEvent->m_pServer->iInPool == 0)
        {
            return GR_OK;
        }
        uint64 ulIdx = pEvent->m_pServer->ulIdx;
        char *szIP = pEvent->m_szAddr;
        uint16 usPort = pEvent->m_uiPort;
        if (ulIdx >= this->m_iSrvNum)
        {
            return GR_OK;
        }
        GR_RedisServer *pServer;
        uint64 ulValue;
        AddrToLong(szIP, usPort, ulValue);
        pServer = this->m_mapServers[ulValue];
        pServer->pEvent = nullptr;
        if (pServer !=nullptr && pServer->ulIdx == ulIdx)
        {
            this->m_mapServers.erase(ulValue); 
        }
        
        int index = -1;
        for (int i=0; i<this->m_iSrvNum; i++)
        {
            pServer = m_vServers[i];
            if (pServer == nullptr || pServer->ulIdenty == 0)
            {
                continue;
            }
            if (pServer->ulIdenty == ulValue && ulIdx == i)
            {
                index = i;
                delete m_vServers[i];
                this->m_vServers[i] = nullptr;
                break;
            }
        }
        if (index>=0)
        {
            this->m_iSrvNum -= 1;
            ASSERT(this->m_iSrvNum>=0);
            if (index == MAX_REDIS_GROUP-1 || index==this->m_iSrvNum)
            {
                return GR_OK;
            }
            this->m_vServers[index] = this->m_vServers[this->m_iSrvNum];
            this->m_vServers[this->m_iSrvNum] = nullptr;
            return GR_OK;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("delete redis server got exception:%s", e.what());
        return GR_ERROR;
    }
    
    return GR_OK;
}

int GR_Route::ConnectToRedis()
{
    int iRet;
    GR_RedisServer *pServer;
    for (int i=0; i<this->m_iSrvNum; i++)
    {
        pServer = this->m_vServers[i];
        iRet = pServer->Connect(this);
        if (iRet != GR_OK)
        {
            GR_LOGE("connect to redis failed:%s", pServer->strInfo.c_str());
            return GR_ERROR;
        }
    }
    
    GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
    // 启动监听端口
    if (pConfig->m_iBAType != GR_PROXY_BA_IP)
    {
        iRet = this->m_pAccessMgr->Listen(pConfig->m_strIP.c_str(), pConfig->m_usPort, 
            pConfig->m_iTcpBack, pConfig->m_iBAType == GR_PROXY_BA_SYSTEM);
    }
    if (iRet != GR_OK)
    {
        GR_LOGE("listen failed.");
        exit(0);
    }
    
    return GR_OK;
}

GR_RouteGroup::GR_RouteGroup()
{
}

GR_RouteGroup::~GR_RouteGroup()
{
}

int GR_RouteGroup::Init(GR_Config *pConfig)
{
    return GR_OK;
}

int GR_RouteGroup::ConnectToRedis()
{
    for (GR_Route *pRoute=this->m_vRoute; pRoute!=nullptr; pRoute++)
    {
        if (GR_OK != pRoute->ConnectToRedis())
        {
            return GR_ERROR;
        }
    }
    return GR_OK;
}

