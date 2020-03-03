#include "gr_tiny.h"
#include "include.h"
#include "redismsg.h"
#include "proxy.h"

GR_TinyRedisServer::GR_TinyRedisServer()
{
    this->iWeight = 1;
}

GR_TinyRedisServer::~GR_TinyRedisServer()
{
}

int GR_TinyRedisServer::Connect(GR_Route *pRoute)
{
    int iRet = GR_OK;
    try
    {
        if (this->pEvent != nullptr)
        {
            this->pEvent->Close(true);
            delete this->pEvent; // TODO 连接池子
            this->pEvent = nullptr;
        }
        this->pEvent = new GR_TinyRedisEvent(this);
        this->pEvent->m_pRoute = pRoute;
        this->pEvent->m_iRedisType = REDIS_TYPE_MASTER;
        iRet = this->pEvent->ConnectToRedis(this->iPort, this->strHostname.c_str());
        if (iRet != GR_OK)
        {
            GR_LOGE("connect to redis failed, %s:%d", this->strHostname.c_str(), this->iPort);
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("connect to redis got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

GR_TinyRedisEvent::GR_TinyRedisEvent()
{
}

GR_TinyRedisEvent::GR_TinyRedisEvent(GR_RedisServer *pServer): GR_RedisEvent(pServer)
{

}

GR_TinyRedisEvent::~GR_TinyRedisEvent()
{
}

int GR_TinyRedisEvent::ConnectSuccess()
{
    // 第一次建立连接成功则启动监听端口
    GR_TinyRedisServer *pServer = dynamic_cast<GR_TinyRedisServer*>(this->m_pServer);
    ASSERT(pServer!=nullptr);
    if (this->m_lConnectSuccessTimes == 0)
    {
        auto pData = GR_MsgProcess::Instance()->SelectCmd(pServer->m_iDB);
        if (pData == nullptr)
        {
            GR_LOGE("get select cmd package failed %s:%d", this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
        // 发送select 命令
        GR_MsgIdenty* pIdenty = GR_MsgIdentyPool::Instance()->Get();
        if (pIdenty == nullptr)
        {
            GR_LOGE("get msgidenty failed %s:%d", this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
        pIdenty->pAccessEvent = this;
        pIdenty->uiCmdType = GR_CMD_SELECT;
        pIdenty->pCB = this;
        GR_LOGD("connect success, begin to send select cmd %s:%d", this->m_szAddr, this->m_uiPort);
        int iRet = this->SendMsg(pData, pIdenty);
        if (iRet != GR_OK)
        {
            GR_LOGE("send message to redis failed:%s,%d", this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
    }
    return GR_OK;
}

int GR_TinyRedisEvent::ResultCheck(GR_MsgIdenty* pIdenty)
{
    if (this->m_lConnectSuccessTimes != 0)
    {
        return GR_OK;
    }
    // 第一次返回的是select命令结果，检查是否ok
    if (pIdenty->uiCmdType != GR_CMD_SELECT)
    {
        GR_LOGE("not select db %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    // 如果结果不是+OK\r\n则不能使用
    if (!IS_OK_RESULT(this->m_ReadMsg))
    {
        GR_TinyRedisServer *pServer = dynamic_cast<GR_TinyRedisServer*>(this->m_pServer);
        GR_LOGE("select result is not ok, db:%d, address %s:%d", pServer->m_iDB, this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    this->m_lConnectSuccessTimes = 1;
    GR_TinyRoute *pRoute = dynamic_cast<GR_TinyRoute*>(this->m_pRoute);
    ASSERT(pRoute!=nullptr);
    // 没有启动监听端口，则启动监听端口
    if (!pRoute->m_bListenFlag)
    {
        // 当所有的redis都select dbid成功之后启动监听端口
        int iRet = GR_OK;
        GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
        GR_AccessMgr *pMgr = this->m_pRoute->m_pAccessMgr;
        bool bStartListen = true;
        GR_TinyRedisServer *pServer = nullptr;
        GR_TinyRedisEvent *pEvent = nullptr;
        for(int i=0; i<this->m_pRoute->m_iSrvNum; i++)
        {
            pServer = dynamic_cast<GR_TinyRedisServer*>(this->m_pRoute->m_vServers[i]);
            if (pServer == nullptr || pServer->pEvent == nullptr )
            {
                bStartListen = false;
                break;
            }
            pEvent = dynamic_cast<GR_TinyRedisEvent *>(pServer->pEvent);
            if (pEvent->m_lConnectSuccessTimes == 0)
            {
                bStartListen = false;
                break;
            }
        }
        if (bStartListen)
        {
            GR_LOGI("all redis prepare success, begin to start listen, group name:%s", this->m_pRoute->m_strGroup.c_str() );
            // 启动监听端口
            if (pConfig->m_iBAType != GR_PROXY_BA_IP)
            {
                iRet = pMgr->Listen(this->m_pRoute->m_strListenIP.c_str(), this->m_pRoute->m_usListenPort, 
                    pConfig->m_iTcpBack, pConfig->m_iBAType == GR_PROXY_BA_SYSTEM);
            }
            if (iRet != GR_OK)
            {
                GR_LOGE("redis group listen failed:%s", this->m_pRoute->m_strGroup.c_str());
                return iRet;
            }
            pRoute->m_bListenFlag = true;
        }
    }

    return GR_OK;
}

int GR_TinyRedisEvent::Close(bool bForceClose)
{
    GR_TinyRoute *pRoute = dynamic_cast<GR_TinyRoute*>(this->m_pRoute);
    ASSERT(pRoute!=nullptr);
    // 还没开始处理外部请求，说明启动的时候就没连上redis，退出进程，检查问题
    if (this->m_lConnectSuccessTimes == 0 || !pRoute->m_bListenFlag)
    {
        // 直接关闭
        GR_RedisEvent::Close(true);
        GR_LOGE("connect to redis failed at start, exit process, please check redis...");
        exit(0);
    }
    this->m_lConnectSuccessTimes = 0;
    GR_RedisEvent::Close(false);
    // 和redis重新建立连接，并且执行select dbid命令
    return GR_OK;
}

GR_TinyRoute::GR_TinyRoute()
{
}

GR_TinyRoute::~GR_TinyRoute()
{
}

int GR_TinyRoute::AddRedis(string &redisInfo)
{
    int iResult = GR_OK;
    GR_TinyRedisServer *pServer = new GR_TinyRedisServer();
    if (pServer == nullptr)
    {
        GR_LOGE("add redis server failed %s", redisInfo.c_str());
        return GR_ERROR;   
    }
    GR_Route::AddRedis(pServer, iResult);
    if (iResult != GR_OK)
    {
        GR_LOGE("add redis server failed %s", redisInfo.c_str());
        return GR_ERROR;
    }

    // 127.0.0.1:60001@1  ip:port@dbid
    vector<string> vNodeData = split(redisInfo, string("@"));
    if (vNodeData.size() != 2)
    {
        GR_LOGE("invalid redis info:%s", redisInfo.c_str());
        return GR_ERROR;
    }

    pServer->m_iDB = CharToInt((char*)vNodeData[1].c_str(), vNodeData[1].length(), iResult);
    if (iResult != GR_OK)
    {
        GR_LOGE("invalid dbid of redis info %s", redisInfo.c_str());
        return GR_ERROR;
    }

    string strIP;
    uint16 usPort;
    if (GR_OK != ParseAddr(vNodeData[0], strIP, usPort))
    {
        GR_LOGE("invalid dbid of redis inf %s", redisInfo.c_str());
        return GR_ERROR;
    }
    pServer->strAddr = vNodeData[0];
    pServer->strHostname = strIP;
    pServer->iPort = usPort;
    return GR_OK;
}

GR_RedisEvent* GR_TinyRoute::Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError)
{
    iError = GR_OK;
    uint32 uiKey = GR_Hash::Fnv1a32(msg.m_Info.szKeyStart, msg.m_Info.iKeyLen);
    return GR_Dispatch::Ketama(this->m_vServers, this->m_vContinuum, this->ncontinuum, uiKey);
}

int GR_TinyRoute::Broadcast(GR_AccessEvent* pAccessEvent, GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData)
{
    ASSERT(pAccessEvent!=nullptr);
    pIdenty->iNeedWaitNum = this->m_iSrvNum;
    GR_TinyRedisEvent *pEvent = nullptr;
    GR_TinyRedisServer *pServer = nullptr;
    int iRet = GR_OK;
    for (int i=0; i<this->m_iSrvNum; i++)
    {
        pServer = dynamic_cast<GR_TinyRedisServer*>(this->m_vServers[i]);
        if (pServer == nullptr )
        {
            pIdenty->iGotNum += 1;
            continue;
        }
        pEvent = dynamic_cast<GR_TinyRedisEvent*>(pServer->pEvent);
        if (pEvent == nullptr || !pEvent->ConnectOK())
        {
            pIdenty->iGotNum += 1;
            continue;
        }
        iRet = pEvent->SendMsg(pData, pIdenty);
        if (iRet != GR_OK)
        {
            pIdenty->iGotNum += 1;
            continue;
        }
    }
    if (pIdenty->iGotNum == pIdenty->iNeedWaitNum)
    {
        GR_MemPoolData *pData = GR_MsgProcess::Instance()->GetErrMsg(REDIS_RSP_COMM_ERR);
        pIdenty->pData = nullptr;
        pAccessEvent->DoReply(pData, pIdenty);
    }
    return GR_OK;
}

int GR_TinyRoute::Init(string &strGroup, YAML::Node &node)
{
    try
    {
        if (GR_OK != GR_Route::Init(strGroup, node))
        {
            GR_LOGE("init route group failed:%s", strGroup.c_str());
            return GR_ERROR;
        }
        if (node.size() != 2)
        {
            GR_LOGE("invalid redis group info:%s", strGroup.c_str());
            return GR_ERROR;
        }
        if (!node["listen"] || !node["servers"] )
        {
            GR_LOGE("invalid redis group info:%s", strGroup.c_str());
            return GR_ERROR;
        }
        string strIP;
        uint16 usPort;
        // 解析监听端口
        this->m_strListen = node["listen"].as<string>();
        if (GR_OK != ParseAddr(this->m_strListen, this->m_strListenIP, this->m_usListenPort))
        {
            GR_LOGE("invalid listen of group %s", strGroup.c_str() );
            return GR_ERROR;
        }
        
        YAML::Node servers = node["servers"];
        if (servers.size() == 0 || servers.size() > MAX_REDIS_GROUP)
        {
            GR_LOGE("there should has 1-%d redis, group name:%s", strGroup.c_str(), MAX_REDIS_GROUP);
            return GR_ERROR;
        }
        this->m_strGroup = strGroup;

        string strRedisInfo;
        for(int i=0; i<servers.size(); i++)
        {
            strRedisInfo = servers[i].as<string>();
            if (GR_OK != this->AddRedis(strRedisInfo))
            {
                GR_LOGE("add redis failed, group name:%s, redis info:%s", strGroup.c_str(), strRedisInfo.c_str());
                return GR_ERROR;
            }
        }

        if (GR_OK != GR_Dispatch::KetamaUpdate(this->m_vServers, this->m_vContinuum, this->m_iSrvNum, this->ncontinuum))
        {
            GR_LOGE("ketama update failed");
            return GR_ERROR;
        }
        if (GR_OK != this->ConnectToServers())
        {
            GR_LOGE("connect to redis failed %s", strGroup.c_str() );
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("cluster route init got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_TinyRoute::ConnectToServers()
{
    int iRet = GR_OK;
    GR_TinyRedisServer *pServer;
    for (int i=0; i<this->m_iSrvNum; i++)
    {
        pServer = dynamic_cast<GR_TinyRedisServer*>(this->m_vServers[i]);
        iRet = pServer->Connect(this);
        if (iRet != GR_OK)
        {
            GR_LOGE("connect to redis failed:%s", pServer->strInfo.c_str());
            return GR_ERROR;
        }
    }
    
    return GR_OK;
}


GR_TinyRouteGroup::GR_TinyRouteGroup()
{
}

GR_TinyRouteGroup::~GR_TinyRouteGroup()
{
}

int GR_TinyRouteGroup::Init(GR_Config *pConfig)
{
    try
    {
        this->m_vRoute = new GR_TinyRoute[GR_MAX_GROUPS];
        YAML::Node node = YAML::LoadFile(pConfig->m_strTinyConfig.c_str());
        string strGroup;
        if (node.size() > GR_MAX_GROUPS)
        {
            GR_LOGE("too many groups %d, it is should less than %d", node.size(), GR_MAX_GROUPS);
            return GR_ERROR;
        }
        GR_TinyRoute *pRoute = dynamic_cast<GR_TinyRoute*>(this->m_vRoute);
        for(auto c=node.begin(); c!=node.end(); c++, ++pRoute)
        {
            strGroup = c->first.as<string>();
            pRoute = dynamic_cast<GR_TinyRoute*>(pRoute);
            if (GR_OK != pRoute->Init(strGroup, c->second))
            {
                GR_LOGE("init redis route failed:%s", strGroup.c_str());
                return GR_ERROR;
            }
        }
    }
    catch(exception &e)
    {
        GR_LOGE("init route group got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}


