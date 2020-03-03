#include "accesslayer.h"
#include "include.h"
#include "redismgr.h"
#include "proxy.h"
#include "gr_proxy_global.h"
#include "event.h"
#include "gr_tiny.h"
#include "gr_clusterroute.h"

GR_AccessEvent::GR_AccessEvent(int iFD, sockaddr & sa, socklen_t &salen, GR_AccessMgr* pMgr)
{
    this->m_iFD = iFD;
    sockaddr_in *s = (sockaddr_in *)&sa;
    inet_ntop(AF_INET, (void*)&(s->sin_addr), this->m_szAddr, NET_IP_STR_LEN);
    this->m_uiPort = ntohs(s->sin_port);
    this->m_pMgr = pMgr;
}

GR_AccessEvent::GR_AccessEvent()
{

}

int GR_AccessEvent::Init()
{
    try
    {
        this->m_ulMsgIdenty = 0;

        this->m_ReadCache.Init(REDIS_MSG_INIT_BUFFER);

        this->m_pWaitRing   = new RingBuffer(MAX_WAIT_RING_LEN);
        this->m_vCiov       = new iovec[MAX_WAIT_RING_LEN];

        // 将缓存的起始地址赋值给msg
        this->m_ReadMsg.Init((char*)this->m_ReadCache.m_pData->m_uszData);
    }
    catch(exception &e)
    {
        GR_LOGE("init accesss event got exception:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

GR_AccessEvent::~GR_AccessEvent()
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
        GR_LOGE("destroy access event got exception:%s", e.what());        
    }
}

int GR_AccessEvent::GetReply(int iErr, GR_MsgIdenty *pIdenty)
{
    GR_MemPoolData *pData = GR_MsgProcess::Instance()->GetErrMsg(iErr);
    pIdenty->pData = nullptr;
    return this->DoReply(pData, pIdenty);
}

int GR_AccessEvent::GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    pIdenty->pData = pData; // pData所有权属于pIdenty
    return this->DoReply(pData, pIdenty);
}

int GR_AccessEvent::DoReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    static uint64 global_process_msg_num = 0;
    ++global_process_msg_num;
    if (global_process_msg_num % 10000 == 0)
    {
        GR_LOGD("process msg num %ld", global_process_msg_num);
    }
    pIdenty->iGotNum += 1;
    if (pIdenty->iNeedWaitNum > 1 && pIdenty->iGotNum < pIdenty->iNeedWaitNum)
    {
        return GR_OK;
    }
    int iIndex = pIdenty->ulIndex % MAX_WAIT_RING_LEN;
    pIdenty->iWaitDone = 1;
    this->m_vCiov[iIndex].iov_base = pData->m_uszData;
    this->m_vCiov[iIndex].iov_len = pData->m_sUsedSize;
    // 如果iIndex之前的消息全部收到响应了则发送响应给客户端
    GR_MsgIdenty *pPreIdenty = (GR_MsgIdenty *)this->m_pWaitRing->GetPre(pIdenty->ulIndex);
    if (pPreIdenty == nullptr || pPreIdenty->iWaitRsp == 0 || pPreIdenty->iWaitDone == 1 || pPreIdenty->iPreGood == 1)
    {
        // 触发写事件
        GR_EPOLL_ADDWRITE(this);
        return GR_OK;
    }
    return GR_OK;
}

int GR_AccessEvent::Sendv(iovec * iov, int nsend)
{
    if (nsend == 0)
    {
        return GR_OK;
    }
    int iSendRet = this->DoSendv(iov, nsend);
    if (iSendRet <=0)
    {
        if (iSendRet != GR_EAGAIN)
        {
            this->Close();
            return GR_ERROR;
        }
        return iSendRet;
    }
    GR_MsgIdenty* pIdenty;
    iovec *pEndIov = iov + nsend;
    for(; iov<pEndIov; ++iov)
    {
        iSendRet -= iov->iov_len;
        // 没发送完
        if (iSendRet < 0)
        {
            if (-iSendRet == iov->iov_len)
            {
                return GR_EAGAIN;
            }
            iov->iov_base = iov->iov_base + iSendRet + iov->iov_len;
            iov->iov_len = -iSendRet;
            return GR_EAGAIN;
        }
        // 已经完成一个生命周期了，归还资源
        //pIdenty = (GR_MsgIdenty*)this->m_pWaitRing->PopFront();
        GR_RB_POPFRONT(GR_MsgIdenty*, pIdenty, this->m_pWaitRing);
        ASSERT(pIdenty!=nullptr);
        pIdenty->Release(ACCESS_RELEASE);
    }
    return GR_OK;
}

int GR_AccessEvent::Write()
{
    if (this->m_iFD <= 0)
    {
        return GR_OK;
    }
    // 循环遍历等待响应环，将准备好的消息全部发送出去
    GR_MsgIdenty *pIdenty = nullptr;
    GR_RB_GETFRONT(GR_MsgIdenty*, pIdenty, this->m_pWaitRing);
    if (pIdenty == nullptr)
    {
        GR_EPOLL_DELWRITE(this);
        return GR_OK;
    }
    int iWaitNum = this->m_pWaitRing->GetNum();
    int iSendNum = 0;
    int iRet = GR_OK;
    uint64 ulNextIndex = 0;
    int iStart = pIdenty->ulIndex%MAX_WAIT_RING_LEN;
    for (int i=0; i<iWaitNum; i++)
    {
        ulNextIndex = pIdenty->ulIndex+1;
        ASSERT(pIdenty->pAccessEvent!=nullptr && pIdenty->pAccessEvent==this);
        pIdenty->iPreGood = 1;
        if (pIdenty->iWaitDone == 1) // 获取到响应消息了
        {
            iSendNum += 1;
        }
        else
        {
            break;
        }
        // 如果已经到结尾了则发送
        if (((pIdenty->ulIndex+1)%MAX_WAIT_RING_LEN) == 0)
        {
            iRet = this->Sendv(this->m_vCiov+iStart, iSendNum);
            if (GR_OK != iRet) // 发送完了，继续发送
            {
                return iRet;
            }
            iSendNum = 0;
            iStart = 0;
            ulNextIndex = 0;
        }
        GR_RB_GETDATA(GR_MsgIdenty*, pIdenty, ulNextIndex, this->m_pWaitRing);
        //pIdenty = (GR_MsgIdenty *)this->m_pWaitRing->GetData(ulNextIndex);
        if (pIdenty == nullptr)
        {
            break;
        }
    }
    if (iSendNum > 0)
    {
        iRet = this->Sendv(this->m_vCiov+iStart, iSendNum);
        if (iRet == GR_OK)
        {
            GR_EPOLL_DELWRITE(this);
        }
        else
        {
            return iRet;
        }
    }
    
    return iRet;
}

// TODO 请求消息的空间不复用了，直接一次性把空间申请好
int GR_AccessEvent::StartNext(bool bRealseReq)
{
    /*if (pIdenty==nullptr || pIdenty->pReqData==nullptr) // 不是扩容期间，没有缓存数据，空间可以继续例用
    {
        this->m_ReadCache.Read(this->m_ReadMsg.m_Info.iLen);
        this->m_ReadMsg.StartNext();
        return GR_OK;
    }*/

    // TODO 判断空闲内存是否过大，如果过大则缩小重新申请内存的规格
    MSGBUFFER_READ(this->m_ReadCache, this->m_ReadMsg.m_Info.iLen);
    //this->m_ReadCache.Read(this->m_ReadMsg.m_Info.iLen);
    // pIdenty->pReqData==nullptr说明请求不需要缓存，此处回收请求所占用内存
    this->m_ReadCache.ResetMemPool(this->m_ReadCache.m_pData->m_sCapacity, bRealseReq);
    this->m_ReadMsg.StartNext();
    this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
    return GR_OK;
}

// 读取网络消息，解析
int GR_AccessEvent::Read()
{
    // 已经关闭了
    if (this->m_iFD <= 0 )
    {
        return GR_ERROR;
    }
    // 积攒的消息太多，处理不过来了则关闭客户端
    if (RINGBUFFER_POOLFULL(this->m_pWaitRing))
    //if (this->m_pWaitRing->PoolFull())
    {
        // TODO 告警
        //ASSERT(false);
        GR_LOGE("wait for response pool is full, fd %d, addr %s:%d", this-m_iFD, this->m_szAddr, this->m_uiPort);
        this->Close();
        return GR_ERROR;
    }
    m_ulActiveMS = CURRENT_MS();
    // 将消息读到缓存中，并解析消息，如果解析完一条则开始转发这条消息
    // 如果内存不够解析一条消息的则移动数据
    // 如果消息已经占内存的一半则申请一个更大的缓存，否则在此缓存中直接移动
    int iMaxMsgRead = 1000; // 每次最多接收1000条消息则让出cpu
    int iRead;              // 本次读取的字节数
    int iLeft = 0;
    int iRet = 0;
    int iProcNum = 0;
    do{
        // iLeft = m_ReadCache.LeftCapcityToEnd();
        iLeft = m_ReadCache.m_pData->m_uszEnd - m_ReadCache.m_szMsgEnd;
        iRead = read(this->m_iFD, m_ReadCache.m_szMsgEnd, iLeft);
        if (iRead < 0)
        {
            if (errno == EINTR)
            {
                GR_LOGD("recv from client not ready - eintr, %s:%d", this->m_szAddr, this->m_uiPort);
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                GR_LOGD("recv from client not ready - eagain, %s:%d", this->m_szAddr, this->m_uiPort);
                return GR_EAGAIN;
            }
            this->Close();
            //GR_LOGE("client read failed:%d, errmsg:%s", errno, strerror(errno));
            return GR_ERROR;
        }
        else if (iRead == 0)
        {
            GR_LOGE("client close the connection %s:%d", this->m_szAddr, this->m_uiPort);
            this->Close();
            return GR_ERROR;
        }
        if (iRead > 0)
        {
            REDISMSG_EXPAND(this->m_ReadMsg, iRead);
            //this->m_ReadMsg.Expand(iRead);
            // this->m_ReadMsg.szEnd = this->m_ReadMsg.szEnd + iRead;
            // for (pTmp=this->m_ReadMsg.pNextLayer; pTmp!=nullptr; pTmp=pTmp->pNextLayer)
            // {
            //     pTmp->szEnd = pTmp->szEnd + iRead;
            // }
            MSGBUFFER_WRITE(this->m_ReadCache, iRead);
            //this->m_ReadCache.Write(iRead);
            // this->m_ReadCache.m_szMsgEnd += iRead;
        }
        iProcNum = 0;
        iRet = this->ProcessMsg(iProcNum);
        if (iRet != GR_OK)
        {
            if (iRet == GR_FULL)
            {
                return GR_OK;
            }
            this->Close();
            return iRet;
        }
        if (iRead < iLeft) // 数据已经读完了
        {
            break;
        }

        // 如果是超大key，则一步扩容到位
        if (this->m_ReadMsg.m_Info.iMaxLen+256 > this->m_ReadCache.m_pData->m_sCapacity)
        {
            this->m_ReadCache.Expand(this->m_ReadMsg.m_Info.iMaxLen+256);
            this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
        }
        else
        {
            // 空间不够存储当前消息，但是剩余空间大于实际空间的一半，将消息拷贝到开始
            if (this->m_ReadCache.LeftCapcity() > this->m_ReadCache.m_pData->m_sCapacity>>1)
            {
                this->m_ReadCache.ResetBuffer();
                this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
            }
            else
            {
                this->m_ReadCache.Expand();
                this->m_ReadMsg.Reset(this->m_ReadCache.m_pData->m_uszData);
            } 
        }

        iMaxMsgRead -= iProcNum;
    } while(iMaxMsgRead > 0);
    return GR_OK;
}

int GR_AccessEvent::ProcessMsg(int &iMsgNum)
{
    GR_MsgIdenty *pIdenty = nullptr;
    int iParseRet = 0;
    int iRet = 0;
    GR_RedisEvent *pRedisEvent = nullptr;
    GR_MemPoolData  *pSendData = nullptr;
    for (;;)
    {
        if (RINGBUFFER_POOLFULL(this->m_pWaitRing))
        //if (this->m_pWaitRing->PoolFull()) // 不能接收更多消息了
        {
            // TODO 告警
            //ASSERT(false);
            GR_LOGE("wait for response pool is full, fd %d, addr %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return GR_FULL;
        }
        // 解析消息
        iParseRet = this->m_ReadMsg.ParseRsp();
        if (iParseRet != GR_OK)
        {
            ASSERT(false);
            GR_LOGE("parse client msg failed, fd %d, addr %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
        // 没有收到完整的消息
        if (this->m_ReadMsg.m_Info.nowState != GR_END)
        {
            break;
        }
        this->m_ReadCache.m_pData->m_sUsedSize = this->m_ReadMsg.m_Info.iLen;
        
        iMsgNum+=1;
        // GR_LOGD("got a message from client:%s", this->m_ReadMsg.szStart);
        // 将消息放入等待列表
        pIdenty = GR_MSGIDENTY_POOL()->Get();
        if (pIdenty == nullptr)
        {
            GR_LOGE("get msg identy failed, fd %d, addr %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            ASSERT(false);
            return GR_ERROR;
        }
        ASSERT(pIdenty->pData==nullptr && pIdenty->pReqData==nullptr && pIdenty->pAccessEvent==nullptr && pIdenty->pRedisEvent==nullptr);
        pIdenty->pAccessEvent = this;
        pIdenty->ulIndex = this->m_ulMsgIdenty++;
        if (!this->m_pWaitRing->AddData((char*)pIdenty, pIdenty->ulIndex))
        {
            // TODO 告警 满了不能接收数据了，不应该出现
            ASSERT(false);
            GR_LOGE("add identy failed %s:%d", this->m_szAddr, this->m_uiPort);
            return GR_ERROR;
        }
        pIdenty->iWaitRsp = 1;
        // 过滤不支持命令
        if (GR_NOT_SRPPORT_CMD(this->m_ReadMsg.m_Info))
        {
            this->GetReply(REDIS_RSP_UNSPPORT_CMD, pIdenty);
            this->StartNext();// 没有缓存请求，直接释放请求的内存
            continue;
        }
        // 如果没有key则不转发, 消息总长度不能超过配置长度
        GR_Config* pConfig = &GR_PROXY_INSTANCE()->m_Config;
        if (this->m_ReadMsg.m_Info.iKeyLen == 0 || this->m_ReadMsg.m_Info.iLen > pConfig->m_lLongestReq)
        {
            if (this->m_ReadMsg.m_Info.iLen > pConfig->m_lLongestReq)
            {
                this->GetReply(REDIS_REQ_TO_LARGE, pIdenty);
                this->StartNext();// 没有缓存请求，直接释放请求的内存
                continue;
            }
            else if (m_ReadMsg.m_Info.iCmdLen == 4 && str4icmp(m_ReadMsg.m_Info.szCmd, 'p', 'i', 'n', 'g')) // 如果是ping则回复pong
            {
                this->GetReply(REDIS_RSP_PONG, pIdenty);
                this->StartNext(); // 没有缓存请求，直接释放请求的内存
                continue;
            }
            else if (IS_RESET(m_ReadMsg.m_Info)) // reset命令
            {
            }
            else
            {
                this->GetReply(REDIS_RSP_UNSPPORT_CMD, pIdenty);
                this->StartNext();// 没有缓存请求，直接释放请求的内存
                continue;
            }
        }
        pSendData = this->m_ReadCache.m_pData;
        switch (GR_Proxy::m_pInstance->m_iRouteMode)
        {
            case PROXY_ROUTE_CLUSTER:
            {
                // 处理集群命令
                if (this->m_ReadMsg.m_Info.iCmdLen == 7 && IS_CLUSTER_CMD(m_ReadMsg.m_Info.szCmd))
                {
                    if (!IF_CLUSTER_SUPPORT_CMD(m_ReadMsg.m_Info))
                    {
                        this->GetReply(REDIS_RSP_UNSPPORT_CMD, pIdenty);
                        this->StartNext();// 没有缓存请求，直接释放请求的内存
                        continue;
                    }
                    if (IS_CLUSTER_SLOTS(m_ReadMsg.m_Info))
                    {
                        // 组织cluster slots返回值
                        GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
                        auto pData = GR_MsgProcess::Instance()->ClusterSlots((char*)pConfig->m_strIP.c_str(), pConfig->m_usPort);
                        if (pData == nullptr)
                        {
                            this->GetReply(REDIS_RSP_UNSPPORT_CMD, pIdenty);
                        }
                        else
                        {
                            this->GetReply(pData, pIdenty);
                        }
                        
                        this->StartNext();// 没有缓存请求，直接释放请求的内存
                        continue;
                    }
                }
                break;
            }
            case PROXY_ROUTE_TINY:
            {
                // 处理tiny命令
                // 如果传入clear命令，则将后端redis的数据都清除
                if (IS_RESET(m_ReadMsg.m_Info))
                {
                    // 将命令替换为 flushdb
                    pIdenty->iBroadcast = 1;
                    pSendData = GR_MsgProcess::Instance()->FlushdbCmd();
                }
                break;
            }
        }

        // 将消息发送给后端redis
        //iRet = GR_REDISMSG_INSTANCE()->TransferMsgToRedis(this, pIdenty);
        //GR_ROUTE(this->m_pRoute, pRedisEvent, pIdenty, this->m_ReadCache.m_pData, this->m_ReadMsg, iRet);
        if (pIdenty->iBroadcast == 0) // 不是广播消息
        {
            pRedisEvent = this->m_pRoute->Route(pIdenty, pSendData, this->m_ReadMsg, iRet);
            if (pRedisEvent == nullptr || pRedisEvent->m_iFD <= 0)
            {
                // TODO 先放缓冲池中，等redis连接上之后发送给redis
                GR_LOGE("transfer message get redis failed");
                iRet = REDIS_RSP_DISCONNECT;
            }
            else
            {
                iRet = pRedisEvent->SendMsg(pSendData, pIdenty);
            }
        }
        else
        {
            iRet = this->m_pRoute->Broadcast(this, pIdenty, pSendData);
        }

        if (GR_OK != iRet)
        {
            // TODO 和后端redis连接有问题的处理
            // 1、如果是池子满了则返回繁忙的标记,并触发告警
            if (iRet == GR_FULL)
            {
                GR_LOGE("redis is busy %s:%d", this->m_szAddr, this->m_uiPort);
                //GR_Proxy::Instance()->m_AccessMgr.AddPendingEvent(this);
                this->m_pRoute->m_pAccessMgr->AddPendingEvent(this);
                return iRet;
            }
            // 2、如果是出错了，则返回错误
            this->GetReply(iRet, pIdenty);
            this->StartNext();// 没有缓存请求，直接释放请求的内存
            continue;
        }
        // 更新缓存
        // 开始解析下一条消息
        // 重新申请一块内存
        this->StartNext(false); // 根据pIdenty->pReqData判断是否缓存了请求，决定是否释放请求的内存
        continue;
    }
    return GR_OK;
}

int GR_AccessEvent::ProcPendingMsg(bool &bFinish)
{
    bFinish = false;
    if (this->m_cPendingFlag == 0)
    {
        bFinish = true;
        return GR_OK;
    }
    GR_MsgIdenty *pIdenty = (GR_MsgIdenty*)this->m_pWaitRing->GetBack();
    if (pIdenty == nullptr || pIdenty->pRedisEvent != nullptr)
    {
        GR_LOGE("ProcPendingMsg, should not happend %s:%d", this->m_szAddr, this->m_uiPort);
        return GR_ERROR;
    }
    int iRet = GR_RedisMgr::Instance()->TransferMsgToRedis(this, pIdenty);
    if (GR_OK != iRet)
    {
        if (iRet == GR_FULL)
        {
            return GR_OK;
        }
        return iRet;
    }
    int iNum = 0;
    // 将剩余消息处理完
    iRet = this->ProcessMsg(iNum);
    if (iRet != GR_OK)
    {
        return iRet;
    }
    bFinish = true;
    return GR_OK;
}

// cluster消息不能转发
int GR_AccessEvent::ClusterReqMsg()
{
    if (IS_CLUSTER_CMD(m_ReadMsg.m_Info.szCmd))
    {
        return REDIS_RSP_UNSPPORT_CMD;
    }
    return GR_OK;
}

int GR_AccessEvent::Error()
{
    try
    {
        GR_LOGD("client connection %s:%d got error, error:%d, errmsg:%s", this->m_szAddr, this->m_uiPort, errno, strerror(errno));
        this->Close();
    }
    catch(exception &e)
    {
        GR_LOGE("process err msg failed:%s", e.what());
        return GR_ERROR;
    }
    return GR_ERROR;
}

int GR_AccessEvent::SetAddr(int iFD, sockaddr &sa, socklen_t &salen)
{
    this->m_iFD = iFD;
    sockaddr_in *s = (sockaddr_in *)&sa;
    inet_ntop(AF_INET,(void*)&(s->sin_addr),this->m_szAddr,NET_IP_STR_LEN);
    this->m_uiPort = ntohs(s->sin_port);
    return GR_OK;
}

int GR_AccessEvent::ReInit()
{
    try
    {
        this->m_uiPort = 0;
        memset(this->m_szAddr, 0, NET_IP_STR_LEN);
        if (GR_OK != this->m_ReadCache.ReInit())
        {
            return GR_ERROR;
        }
        if (GR_OK != this->m_ReadMsg.ReInit(this->m_ReadCache.m_pData->m_uszData))
        {
            return GR_ERROR;
        }
        ASSERT(this->m_pWaitRing != nullptr);
        if (GR_OK != this->m_pWaitRing->ReInit())
        {
            return GR_ERROR;
        }
        this->m_ulMsgIdenty = 0;
        return GR_OK;
    }
    catch(exception &e)
    {
        GR_LOGE("reinit access event got exception:%s", e.what());
        return GR_ERROR;
    }
}

int GR_AccessEvent::Close()
{
    if (m_iFD <= 0)
    {
        return GR_OK;
    }
    try
    {
        GR_Event::Close();
        GR_LOGI("close client, fd is %d, addr is %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
        GR_MsgIdenty *pIdenty = (GR_MsgIdenty*)this->m_pWaitRing->PopFront();
        for(;pIdenty != nullptr;)
        {
            // 已经没有属主了
            pIdenty->Release(ACCESS_RELEASE);
            pIdenty = (GR_MsgIdenty*)this->m_pWaitRing->PopFront();
        }
        ASSERT(this->m_pMgr != nullptr);
        this->m_pMgr->ReleaseEvent(this);
    }
    catch(exception &e)
    {
        GR_LOGE("close client failed:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

GR_AccessMgr::GR_AccessMgr()
{
    this->m_iListenFD = 0;
    this->m_pgShareInfo = GR_ProxyShareInfo::Instance();
}

GR_AccessMgr::~GR_AccessMgr()
{
    try
    {
        if (this->m_pListenEvent != nullptr)
        {
            delete this->m_pListenEvent;
        }
        if (this->m_pPendingClients != nullptr)
        {
            delete []m_pPendingClients;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("destroy accessmgr got exception:%s", e.what());
    }
}

int GR_AccessMgr::Init(GR_Route *pRoute)
{
    try
    {
        this->m_pRoute = pRoute;
        // 启动监听端口
        GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
        // 如果是按服务类型来做负载均衡，则子进程不监听客户端连接
        // 父进程监听客户端连接，然后将客户端连接发送给子进程
        this->m_pPendingClients = new GR_AccessEvent*[pConfig->m_iWorkConnections];
        memset(this->m_pPendingClients, 0, sizeof(GR_AccessEvent*)*pConfig->m_iWorkConnections);
    }
    catch(exception &e)
    {
        GR_LOGE("init accessmgr got exception:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

// 进程退出了
int GR_AccessMgr::Close()
{
    if (this->m_pListenEvent != nullptr && this->m_pListenEvent->m_iFD>0)
    {
        close(this->m_pListenEvent->m_iFD);
        this->m_pListenEvent->Reset();
    }
    return GR_OK;
}

GR_Event* GR_AccessMgr::CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen)
{
    GR_AccessEvent *pEvent = nullptr;
    try
    {
        pEvent = new GR_AccessEvent(iFD, sa, salen, this);
        if (pEvent->Init() != GR_OK)
        {
            delete pEvent;
            return nullptr;
        }
        pEvent->m_ulIdx = ++this->m_ulAccessIdx;
        pEvent->m_pRoute = this->m_pRoute;
        m_mapAccess[pEvent->m_ulIdx] = pEvent;
    }
    catch(exception &e)
    {
        GR_LOGE("create client got exception:%s", e.what());
        return nullptr;
    }

    return pEvent;
}

void GR_AccessMgr::ReleaseEvent(GR_AccessEvent *pEvent)
{
    try
    {
        pEvent->m_pMgr = nullptr;
        this->m_iClientNum-=1;
        ASSERT(this->m_iClientNum>=0);
        ASSERT(pEvent != nullptr);
        this->DelPendingEvent(pEvent);
        m_mapAccess.erase(pEvent->m_ulIdx);
        delete pEvent;
        GR_LOGI("now clients num is:%d", this->m_iClientNum);
    }
    catch(exception &e)
    {
        GR_LOGE("release event got exception:%s", e.what());
    }

    return;
}

int GR_AccessMgr::EnableAcceptEvent(bool &bSuccess)
{
    try
    {
        bSuccess = false;
        if (this->m_iAcceptDisabled > 0)
        {
            this->m_iAcceptDisabled--;
            return GR_OK;
        }
        // 抢锁
        this->m_ulLockValue = CURRENT_MS();
        if (m_pgShareInfo->Info->ShareLock.TryLock(this->m_ulLockValue))
        {
            GR_EPOLL_ADDREAD(this->m_pListenEvent);
            bSuccess = true;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("add accept event to epoll got exception:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

void GR_AccessMgr::DisableAcceptEvent()
{
    try
    {
        GR_ProxyShareInfo::Instance()->Info->ShareLock.UnLock(this->m_ulLockValue);
        GR_EPOLL_INSTANCE()->DelEventRW(this->m_pListenEvent);
    }
    catch(exception &e)
    {
        GR_LOGE("inactive accept event got exception:%s", e.what());
    }
}

GR_ListenEvent* GR_AccessMgr::CreateListenEvent()
{
    try
    {
        return new GR_ListenEvent(this);
    }
    catch(exception &e)
    {
        GR_LOGE("create listen event failed.");
        return nullptr;
    }
}

int GR_AccessMgr::ProcPendingEvents()
{
    if (this->m_iPendingFlag < 1)
    {
        return GR_OK;
    }
    try
    {
        int iNowPending = m_iPendingFlag;
        GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
        GR_AccessEvent *pEvent = nullptr;
        bool bFinishPend = false;
        int iProcRet = GR_OK;
        for (int i=0; i<pConfig->m_iWorkConnections; i++)
        {
            pEvent = m_pPendingClients[i];
            if (pEvent == nullptr)
            {
                continue;
            }
            ASSERT(pEvent->m_cPendingFlag == 1);
            bFinishPend = false;
            iProcRet = pEvent->ProcPendingMsg(bFinishPend);
            if (iProcRet == GR_OK && bFinishPend)
            {
                GR_LOGE("proc pending message failed %s:%d", pEvent->m_szAddr, pEvent->m_uiPort);
                this->DelPendingEvent(pEvent);
            }
            if (iProcRet != GR_OK)
            {
                pEvent->Close();
            }
            if (--iNowPending == 0)
            {
                break;
            }
        }
    }
    catch(exception &e)
    {
        GR_LOGE("proc pending event got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_AccessMgr::AddPendingEvent(GR_AccessEvent *pEvent)
{
    pEvent->m_cPendingFlag = 1; // 此时pIdenty的redisevent没有赋值
    GR_EPOLL_INSTANCE()->DelEventRead(pEvent); // 清除读事件触发器
    if (pEvent->m_iPendingIndex > 0)
    {
        GR_AccessEvent *pTmpEvent = this->m_pPendingClients[pEvent->m_iPendingIndex];
        if (pTmpEvent!=nullptr && pTmpEvent->m_uiPort == pEvent->m_uiPort) // 已经pending了
        {
            return GR_OK;
        }
    }

    GR_Config *pConfig = &GR_Proxy::Instance()->m_Config;
    for(int i=0; i<pConfig->m_iWorkConnections; i++)
    {
        GR_AccessEvent *pTmpEvent = this->m_pPendingClients[i];
        if (pTmpEvent!=nullptr) // 已经pending了
        {
            continue;
        }
        pEvent->m_iPendingIndex = i;
        this->m_pPendingClients[i] = pEvent;
        this->m_iPendingFlag += 1;
        return GR_OK;
    }

    ASSERT(false);
    return GR_OK;
}

int GR_AccessMgr::DelPendingEvent(GR_AccessEvent *pEvent)
{
    if (pEvent->m_cPendingFlag == 0)
    {
        return GR_OK;
    }
    pEvent->m_cPendingFlag = 0;
    if (pEvent->m_iFD > 0)
    {
        GR_EPOLL_ADDREAD(pEvent);
    }
    if (pEvent->m_iPendingIndex > 0)
    {
        GR_AccessEvent *pTmpEvent = this->m_pPendingClients[pEvent->m_iPendingIndex];
        if (pTmpEvent!=nullptr && pTmpEvent->m_uiPort == pEvent->m_uiPort) // 已经pending了
        {
            this->m_iPendingFlag -= 1;
            ASSERT(this->m_iPendingFlag > 0);
            this->m_pPendingClients[pEvent->m_iPendingIndex] = nullptr;
        }
    }
    pEvent->m_iPendingIndex = -1;
    return GR_OK;
}

int GR_AccessMgr::LoopCheck()
{
    try
    {
        GR_Event *pEvent = nullptr;
        auto ulNow = CURRENT_MS();
        for(auto itr=m_mapAccess.begin(); itr!=m_mapAccess.end();)
        {
            pEvent = itr->second;
            ++itr;// pEvent->Close会删除map，此处为删除map常规操作
            if (ulNow - pEvent->m_ulActiveMS > GR_EVENT_UN_ACTIVE_MS)
            {
                GR_LOGI("close unactive client %s:%d", pEvent->m_szAddr, pEvent->m_uiPort);
                pEvent->Close();
            }
        }
    }
    catch(exception &e)
    {
        GR_LOGE("accessmgr loop check got exception:%s", e.what());
        return GR_ERROR;
    }
        
    return GR_OK;
}


////////////////////////////////////////////////////////////////////

