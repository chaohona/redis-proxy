#include "gr_clusterroute.h"
#include "redismsg.h"
#include "define.h"
#include "hash.h"
#include "redismgr.h"
#include "proxy.h"

int GR_GetClusterNodesCB(GR_TimerMeta * pMeta, void *pCB)
{
    GR_LOGI("get cluster nodes info callback.");
    GR_ClusterRoute *pRoute = (GR_ClusterRoute*)pCB;
    pRoute->ReInitFinish(); // 打开重连开关
    if (GR_OK == pRoute->ReInit())
    {
        if (pMeta != nullptr)
        {
            pMeta->Finish();
        }
        return GR_OK;
    }
    return GR_OK;
}

GR_ClusterSeedEvent::GR_ClusterSeedEvent()
{
    this->m_iRedisType = REDIS_TYPE_CLUSTER_SEED;
}

GR_ClusterSeedEvent::~GR_ClusterSeedEvent()
{
}

int GR_ClusterSeedEvent::GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    switch (pIdenty->uiCmdType)
    {
        case GR_CMD_CLUSTER_NODES:
        {
            this->m_iGotNodes = 1;
            int64 lNow = GR_GetNowMS();
            this->m_pRoute->m_lNextGetClustesMS = lNow + 1000; // 间隔1秒再重试
            ASSERT(this->m_pRoute != nullptr);
            this->m_pRoute->ReInitFinish();
            // 重新解析一遍获取结果
            static GR_RedisMsg ReadMsg;
            static GR_RedisMsgResults Results(2048);
            Results.Reinit();
            ReadMsg.StartNext();
            ReadMsg.Reset(pData->m_uszData);
            ReadMsg.Expand(pData->m_sUsedSize);
            if (GR_OK != ReadMsg.ParseRsp(&Results))
            {
                GR_LOGE("parse GR_CMD_SENTINEL_GET_MASTER message failed.");
                exit(0);
                return GR_ERROR;
            }
            if (Results.iCode != GR_OK || Results.iUsed == 0)
            {
                GR_LOGE("parse GR_CMD_SENTINEL_GET_MASTER results failed.");
                exit(0);
                return GR_ERROR;
            }
            return this->ParseClusterNodes(Results);
        }
        default:
        {
            GR_LOGE("invalid cmd type:%d", pIdenty->uiCmdType);
            break;
        }
    }
    return GR_OK;
}

// 解析clusternode结果，然后和集群建立连接
//d2740a205233e19cc1a61a22ef36711973787718 192.168.21.95:10004@20004 master - 0 1571132561436 0 connected\nc8c79971b770a1460f241cef5adeb89324076f78 192.168.21.95:10005@20005 master - 0 15711325604
int GR_ClusterSeedEvent::ParseClusterNodes(GR_RedisMsgResults &Results)
{
    ASSERT(Results.iUsed==1 && Results.vMsgMeta!=nullptr);
    GR_RedisMsgMeta dataLine = Results.vMsgMeta[0];
    int iResult = GR_OK;
    vector<GR_String> vResults = GR_SplitLine(dataLine.szStart, dataLine.iLen, '\n');
    for(auto itr=vResults.begin(); itr!=vResults.end(); itr++)
    {
        // 解析数据，并连接redis，
        vector<GR_String> vNodeData = GR_SplitLine(itr->szChar, itr->iLen, ' ');
        if (vNodeData.size() < 8)
        {
            continue;
        }
        GR_ClusterInfo info = GetClusterInfo(vNodeData, iResult);
        if (iResult != GR_OK )
        {
            // TODO 告警
            GR_LOGE("parse cluster info failed.");
            continue;
        }
        if (info.listSlots.size() == 0)
        {
            GR_LOGI("redis has no slot, addr %s:%d", info.szIP, info.usPort);
            continue;
        }
        GR_LOGI("redis has slots %d, addr %s:%d", info.listSlots.size(), info.szIP, info.usPort);
        GR_RedisServer* pServer;
        if (GR_OK != this->m_pRoute->AddRedisServer(info, pServer))
        {
            // TODO 告警
            GR_LOGE("connect slots nodes failed, %s:%d", info.szIP, info.usPort);
            continue;
        }
    }
    // 如果有slots没有对应的redis，则退出进程
    bool bReady = false;
    if (GR_OK != this->m_pRoute->ReadyCheck(bReady) || !bReady)
    {
        // TODO 告警
        GR_LOGE("slot to redis not ready.");
        if (this->m_pRoute->m_uiReConnectTimes == 0) // 启动有问题则进程直接退出
        {
            exit(0);
        }
        else // 说明还在选举期间，则重新获取
        {
            this->m_pRoute->m_iReInitFlag = 1; // 关闭重连开关
            GR_Timer::Instance()->AddTimer(GR_GetClusterNodesCB, this->m_pRoute, 500, true);
        }
    }
    else
    {
        GR_ClusterRoute *pRoute = dynamic_cast<GR_ClusterRoute*>(this->m_pRoute);
        this->m_pRoute->ReInitFinish();
        GR_LOGI("redis cluster reinit success:%s", pRoute->m_strGroup.c_str());
        ++this->m_pRoute->m_uiReConnectTimes;
        if (this->m_pRoute->m_uiReConnectTimes > 1024)
        {
            this->m_pRoute->m_uiReConnectTimes = 2;
        }
        // 只有在连接redis没问题的情况下才会启动监听端口
        if (this->m_pRoute->m_uiReConnectTimes == 1)
        {
            int iRet = GR_OK;
            GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
            GR_AccessMgr *pMgr = this->m_pRoute->m_pAccessMgr;
            // 启动监听端口
            if (pConfig->m_iBAType != GR_PROXY_BA_IP)
            {
                if (!pConfig->m_bSupportClusterSlots)
                {
                    iRet = pMgr->Listen(this->m_pRoute->m_strListenIP.c_str(), this->m_pRoute->m_usListenPort, 
                        pConfig->m_iTcpBack, pConfig->m_iBAType == GR_PROXY_BA_SYSTEM);
                }
                else
                {
                    // 如果需要支持cluster slots命令，则启动3个端口，端口号在配置的基础上加0-2
                    uint16 usPort = pConfig->m_usPort + (GR_Proxy::Instance()->m_ulInnerId%GR_CLUSTER_SLOTS_MIX_CASE);
                    iRet = pMgr->Listen(this->m_pRoute->m_strListenIP.c_str(), this->m_pRoute->m_usListenPort, 
                        pConfig->m_iTcpBack, pConfig->m_iBAType == GR_PROXY_BA_SYSTEM);
                }
            }
            if (iRet != GR_OK)
            {
                GR_LOGE("redis group listen failed:%s", this->m_pRoute->m_strGroup.c_str());
                return iRet;
            }
        }
    }
    
    return GR_OK;
}

GR_ClusterInfo GR_ClusterSeedEvent::GetClusterInfo(vector<GR_String> &vNodeData, int &iResult)
{
    iResult = GR_OK;
    try
    {
        GR_ClusterInfo info;
        int iLen;
        // Parse ip:port
        char *p, *s;
        iLen = vNodeData[1].iLen;
        vNodeData[1].szChar[iLen] = '\0';
        if ((p = strrchr(vNodeData[1].szChar, ':')) == NULL)
        {
            GR_LOGE("redis cluster config is invalid:%s", vNodeData[0].szChar);
            iResult = GR_ERROR;
            return info;
        }
        *p = '\0';
        info.szIP = vNodeData[1].szChar;
        char *port = p+1;
        char *busp = strchr(port,'@');
        if (busp) {
            *busp = '\0';
        }
        info.usPort = atoi(port);
        
        /* Parse flags */
        p = s = vNodeData[2].szChar;
        iLen = vNodeData[2].iLen;
        vNodeData[2].szChar[iLen] = '\0';
        int flags = 0;
        while(p) {
            p = strchr(s,',');
            if (p) *p = '\0';
            if (!strcasecmp(s,"myself")) {
                flags |= CLUSTER_NODE_MYSELF;
            } else if (!strcasecmp(s,"master")) {
                flags |= CLUSTER_NODE_MASTER;
            } else if (!strcasecmp(s,"slave")) {
                flags |= CLUSTER_NODE_SLAVE;
                return info;
            } else if (!strcasecmp(s,"fail?")) {
                flags |= CLUSTER_NODE_PFAIL;
                return info;
            } else if (!strcasecmp(s,"fail")) {
                flags |= CLUSTER_NODE_FAIL;
                return info;
            } else if (!strcasecmp(s,"handshake")) {
                flags |= CLUSTER_NODE_HANDSHAKE;
                return info;
            } else if (!strcasecmp(s,"noaddr")) {
                flags |= CLUSTER_NODE_NOADDR;
                return info;
            } else if (!strcasecmp(s,"nofailover")) {
                flags |= CLUSTER_NODE_NOFAILOVER;
                return info;
            } else if (!strcasecmp(s,"noflags")) {
                /* nothing to do */
            } else {
                GR_LOGE("Unknown flag in redis cluster config file");
                iResult = GR_ERROR;
            }
            if (p) s = p+1;
        }
        info.iFlags = flags;

        for (int i=8; i<vNodeData.size(); i++)
        {
            if (vNodeData[i].szChar[0] == '[')
            {
                continue;
            }
            iLen = vNodeData[i].iLen;
            vNodeData[i].szChar[iLen] = '\0';
            vector<GR_String> vSlots = GR_SplitLine(vNodeData[i].szChar, iLen, '-');
            ASSERT(vSlots.size() > 0);
            iLen = vSlots[0].iLen;
            vSlots[0].szChar[iLen] = '\0';
            GR_ClusterInfo::Slots s;
            s.iBegin = s.iEnd = atoi(vSlots[0].szChar);
            if (vSlots.size() == 2)
            {
                iLen = vSlots[1].iLen;
                vSlots[1].szChar[iLen] = '\0';
                s.iEnd = atoi(vSlots[1].szChar);
            }
            
            info.listSlots.push_back(s);
        }
        return info;
    }
    catch(exception &e)
    {
        GR_LOGE("parse cluster node info got exception:%s", e.what());
        iResult = GR_ERROR;
        return GR_ClusterInfo();
    }
    
}

int GR_ClusterSeedEvent::ConnectResult()
{
    if (!GR_Socket::ConnectedCheck(this->m_iFD))
    {
        GR_LOGE("connect result is failed %s:%d", this->m_szAddr, this->m_uiPort);
        // 如果是启动的时候错误则进程退出
        if (this->m_pRoute->m_vRedisSlot[0] == nullptr)
        {
            GR_LOGE("can not connect to redis cluster, process exit.");
            exit(0);
        }
        this->m_pRoute->ReInitFinish();
        GR_Timer::Instance()->AddTimer(GR_GetClusterNodesCB, this->m_pRoute, 500, true);
        this->Close(false);
        return GR_ERROR;
    }
    if (this->m_Status == GR_CONNECT_CONNECTED)
    {
        return GR_OK;
    }
    this->m_Status = GR_CONNECT_CONNECTED;
    GR_Epoll::Instance()->AddEventRead(this);
    GR_LOGI("connect to seed redis success:%s,%d", this->m_szAddr, this->m_uiPort);
    if (GR_OK != this->ReqClusterInfo())
    {
        this->m_pRoute->ReInitFinish();
        this->Close(false);
        GR_Timer::Instance()->AddTimer(GR_GetClusterNodesCB, this->m_pRoute, 500, true);
    }

    return GR_OK;
}

int GR_ClusterSeedEvent::ReqClusterInfo()
{
    auto pData = GR_MsgProcess::Instance()->ClusterNodesCmd();
    if (pData == nullptr)
    {
        GR_LOGE("package redis msg failed.");
        return GR_ERROR;
    }
    
    GR_MsgIdenty* pIdenty = GR_MsgIdentyPool::Instance()->Get();
    if (pIdenty == nullptr)
    {
        GR_LOGE("get msgidenty failed.");
        return GR_ERROR;
    }
    pIdenty->pAccessEvent = this;
    pIdenty->uiCmdType = GR_CMD_CLUSTER_NODES;
    pIdenty->pCB = this;
    GR_LOGD("begin to get cluster nodes.");
    int iRet = this->SendMsg(pData, pIdenty);
    // 获取redis地址
    if (iRet != GR_OK)
    {
        GR_LOGE("send message to redis failed:%s,%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_ClusterSeedEvent::Close(bool bForceClose)
{
    try
    {
        if (this->m_pRoute != nullptr)
        {
            this->m_pRoute->ReInitFinish();
            this->m_pRoute->m_pClusterSeedRedis = nullptr;
            // 如果没有获取过cluster nodes则需要重新获取
            if (this->m_iGotNodes == 0)
            {
                GR_Timer::Instance()->AddTimer(GR_GetClusterNodesCB, this->m_pRoute, 500, true);
            }
        }
        GR_RedisEvent::Close(true); // 不走断线重连流程
        delete this;
        return GR_OK;
    }
    catch(exception &e)
    {
        GR_LOGE("close cluster seed redis got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_ERROR;
}

int GR_ClusterSeedEvent::ReConnect(GR_TimerMeta *pMeta)
{
    return GR_ERROR;
}

GR_ClusterRoute::GR_ClusterRoute()
{
}

GR_ClusterRoute::~GR_ClusterRoute()
{
}

int GR_ClusterRoute::Init(string &strGroup, YAML::Node& node)
{
    try
    {
        if (GR_OK != GR_Route::Init(strGroup, node))
        {
            GR_LOGE("init route group failed:%s", strGroup.c_str());
            return GR_ERROR;
        }
        
        this->m_strGroup = strGroup;
        this->m_vServers = new GR_RedisServer*[MAX_REDIS_GROUP];
        memset(this->m_vServers, 0, sizeof(GR_RedisServer*)*MAX_REDIS_GROUP);
        memset(this->m_vRedisSlot, 0, sizeof(GR_RedisServer*)*CLUSTER_SLOTS);
        memset(this->m_szSeedIP, '\0', NET_IP_STR_LEN);
        // 1、解析集群配置文件
        if (GR_OK != ParseNativeClusterCfg(node))
        {
            GR_LOGE("prase native cluster config failed");
            return GR_ERROR;
        }
        
        // 解析监听端口
        
        // 和集群建立连接
        return this->ConnectToSeedRedis();
    }
    catch(exception &e)
    {
        GR_LOGE("cluster route init got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

// 和Redis断开重连流程
// 触发一次重连，会把和所有断开的Redis都重新连接
// m_iReInitFlag标记用来防止和多个Redis都断开，同时触发多次重连
int GR_ClusterRoute::ReInit()
{
    if (m_iReInitFlag == 1)
    {
        return GR_OK;
    }
    int64 lNow = GR_GetNowMS();
    if (lNow < m_lNextGetClustesMS)
    {
        GR_Timer::Instance()->AddTimer(GR_GetClusterNodesCB, this, m_lNextGetClustesMS-lNow, true);
        return GR_OK;
    }
    m_lNextGetClustesMS += 1000;
    bool bNeedReinit = false; // 是否需要重新初始化
    for (int i=0; i<CLUSTER_SLOTS; i++)
    {
        if (this->m_vRedisSlot[i] == nullptr)
        {
            bNeedReinit = true;
            break;
        }
        if (m_vRedisSlot[i]->pEvent == nullptr)
        {
            bNeedReinit = true;
            break;
        }
        if (!m_vRedisSlot[i]->pEvent->ConnectOK())
        {
            bNeedReinit = true;
            break;
        }
    }
    if (!bNeedReinit) // 不需要重新初始化
    {
        return GR_OK;
    }

    GR_LOGI("begin to reinit cluster info:%s", this->m_strGroup.c_str());
    // 重新获取port和端口号
    
    for (int i=0; i<CLUSTER_SLOTS; i++)
    {
        if (this->m_vRedisSlot[i] == nullptr)
        {
            continue;
        }
        if (m_vRedisSlot[i]->pEvent == nullptr)
        {
            continue;
        }
        if (m_vRedisSlot[i]->pEvent->m_Status != GR_CONNECT_CONNECTED)
        {
            continue;
        }
        // 如果所有Redis都断开了则尝试和老的SeedRedis重连
        this->m_usSeedPort = m_vRedisSlot[i]->pEvent->m_uiPort;
        memcpy(this->m_szSeedIP, m_vRedisSlot[i]->pEvent->m_szAddr, NET_IP_STR_LEN);
        break;
    }

    m_iReInitFlag = 1;
    if (GR_OK != this->ConnectToSeedRedis())
    {
        this->ReInitFinish();
        GR_Timer::Instance()->AddTimer(GR_GetClusterNodesCB, this, 500, true);
    }

    return GR_OK;
}

int GR_ClusterRoute::ReInitFinish()
{
    m_iReInitFlag = 0;
    return GR_OK;
}

int GR_ClusterRoute::ConnectToSeedRedis()
{
    try
    {
        if (this->m_pClusterSeedRedis != nullptr)
        {
            this->m_pClusterSeedRedis->Close(true);
            this->m_pClusterSeedRedis = nullptr;
        }
        this->m_pClusterSeedRedis = new GR_ClusterSeedEvent();
        this->m_pClusterSeedRedis->m_pRoute = this;
        if (GR_OK != this->m_pClusterSeedRedis->ConnectToRedis(this->m_usSeedPort, this->m_szSeedIP))
        {
            GR_LOGE("connecto to seed redis failed:%d,%s", this->m_usSeedPort, this->m_szSeedIP);
            return GR_ERROR;
        }
        return GR_OK;
    }
    catch(exception &e)
    {
        GR_LOGE("connect to seed redis got exception:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

GR_RedisEvent* GR_ClusterRoute::Route(char *szKey, int iLen, int &iError)
{
    iError = GR_OK;
    if (iLen == 7 && IS_CLUSTER_CMD(szKey))
    {
        iError = REDIS_RSP_UNSPPORT_CMD;
        return nullptr;
    }
    uint32 uiKey = this->KeyHashSlot(szKey, iLen);
    GR_RedisServer *pServer = this->m_vRedisSlot[uiKey];
    
    // TODO 如果为nullptr则获取slots对应的redis
    return pServer!=nullptr?pServer->pEvent:nullptr;
}

GR_RedisEvent* GR_ClusterRoute::Route(GR_MsgIdenty *pIdenty, GR_MemPoolData  *pData, GR_RedisMsg &msg, int &iError)
{
    iError = GR_OK;
    if (msg.m_Info.iKeyLen == 7 && IS_CLUSTER_CMD(msg.m_Info.szKeyStart))
    {
        iError = REDIS_RSP_UNSPPORT_CMD;
        return nullptr;
    }
    uint32 uiSlot = this->KeyHashSlot(msg.m_Info.szKeyStart, msg.m_Info.iKeyLen);
    GR_RedisServer *pServer = this->m_vRedisSlot[uiSlot];
    //if (this->m_vSlotFlags[uiKey] == GR_SLOT_NOT_SURE)
    //{
    //pIdenty->bRedirect = true;
    //}
    pIdenty->iSlot = uiSlot;

    // TODO 如果为nullptr则获取slots对应的redis
    return pServer!=nullptr?pServer->pEvent:nullptr;
}

//-MOVED 6918 192.168.21.95:10001\r\n
int GR_ClusterRoute::SlotRedirect(GR_MsgIdenty *pIdenty, GR_RedisMsg &msg, bool bAsking)
{
    static GR_RedisMsgResults Results(1);
    Results.Reinit();
    
    if (GR_OK != msg.ParseRsp(&Results))
    {
        GR_LOGE("parse redirect message failed.");
        return GR_ERROR;
    }
    if (Results.iCode != GR_OK || Results.iUsed != 1)
    {
        GR_LOGE("parse redirect message failed.");
        return GR_ERROR;
    }
    // 解析消息得到ip,port
    vector<GR_String> vNodeData = GR_SplitLine(Results.vMsgMeta[0].szStart, Results.vMsgMeta[0].iLen, ' ');
    if (vNodeData.size() != 3)
    {
        GR_LOGE("parse redirect message failed");
        return GR_ERROR;
    }
    vNodeData[2].szChar[vNodeData[2].iLen] = '\0';
    // 获取新的redis，转发
    char *szPort = strchr(vNodeData[2].szChar,':');
    if (szPort) 
        *szPort = '\0';
    else
    {
        GR_LOGE("parse redirect message, invalid address %s", vNodeData[2].szChar);
        return GR_ERROR;
    }
    szPort++;
    int iPort = atoi(szPort);
    GR_RedisServer* pServer = this->GetRedis(vNodeData[2].szChar, iPort);
    if (pServer != nullptr && pServer->pEvent!=nullptr)
    {
        if (bAsking && GR_OK != pServer->pEvent->SendMsg(GR_REDIS_GET_ERR(REDIS_REQ_ASK), GR_ASKING_IDENTY()))
        {
            GR_LOGE("parse redirect message failed");
            return GR_ERROR;
        }
        return pServer->pEvent->SendMsg(pIdenty->pReqData, pIdenty);
    }
    // 和新的redis建立连接，并将消息发送给新的redis
    GR_ClusterInfo info;
    info.szIP = vNodeData[2].szChar;
    info.usPort = iPort;
    if (GR_OK != this->AddRedisServer(info, pServer) || pServer==nullptr || pServer->pEvent==nullptr)
    {
        GR_LOGE("connect to redis failed %s:%d", info.szIP, info.usPort);
        return GR_ERROR;
    }
    // TODO 如果是asking，则校验返回值是否是okay
    if (bAsking && GR_OK != pServer->pEvent->SendMsg(GR_REDIS_GET_ERR(REDIS_REQ_ASK), GR_ASKING_IDENTY()))
    {
        GR_LOGE("parse redirect message failed");
        return GR_ERROR;
    }
    return pServer->pEvent->SendMsg(pIdenty->pReqData, pIdenty);
}

int GR_ClusterRoute::SlotChanged(GR_MsgIdenty *pIdenty, GR_RedisEvent *pEvent)
{
    if (pIdenty->iAskTimes > 0)
    {
        return GR_OK;
    }
    //this->m_vSlotFlags[pIdenty->iSlot] = GR_SLOT_SURE;
    this->m_vRedisSlot[pIdenty->iSlot] = pEvent->m_pServer;
    return GR_OK;
}

// 分析原始集群日志，获取集群中可用master的ip与port
int GR_ClusterRoute::ParseNativeClusterCfg(YAML::Node &node)
{
    if (!node["listen"])
    {
        GR_LOGE("redis group list not configed:%s", this->m_strGroup.c_str());
        return GR_ERROR;
    }
    this->m_strListen = node["listen"].as<string>();
    if (!node["seed_redis"])
    {
        return GR_ERROR;
    }

    if (!node["seed_redis"])
    {
        GR_LOGE("redis seed_redis not configed:%s", this->m_strGroup.c_str());
        return GR_ERROR;
    }
    string strSeedAddr = node["seed_redis"].as<string>();
    string  strIP;
    if (GR_OK != ParseAddr(strSeedAddr, strIP, this->m_usSeedPort))
    {
        GR_LOGE("parse seed addr failed, group:%s, addr:%s", this->m_strGroup.c_str(), strSeedAddr.c_str());
        return GR_ERROR;
    }
    int iLen = NET_IP_STR_LEN;
    if (iLen > strIP.length())
    {
        iLen = strIP.length();
    }
    memcpy(m_szSeedIP, strIP.c_str(), iLen);

    // 解析监听端口
    this->m_strListen = node["listen"].as<string>();
    if (GR_OK != ParseAddr(this->m_strListen, this->m_strListenIP, this->m_usListenPort))
    {
        GR_LOGE("invalid listen of group %s", this->m_strGroup.c_str() );
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_ClusterRoute::AddRedisServer(GR_ClusterInfo &info, GR_RedisServer* &pServer)
{
    int iResult = GR_OK;
    pServer = this->AddRedis(info.szIP, info.usPort, iResult); 
    if (iResult != GR_OK || pServer == nullptr)
    {
        // TODO 告警,如果pServer不为空则删除pServer
        GR_LOGE("connect to redis failed %s:%d", info.szIP, info.usPort);
        return GR_ERROR;
    }
    // 连接Redis
    if (pServer->pEvent == nullptr || !pServer->pEvent->ConnectOK())
    {
        if (GR_OK != pServer->Connect(this))
        {
            GR_LOGE("connect to redis failed %s:%d", info.szIP, info.usPort);
            return GR_ERROR;
        }
    }
    for (auto sItr=info.listSlots.begin(); sItr!=info.listSlots.end(); sItr++)
    {
        for(int iStart=sItr->iBegin; iStart<=sItr->iEnd; iStart++)
        {
            if (iStart >= CLUSTER_SLOTS)
            {
                GR_LOGE("invalid slot, bigger than 0x3FFF:%d", iStart);
                ASSERT(false);
                continue;
            }
            if (this->m_vRedisSlot[iStart] == nullptr || this->m_vRedisSlot[iStart]->pEvent == nullptr ||
                !this->m_vRedisSlot[iStart]->pEvent->ConnectOK())
            {
                this->m_vRedisSlot[iStart] = pServer;
            }
        }
    }
    return GR_OK;    
}

int GR_ClusterRoute::ExpandStart()
{
    //memset(this->m_vSlotFlags, GR_SLOT_NOT_SURE, sizeof(GR_SLOT_FLAG)*CLUSTER_SLOTS);
    return GR_OK;
}

/* -----------------------------------------------------------------------------
 * Key space handling
 * -------------------------------------------------------------------------- */

/* We have 16384 hash slots. The hash slot of a given key is obtained
 * as the least significant 14 bits of the crc16 of the key.
 *
 * However if the key contains the {...} pattern, only the part between
 * { and } is hashed. This may be useful in the future to force certain
 * keys to be in the same node (assuming no resharding is in progress). */
uint32 GR_ClusterRoute::KeyHashSlot(char *szKey, int iKeyLen)
{
    int s, e; /* start-end indexes of { and } */

    for (s = 0; s < iKeyLen; s++)
        if (szKey[s] == '{') break;

    /* No '{' ? Hash the whole key. This is the base case. */
    if (s == iKeyLen) return GR_Hash::Crc16(szKey,iKeyLen) & 0x3FFF;

    /* '{' found? Check if we have the corresponding '}'. */
    for (e = s+1; e < iKeyLen; e++)
        if (szKey[e] == '}') break;

    /* No '}' or nothing between {} ? Hash the whole key. */
    if (e == iKeyLen || e == s+1) return GR_Hash::Crc16(szKey,iKeyLen) & 0x3FFF;

    /* If we are here there is both a { and a } on its right. Hash
     * what is in the middle between { and }. */
    return GR_Hash::Crc16(szKey+s+1,e-s-1) & 0x3FFF;
}

int GR_ClusterRoute::ReadyCheck(bool &bReady)
{
    bReady = false;
    for (int i=0; i<CLUSTER_SLOTS; i++)
    {
        if (m_vRedisSlot[i] == nullptr || m_vRedisSlot[i]->pEvent == nullptr || !m_vRedisSlot[i]->pEvent->ConnectOK())
        {
            GR_LOGE("can not find redis for slot:%d, groupname:%s", i, this->m_strGroup.c_str());
            return GR_OK;
        }
    }
    bReady = true;
    return GR_OK;
}

int GR_ClusterRoute::DelRedis(GR_RedisEvent *pEvent)
{
    try
    {
        for (int i=0; i<CLUSTER_SLOTS; i++)
        {
            if (this->m_vRedisSlot[i] == pEvent->m_pServer)
            {
                this->m_vRedisSlot[i] = nullptr;
            }
        }
        GR_Route::DelRedis(pEvent);
    }
    catch(exception &e)
    {
        GR_LOGE("delete redis from route got exception:%s", e.what());
        abort();
    }

    return GR_OK;
}

GR_ClusterRouteGroup::GR_ClusterRouteGroup()
{
}

GR_ClusterRouteGroup::~GR_ClusterRouteGroup()
{
}

int GR_ClusterRouteGroup::Init(GR_Config *pConfig)
{
    try
    {
        this->m_vRoute = new GR_ClusterRoute[GR_MAX_GROUPS];
        YAML::Node node = YAML::LoadFile(pConfig->m_strClusterConfig.c_str());
        string strGroup;
        int index = 0;
        GR_ClusterRoute *pRoute = dynamic_cast<GR_ClusterRoute*>(this->m_vRoute);
        for(auto c=node.begin(); c!=node.end(); c++, ++pRoute)
        {
            strGroup = c->first.as<string>();
            pRoute = dynamic_cast<GR_ClusterRoute*>(pRoute);
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

