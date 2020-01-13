#include "sentinelevent.h"
#include "redismsg.h"
#include "hiredis/hiredis.h"
#include "redismgr.h"
#include "gr_proxy_global.h"

int GR_SentinelEventReconnectCB(GR_TimerMeta * pMeta, void *pCB)
{
    GR_RedisEvent *pEvent = (GR_RedisEvent*)pCB;
    ASSERT(pEvent!=nullptr);
    pEvent->ReConnect(pMeta);
    return GR_OK;
}

GR_SentinelEvent::GR_SentinelEvent(int iFD, sockaddr &sa, socklen_t &salen):GR_RedisEvent(iFD,sa,salen)
{
    this->m_iRedisType = REDIS_TYPE_SENTINEL;
}

GR_SentinelEvent::GR_SentinelEvent(GR_RedisServer *pServer):GR_RedisEvent(pServer)
{
    this->m_iRedisType = REDIS_TYPE_SENTINEL;
}

GR_SentinelEvent::~GR_SentinelEvent()
{
}

int GR_SentinelEvent::GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    ASSERT(pData != nullptr);
    switch (pIdenty->uiCmdType)
    {
        case GR_CMD_SENTINEL_GET_MASTER:
        {
            // 重新解析一遍获取结果
            static GR_RedisMsg ReadMsg;
            static GR_RedisMsgResults Results(128);
            Results.Reinit();
            ReadMsg.StartNext();
            ReadMsg.Reset(pData->m_uszData);
            ReadMsg.Expand(pData->m_sUsedSize);
            if (GR_OK != ReadMsg.ParseRsp(&Results))
            {
                GR_LOGE("parse GR_CMD_SENTINEL_GET_MASTER message failed.");
                return GR_ERROR;
            }
            if (Results.iCode != GR_OK || Results.iUsed != 2)
            {
                GR_LOGE("parse GR_CMD_SENTINEL_GET_MASTER results failed.");
                return GR_ERROR;
            }
            //连接redis
            GR_RedisServer *pServer = (GR_RedisServer *)pIdenty->pCB;
            ASSERT(pServer!=nullptr);
            pServer->strHostname = string(Results.vMsgMeta[0].szStart, Results.vMsgMeta[0].iLen);
            pServer->iPort = Results.vMsgMeta[1].ToInt();
            int iRet = pServer->Connect();
            if (iRet != GR_OK)
            {
                GR_LOGE("connect to redis failed:%s", pServer->strHostname.c_str());
                return GR_ERROR;
            }
            return GR_OK;
        }
        default:
        {
            
        }
    }

    return GR_OK;
}

// 
int GR_SentinelEvent::ConnectResult()
{
    if (0 != GR_Socket::GetError(this->m_iFD))
    {
        this->Close();
        GR_LOGD("connect result is failed %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    this->m_Status = GR_CONNECT_CONNECTED;
    GR_Epoll::Instance()->AddEventRead(this);
    GR_LOGI("connect to sentinel success:%s,%d", this->m_szAddr, this->m_uiPort);

    return GR_RedisMgr::Instance()->SentinelConnected(this);
}

int GR_SentinelEvent::ReConnect(GR_TimerMeta *pMeta)
{
    uint64 ulNow = CURRENT_MS();
    if (ulNow < this->m_ulNextTry) // 没到重试时间
    {
        if (pMeta == nullptr)
        {
            // 创建自动重连的回调函数
            pMeta = GR_Timer::Instance()->AddTimer(GR_SentinelEventReconnectCB, this->m_pServer, 10, true);
        }
        return GR_OK;
    }
    this->m_ulNextTry = ulNow + 100;
    for(int i=0; i<3; i++)
    {
        if (GR_OK == this->ConnectToRedis(this->m_uiPort, this->m_szAddr))
        {
            if (pMeta != nullptr)
            {
                pMeta->Finish();
            }
            return GR_OK;
        }
    }

    if (pMeta == nullptr)
    {
        // 创建自动重连的回调函数
        pMeta = GR_Timer::Instance()->AddTimer(GR_SentinelEventReconnectCB, this, 10, true);
    }
    return GR_ERROR;
}


// 短线重连
int GR_SentinelEvent::Close(bool bForceClose)
{
    GR_RedisEvent::Close(bForceClose);
    return GR_OK;
}

GR_SentinelMgr::GR_SentinelMgr(GR_TwemRoute *pRoute)
{
    m_pRoute = pRoute;
}

GR_SentinelMgr::~GR_SentinelMgr()
{
}

int GR_SentinelMgr::ConnectToSentinel()
{
    int iRet;
    GR_RedisServer *pServer;
    auto itr=this->m_pRoute->m_listSentinels.begin();
    for (; itr!=this->m_pRoute->m_listSentinels.end(); itr++)
    {
        pServer = *itr;
        pServer->pEvent = new GR_SentinelEvent(pServer);
        iRet = pServer->pEvent->ConnectToRedis(pServer->iPort, pServer->strHostname.c_str());
        if (iRet != GR_OK)
        {
            GR_LOGE("connect to redis failed:%s", pServer->strInfo.c_str());
            return GR_ERROR;
        }
    }
    return GR_OK;
}

int GR_SentinelMgr::SentinelConnected(GR_SentinelEvent *pEvent)
{
    if (this->m_iInitFlag != 0)
    {
        return GR_OK;
    }
    int iRet;
    GR_RedisServer *pServer;
    for (int i=0; i<this->m_pRoute->m_iSrvNum; i++)
    {
        pServer = this->m_pRoute->m_vServers[i];
        ASSERT(pServer != nullptr);
        auto pData = GR_MsgProcess::Instance()->SentinelGetAddrByName(pServer->strSentinelName);
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
        pIdenty->pAccessEvent = pEvent;
        pIdenty->uiCmdType = GR_CMD_SENTINEL_GET_MASTER;
        pIdenty->pCB = pServer;
        iRet = pEvent->SendMsg(pData, pIdenty);
        // 获取redis地址
        if (iRet != GR_OK)
        {
            GR_LOGE("connect to redis failed:%s", pServer->strInfo.c_str());
            return GR_ERROR;
        }
    }
    return GR_OK;
}


