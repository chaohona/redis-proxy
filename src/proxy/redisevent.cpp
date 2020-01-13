#include "redisevent.h"
#include "include.h"
#include "redismgr.h"
#include "redismsg.h"
#include "gr_signal.h"
#include "gr_proxy_global.h"
#include "proxy.h"
#include "gr_clusterroute.h"
#include "gr_masterinfo_cfg.h"

int GR_RedisEventStatusCB(GR_TimerMeta * pMeta, void *pCB)
{
    GR_RedisEvent *pEvent = (GR_RedisEvent*)pCB;
    pEvent->ConnectResult();
    if (pMeta != nullptr)
    {
        pMeta->Finish();
    }
    return GR_OK;
}


// 原生集群模式不走这里
int GR_RedisEventReconnectCB(GR_TimerMeta * pMeta, void *pCB)
{
    GR_RedisServer *pServer = (GR_RedisServer *)pCB;
    ASSERT(pServer!=nullptr && pServer->pEvent!=nullptr);
    if (pServer->strSentinelName == "")
    {
        pServer->pEvent->ReConnect(pMeta); // 目前twem模式会走到这里
    }
    else // sentinel模式，走重新获取redis地址流程
    {
        if (GR_OK == pServer->Connect())
        {
            GR_LOGI("reconnect to redis success %s:%d", pServer->strAddr.c_str(), pServer->iPort);
            return GR_OK;
        }
        GR_LOGE("reconnect to redis failed %s:%d", pServer->strAddr.c_str(), pServer->iPort);
        return GR_ERROR;
    }
    return GR_OK;
}

// TODO 改成先new出来之后单独init，防止构造函数抛异常
GR_RedisEvent::GR_RedisEvent(int iFD, sockaddr &sa, socklen_t &salen)
{
    this->Init();
}

GR_RedisEvent::GR_RedisEvent(GR_RedisServer *pServer)
{
    this->Init();
    this->m_pServer = pServer;
}

GR_RedisEvent::GR_RedisEvent()
{
    this->Init();
}

int GR_RedisEvent::Init()
{
    this->m_Status = GR_CONNECT_INIT;
    this->m_ReadCache.Init(REDIS_MSG_INIT_BUFFER);
    this->m_ReadMsg.Init((char*)this->m_ReadCache.m_pData->m_uszData);

    this->m_pWaitRing = new RingBuffer(MAX_REDIS_MSG_IDENTY_POOL);
    this->m_vCiov     = new iovec[MAX_REDIS_MSG_IDENTY_POOL];

    return GR_OK;
}

int GR_RedisEvent::ReInit()
{
    this->m_lCiovReadIdx = 0;
    this->m_lCiovWriteIdx = 0;
    this->m_pWaitRing->ReInit();
    this->m_ReadCache.ReInit();
    this->m_ReadMsg.ReInit(this->m_ReadCache.m_pData->m_uszData);
    return GR_OK;
}

GR_RedisEvent::~GR_RedisEvent()
{
    try
    {
        if (this->m_pWaitRing != nullptr)
        {
            delete this->m_pWaitRing;
            this->m_pWaitRing = nullptr;
        }
        if (this->m_vCiov != nullptr)
        {
            delete []this->m_vCiov;
            this->m_vCiov = nullptr;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("close redis connection got exception:%s", e.what());
    }
}

int GR_RedisEvent::TTCheck()
{
    // 初始状态不检查
    if (this->m_Status == GR_CONNECT_INIT || this->m_ulFirstMsgMS == 0)
    {
        return GR_OK;
    }
    // 如果响应超时，则和redis断开连接，重新连接
    uint64 ulNow = CURRENT_MS();
    if (ulNow - this->m_ulFirstMsgMS > this->m_iRedisRspTT)
    {
        this->Close();
        GR_LOGE("redis response tt %uld, %uld, %d", ulNow,  this->m_ulFirstMsgMS, this->m_iRedisRspTT);
        return GR_OK;
    }
    return GR_OK;
}

int GR_RedisEvent::SendMsg(GR_RedisMsg *pMsg, GR_MsgIdenty *pIdenty)
{
    try
    {
        ASSERT(pMsg!=nullptr && pIdenty!=nullptr);
        GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(pMsg->m_Info.iLen);
        strncpy(pData->m_uszData, pMsg->szStart, pMsg->m_Info.iLen);
        pIdenty->pReqData = pData;
        return this->SendMsg(pData, pIdenty);
    }
    catch(exception &e)
    {
        GR_LOGE("copy data got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_RedisEvent::SendMsg(const char *szData, int iLen, GR_MsgIdenty *pIdenty)
{
    try
    {
        GR_MemPoolData *pData = GR_MEMPOOL_GETDATA(iLen);
        strncpy(pData->m_uszData, szData, iLen);
        pIdenty->pReqData = pData;
        return this->SendMsg(pData, pIdenty);
    }
    catch(exception &e)
    {
        GR_LOGE("copy data got exception:%s", e.what());
        return GR_ERROR;
    }
    
    return GR_OK;
}

int GR_RedisEvent::SendMsg(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    pIdenty->pReqData = pData;
    if (this->m_Status != GR_CONNECT_CONNECTING && this->m_Status != GR_CONNECT_CONNECTED) // 正在连接中，可以将消息缓存在waitring中
    {
        GR_LOGD("not connect with redis %s:%d", this->m_szAddr, this->m_uiPort);
        pIdenty->ReplyError(REDIS_RSP_DISCONNECT);
        //ASSERT(false);
        return GR_OK;
    }
    if (this->m_lCiovWriteIdx - this->m_lCiovReadIdx == MAX_REDIS_MSG_IDENTY_POOL)
    {
        // TODO 告警
        GR_LOGE("waiting buffer is full %s:%d", this->m_szAddr, this->m_uiPort);
                ASSERT(false);
        return GR_FULL;
    }
    if (!this->m_pWaitRing->AddData((char*)pIdenty))
    {
        GR_LOGE("waiting buffer is full %s:%d", this->m_szAddr, this->m_uiPort);
        ASSERT(false);
        return GR_FULL;
    }
    pIdenty->pRedisEvent = this; // 已经将标记放到redis的等待队列了
    int iIdx = this->m_lCiovWriteIdx%MAX_REDIS_MSG_IDENTY_POOL;
    ++this->m_lCiovWriteIdx;
    ASSERT(this->m_lCiovWriteIdx > this->m_lCiovReadIdx && m_lCiovWriteIdx-m_lCiovReadIdx<=MAX_REDIS_MSG_IDENTY_POOL);
    if (this->m_lCiovWriteIdx > 0xFFFFFFF) // 防止溢出
    {
        int iDiff = this->m_lCiovWriteIdx - this->m_lCiovReadIdx;
        this->m_lCiovReadIdx = m_lCiovReadIdx%MAX_REDIS_MSG_IDENTY_POOL;
        this->m_lCiovWriteIdx = this->m_lCiovReadIdx + iDiff;
    }
    iovec * iov = this->m_vCiov + iIdx;
    iov->iov_base = pData->m_uszData;
    iov->iov_len = pData->m_sUsedSize;
    if (this->m_ulFirstMsgMS == 0) // 超时判断不在这儿做，这个函数调用太频繁
    {
        this->m_ulFirstMsgMS = pIdenty->ulReqMS;
    }
    // 加入写事件触发
    GR_EPOLL_ADDWRITE(this);
    
    return GR_OK;
}

int GR_RedisEvent::Write()
{
    if (this->m_Status != GR_CONNECT_CONNECTED) // 连接还没有成功
    {
        if (GR_OK != this->ConnectResult())
        {
            return GR_ERROR;
        }
    }
    ASSERT(this->m_lCiovWriteIdx >= this->m_lCiovReadIdx && m_lCiovWriteIdx-m_lCiovReadIdx<=MAX_REDIS_MSG_IDENTY_POOL);
    if (this->m_lCiovWriteIdx == this->m_lCiovReadIdx) // 没有数据需要发送
    {
        GR_EPOLL_DELWRITE(this);
        return GR_OK;
    }
    int iSendNum = 0;
    int iWriteIdx = m_lCiovWriteIdx%MAX_REDIS_MSG_IDENTY_POOL;
    int iReadIdx = m_lCiovReadIdx%MAX_REDIS_MSG_IDENTY_POOL;
    if (iWriteIdx > iReadIdx)
    {
        return this->Sendv(this->m_vCiov+iReadIdx, iWriteIdx-iReadIdx);
    }
    else
    {
        int iRet = this->Sendv(this->m_vCiov+iReadIdx, MAX_REDIS_MSG_IDENTY_POOL-iReadIdx);
        if (iRet != GR_OK)
        {
            return iRet;
        }
        if (iWriteIdx == 0)
        {
            return GR_OK;
        }
        return this->Sendv(this->m_vCiov, iWriteIdx);
    }
    
    return GR_OK;
}

int GR_RedisEvent::Sendv(iovec * iov, int nsend)
{
    if (nsend == 0)
    {
        ASSERT(false);
        return GR_OK;
    }
    int iSendRet = this->DoSendv(iov, nsend);
    if (iSendRet < 0)
    {
        if (iSendRet != GR_EAGAIN)
        {
            this->Close();
            return GR_ERROR;
        }
        return iSendRet;
    }
    if (iSendRet == 0)
    {
        return GR_OK;
    }
    iovec *pEndIov = iov + nsend;
    for(; iov < pEndIov; ++iov)
    {
        iSendRet -= iov->iov_len;
        // 没发送完
        if (iSendRet < 0)
        {
            if (-iSendRet == iov->iov_len)
            {
                return GR_EAGAIN;
            }
            iov->iov_base = (char*)iov->iov_base + iSendRet + iov->iov_len;
            iov->iov_len = -iSendRet;
            return GR_EAGAIN;
        }
        ++this->m_lCiovReadIdx;
    }

    GR_EPOLL_DELWRITE(this);
    return GR_OK;
}

#define IS_MOVED_REDIRECT(msg)\
(msg.m_Info.iLen>5 && IS_MOVED_ERR(msg.szStart))
#define IS_ASK_REDIRECT(msg)\
(msg.m_Info.iLen>3 && IS_ASK_ERR(msg.szStart))

int GR_RedisEvent::ProcessMsg(int &iNum)
{
    int iParseRet = 0;
    GR_MsgIdenty *pIdenty;
    GR_MsgIdenty *pFirstIdenty;
    bool bReleaseData = true;
    for (;;)
    {
        // 解析消息
        iParseRet = this->m_ReadMsg.ParseRsp();
        if (iParseRet != GR_OK)
        {
            GR_LOGE("parse redis msg failed %s:%d %d", this->m_szAddr, this->m_uiPort, iParseRet);
            this->Close();
            ASSERT(false);
            return GR_ERROR;
        }
        if (this->m_ReadMsg.m_Info.nowState != GR_END)
        {
            break;
        }
        iNum += 1;
        // GR_LOGD("got a message from redis:%s", this->m_ReadMsg.szStart);
        // 获取当前消息所属的客户端
        // 将消息放入客户端的发送队列
        GR_RB_POPFRONT(GR_MsgIdenty*, pIdenty, this->m_pWaitRing);
        //pIdenty = (GR_MsgIdenty *)this->m_pWaitRing->PopFront();
        if (pIdenty == nullptr)
        {
            GR_LOGE("should not happen, address %s:%d", this->m_szAddr,this->m_uiPort);
            ASSERT(pIdenty != nullptr);
            this->Close();
            return GR_ERROR;
        }
        // 更新最早一个消息的发送时间
        //pFirstIdenty = (GR_MsgIdenty *)this->m_pWaitRing->GetFront();
        GR_RB_GETFRONT(GR_MsgIdenty*, pFirstIdenty, this->m_pWaitRing);
        if (pFirstIdenty != nullptr)
        {
            this->m_ulFirstMsgMS = pFirstIdenty->ulReqMS;
        }
        else
        {
            this->m_ulFirstMsgMS = 0;
        }
        this->m_ReadCache.m_pData->m_sUsedSize = this->m_ReadMsg.m_Info.iLen;
        // 这个地方比较简单，就不用多态实现了
        if (PROXY_ROUTE_CLUSTER == GR_PROXY_INSTANCE()->m_iRouteMode && this->SlotPending(pIdenty))
        {
            this->StartNext(true); // 此处响应不需要继续使用了，释放响应内存
            continue;
        }
        pIdenty->GetReply(this->m_ReadCache.m_pData);
        pIdenty->Release(REDIS_RELEASE);

        this->StartNext(false); // 此处响应被放到客户端发送队列了，由客户端释放内存
        continue;
    }
    return GR_OK;
}

#define REINIT_REDIRECT_MSG(msg)                    \
msg.StartNext();                                    \
msg.ReInit(this->m_ReadCache.m_pData->m_uszData);   \
msg.Expand(this->m_ReadMsg.m_Info.iLen)

bool GR_RedisEvent::SlotPending(GR_MsgIdenty *pIdenty)
{
    try
    {
        GR_ClusterRoute *pRoute = (GR_ClusterRoute*)GR_RedisMgr::Instance()->m_pRoute;
        if (this->m_ReadMsg.m_Info.iErrFlag == 0)
        {
            if (pIdenty->bRedirect)
            {
                pRoute->SlotChanged(pIdenty, this);
            }
            return false;
        }
        static GR_RedisMsg g_global_redirect_msg;
        // 如果是重定向消息则重定向
        if (IS_MOVED_REDIRECT(this->m_ReadMsg))
        {
            pIdenty->bRedirect = true;
            pIdenty->iMovedTimes += 1;
            if (pIdenty->iMovedTimes > 1)
            {
                // TODO 告警，说明此时有元数据状态不一致的情况:系统不应该出现
                GR_LOGW("moved more than 1 times %d, %d, %s, %d", pIdenty->iSlot, pIdenty->iMovedTimes, this->m_szAddr, 
                    this->m_uiPort);
                //if (pIdenty->iMovedTimes > 2)
                //{
                //    pIdenty->ReplyError(REDIS_RSP_MOVED);
                //    return true;
                //}
            }
            REINIT_REDIRECT_MSG(g_global_redirect_msg);
            if (GR_OK != pRoute->SlotRedirect(pIdenty, g_global_redirect_msg))
            {
                pIdenty->ReplyError(REDIS_RSP_COMM_ERR);
            }
            return true;
        } 
        else if (IS_ASK_REDIRECT(this->m_ReadMsg))// 如果是ask消息则不更新，但是重新向新的redis发送请求
        {
            pIdenty->bRedirect = false;
            pIdenty->iAskTimes += 1;
            if (pIdenty->iAskTimes > 1)
            {
                // TODO 告警，说明此时有元数据状态不一致的情况:系统不应该出现
                GR_LOGE("ask more than 1 times %d, %d, %s, %d", pIdenty->iSlot, pIdenty->iAskTimes, this->m_szAddr, 
                    this->m_uiPort);
            //    pIdenty->ReplyError(REDIS_RSP_MOVED);
            //    return true;
            }
            REINIT_REDIRECT_MSG(g_global_redirect_msg);
            if (GR_OK != pRoute->SlotRedirect(pIdenty, g_global_redirect_msg, true))
            {
                pIdenty->ReplyError(REDIS_RSP_COMM_ERR);
            }
            return true;
        }
        // slot对应的redis信息正确
        if (pIdenty->bRedirect)
        {
            pRoute->SlotChanged(pIdenty, this);
        }
        
        return false;
    }
    catch(exception &e)
    {
        GR_LOGE("parse redirect message got exception:%s", e.what());
        return false;
    }
}

int GR_RedisEvent::Read()
{
    if (this->m_iFD <= 0)
    {
        GR_LOGD("try to read from nagtive fd %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    // 将消息读到缓存中，并解析消息，如果解析完一条则开始转发这条消息
    // 如果内存不够解析一条消息的则移动数据
    // 如果消息已经占内存的一半则申请一个更大的缓存，否则在此缓存中直接移动
    int iMaxMsgRead = 1000; // 每次最多接收1000条消息则让出cpu
    int iRead;              // 本次读取的字节数
    int iLeft = 0;
    int iParseRet = 0;
    bool bReleaseData = true;
    GR_MsgIdenty *pIdenty;
    int iProcNum;
    int iRet;
    do{
        iLeft = m_ReadCache.LeftCapcityToEnd();
        iRead = read(this->m_iFD, m_ReadCache.m_szMsgEnd, iLeft);
        if (iRead < 0)
        {
            if (errno == EINTR)
            {
                GR_LOGD("recv from redis not ready - eintr, %s:%d", this->m_szAddr, this->m_uiPort);
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                GR_LOGD("recv from redis not ready - eagain, %s:%d", this->m_szAddr, this->m_uiPort);
                return GR_EAGAIN;
            }
            GR_LOGE("redis  %s:%d read failed:%d, errmsg:%s", this->m_szAddr, this->m_uiPort, errno, strerror(errno));
            this->Close(false);
            return GR_ERROR;
        }
        else if (iRead == 0)
        {
            GR_LOGE("redis close the connection, %d %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            this->Close();
            return GR_ERROR;
        }
        // GR_LOGD("get data from redis, len:%d", iRead);
        this->m_ReadMsg.Expand(iRead);
        this->m_ReadCache.Write(iRead);
        iProcNum = 0;
        iRet = this->ProcessMsg(iProcNum);
        if (iRet != GR_OK)
        {
            return iRet;
        }
        if (iRead < iLeft) // 数据已经读完了
        {
            break;
        }
         if (this->m_ReadMsg.m_Info.iMaxLen+256 > this->m_ReadCache.m_pData->m_sCapacity)
        {
            this->m_ReadCache.Expand(this->m_ReadMsg.m_Info.iMaxLen+256);
            this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
        }
        else
        {
            // 剩余空间小于一半则扩容
            if (this->m_ReadCache.LeftCapcity() < this->m_ReadCache.m_pData->m_sCapacity>>1)
            {
                // 空间不够存储当前消息，扩展消息的缓存
                this->m_ReadCache.Expand();
                this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
            }
            // 否则将剩余数据拷贝到内存池开头
            else
            {
                this->m_ReadCache.ResetBuffer();
                this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
            }
        }

        iMaxMsgRead -= iProcNum;
    } while(iMaxMsgRead > 0);
    return 0;
}

int GR_RedisEvent::StartNext(bool bReleaseData)
{
    // TODO 判断空闲内存是否过大，如果过大则缩小重新申请内存的规格
    MSGBUFFER_READ(this->m_ReadCache, this->m_ReadMsg.m_Info.iLen);
    //this->m_ReadCache.Read(this->m_ReadMsg.m_Info.iLen);
    this->m_ReadCache.ResetMemPool(this->m_ReadCache.m_pData->m_sCapacity, bReleaseData);
    this->m_ReadMsg.StartNext();
    this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
    return GR_OK;
}


int GR_RedisEvent::Error()
{
    this->Close(false);
    return GR_ERROR; // 返回ERR，代表处理出错，此事件不能在继续往下面执行了
}

// 将缓存的需要返回的消息返回给客户端
int GR_RedisEvent::Close(bool bForceClose)
{
    if (this->m_iFD <= 0)
    {
        return GR_OK;
    }
    if (this->pConnectingMeta != nullptr)
    {
        this->pConnectingMeta->Finish();
        this->pConnectingMeta = nullptr;
    }
    this->m_Status = GR_CONNECT_CLOSED;
    try
    {
        GR_Event::Close();
        if (this->m_iRedisType == REDIS_TYPE_MASTER)
        {
            GR_RedisMgr::Instance()->m_pRoute->DelRedis(this);
        }
        // TODO 向没有获取到响应的请求发送错误响应
        if (this->m_pWaitRing != nullptr)
        {
            GR_MsgIdenty *pIdenty = (GR_MsgIdenty *)this->m_pWaitRing->PopFront();
            for(; pIdenty!=nullptr; )
            {
                pIdenty->ReplyError(REDIS_RSP_DISCONNECT);
                pIdenty->Release(REDIS_RELEASE);
                pIdenty = (GR_MsgIdenty *)this->m_pWaitRing->PopFront();
            }
        }

        // 断线重连
        if (gr_global_signal.Exit == 0 && !bForceClose)
        {
            // cluster模式断线重连走ReInit流程
            if (GR_Proxy::Instance()->m_iRouteMode == PROXY_ROUTE_CLUSTER)
            {
                if (GR_OK != GR_RedisMgr::Instance()->m_pRoute->ReInit())
                {
                    GR_LOGE("reconnect redis cluster flow failed.");
                }
            }
            else
            {
                this->ReConnect();
            }
        }
        GR_LOGI("close with redis success %d %s:%d", this->m_iRedisType, this->m_szAddr, this->m_uiPort);
    }
    catch(exception &e)
    {
        GR_LOGE("close redis connection %s:%d got exception:%s", this->m_szAddr, this->m_uiPort,e.what());
        return GR_ERROR;
    }
    
    return GR_OK;
}

int GR_RedisEvent::ConnectToRedis(uint16 uiPort, const char *szIP)
{
    ASSERT(this->m_iRedisType!=REDIS_TYPE_NONE);
    try
    {
        GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
        this->m_iRedisRspTT = pConfig->m_iRedisRspTT;
        this->m_uiPort = uiPort;
        strncpy(this->m_szAddr, szIP, NET_IP_STR_LEN);
        int iFD = socket(AF_INET, SOCK_STREAM, 0);
        if (iFD < 0)
        {
            GR_LOGE("create socket failed:%s,%d, errno:%d,errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return GR_ERROR;
        }
        sockaddr_in server_address;
        bzero( &server_address, sizeof( server_address ));
        server_address.sin_family = AF_INET;
        inet_pton(AF_INET, szIP, &server_address.sin_addr);
        server_address.sin_port = htons(uiPort);

        int iRet = GR_Socket::SetNonBlocking(iFD);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket nonblock failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        iRet = GR_Socket::SetTcpNoDelay(iFD);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket tcpnodelay failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        iRet = GR_Socket::SetSndTimeO(iFD, 100);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket send time out failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        iRet = GR_Socket::SetRcvTimeO(iFD, 100);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket recv time out failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        iRet = GR_Socket::SetSynCnt(iFD, 2);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket syn count failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        // 和redis的buffer设置大一点,测试下来一次最多只能接收127K
        iRet = GR_Socket::SetRevBuf(iFD, 512*1024);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket tcpnodelay failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        iRet = GR_Socket::SetTcpKeepAlive(iFD);
        if (iRet != GR_OK)
        {
            GR_LOGE("set socket SetTcpKeepAlive failed:%s,%d, errno:%d, errmsg:%s", szIP, uiPort, errno, strerror(errno));
            return iRet;
        }
        this->m_iFD = iFD;
        GR_Epoll::Instance()->AddEventRW(this);
        iRet = connect(iFD, (sockaddr*)&server_address, sizeof(server_address));
        if (iRet != GR_OK)
        {
            if (errno == EINPROGRESS)
            {
                this->m_Status = GR_CONNECT_CONNECTING;
                GR_LOGE("fd %d connecting to server %s:%d ", iFD, szIP, uiPort);
                // 1秒之后判断连接是否可读
                pConnectingMeta = GR_Timer::Instance()->AddTimer(GR_RedisEventStatusCB, this, 1000, true);
                return GR_OK;
            }
            GR_LOGE("connect to redis failed %d %s:%d, errno %d, errmsg %s", this->m_iRedisType, szIP, uiPort, errno, strerror(errno));
            this->ConnectFailed();
            return iRet;
        }

        this->ConnectResult();
    }
    catch(exception &e)
    {
        this->ConnectFailed();
        GR_LOGE("connect to redis %s:%d got exception %s", szIP, uiPort, e.what());
        return GR_ERROR;
    }
    
    return GR_OK;
}

int GR_RedisEvent::ReConnect(GR_TimerMeta *pMeta)
{
    uint64 ulNow = CURRENT_MS();
    if (ulNow < this->m_ulNextTry) // 没到重试时间
    {
        if (pMeta == nullptr)
        {
            // 创建自动重连的回调函数
            pMeta = GR_Timer::Instance()->AddTimer(GR_RedisEventReconnectCB, this->m_pServer, 10, true);
        }
        return GR_OK;
    }
    this->m_ulMaxTryConnect += 1;
    uint64 tmpMS = this->m_ulMaxTryConnect * GR_REDIS_DEF_RETRY_MS;
    if (tmpMS > 1000) // 最多间隔一秒重试
    {
        tmpMS = 1000;
    }
    this->m_ulNextTry = ulNow + tmpMS;
    this->ReInit();
    GR_LOGI("try to reconnect to redis %s:%d", this->m_szAddr, this->m_uiPort);
    if (GR_OK == this->ConnectToRedis(this->m_uiPort, this->m_szAddr))
    {
        if (pMeta != nullptr)
        {
            pMeta->Finish();
        }
        return GR_OK;
    }

    if (pMeta == nullptr)
    {
        // 创建自动重连的回调函数
        pMeta = GR_Timer::Instance()->AddTimer(GR_RedisEventReconnectCB, this->m_pServer, tmpMS, true);
    }
    else
    {
        GR_Timer::Instance()->SetTriggerMS(pMeta, this->m_ulNextTry);
    }
    return GR_OK;
}

int GR_RedisEvent::ConnectResult()
{
    if (this->m_Status != GR_CONNECT_CONNECTING)
    {
        return GR_OK;
    }
    if (!GR_Socket::ConnectedCheck(this->m_iFD))
    {
        this->Close();
        GR_LOGD("connect result is failed %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    // 检查是否连接成功，不成功则重新连接
    this->m_Status = GR_CONNECT_CONNECTED;
    this->m_ulMaxTryConnect = 0;
    GR_Epoll::Instance()->AddEventRead(this);
    GR_LOGI("connect to redis success,%s:%d", this->m_szAddr, this->m_uiPort);
    this->ConnectSuccess();
    return GR_OK;
}

int GR_RedisEvent::ConnectSuccess()
{
    if (this->pConnectingMeta != nullptr)
    {
        this->pConnectingMeta->Finish();
        this->pConnectingMeta = nullptr;
    }
    GR_RedisMgr::Instance()->RedisConnected(this->m_uiPort, this->m_szAddr);
    // TODO 1、如果是cluster模式则检查是否所有的slot都有对应的redis，都有则启动监听端口
    //  2、如果是twem模式，则所有的分片redis都连接上，则启动监听
    return GR_OK;
}

int GR_RedisEvent::ConnectFailed()
{
    if (this->pConnectingMeta != nullptr)
    {
        this->pConnectingMeta->Finish();
        this->pConnectingMeta = nullptr;
    }
    // 如果是cluster模式则不走这里
    this->Close(); // 先清除之前的数据，重新建立连接
    if (GR_Proxy::Instance()->m_iRouteMode == PROXY_ROUTE_CLUSTER)
    {
        // 如果是从则不重连
        // 如果是主责需要重新连接
        if (this->m_iRedisType == REDIS_TYPE_MASTER)
        {
            GR_RedisMgr::Instance()->m_pRoute->ReInit();
        }
    }
    else
    {
        this->ReConnect();
    }
    
    return GR_OK;
}

GR_RedisServer::GR_RedisServer()
{
    memset(this->m_vSlaves, 0, sizeof(GR_RedisServer*)*MAX_REDIS_SLAVES);
}

GR_RedisServer::~GR_RedisServer()
{
    try
    {
        if (this->pEvent != nullptr)
        {
            this->pEvent->Close(true);
            delete this->pEvent; // TODO 连接池子
            this->pEvent = nullptr;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("destroy redis server got exception:%s", e.what());
    }
}

bool GR_RedisServer::operator == (GR_RedisServer &other)
{
    if (this->strName != "")
    {
        return this->strAddr == other.strAddr || this->strName == other.strName;
    }
    return this->strAddr == other.strAddr;
}

int GR_RedisServer::Connect()
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
        this->pEvent = new GR_RedisEvent(this);
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
