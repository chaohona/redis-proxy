#include "gr_adminevent.h"
#include "proxy.h"
#include "utils.h"
#include "gr_proxy_global.h"
#include "gr_clusterroute.h"

GR_WorkChannelEvent::GR_WorkChannelEvent(int iFD)
{
    this->m_iFD = iFD;
}

GR_WorkChannelEvent::~GR_WorkChannelEvent()
{
}

int GR_WorkChannelEvent::Write()
{
    GR_GInfo* pInfo = GR_ProxyShareInfo::Instance()->Info;
    int iIdx = GR_Proxy::Instance()->m_iChildIdx;
    GR_WorkProcessInfo *pWorkInfo = &pInfo->Works[iIdx];
    switch (pWorkInfo->iStatus)
    {
    case PROXY_STATUS_EXPAND:
    {
        GR_ChannelMsg channleMsg;
        channleMsg.iCommand = GR_CHANNEL_MSG_EXPANDS;
        if (GR_OK == GR_Channel::Write(this->m_iFD, &channleMsg))
        {
           GR_EPOLL_INSTANCE()->DelEventWrite(this);
        }
        break;
    }
    case PROXY_STATUS_RUN:
    {
        GR_ChannelMsg channleMsg;
        channleMsg.iCommand = GR_CHANNEL_MSG_EXPANDE;
        if (GR_OK == GR_Channel::Write(this->m_iFD, &channleMsg))
        {
            GR_EPOLL_INSTANCE()->DelEventWrite(this);
        }
        break;
    }
    default:
    {
        GR_EPOLL_INSTANCE()->DelEventWrite(this);
        break;
    }
    }
    return GR_OK;
}

int GR_WorkChannelEvent::Read()
{
    int iRet;
    // 获取消息，然后做出相应的处理
    GR_ChannelMsg channleMsg;
    iRet = GR_Channel::Read(this->m_iFD, &channleMsg);
    if (iRet != GR_OK)
    {
        if (iRet != GR_EAGAIN)
        {
            // TODO 结束子进程
            return iRet;
        }
        GR_LOGE("read from channel failed:%d, %s", errno, strerror(errno));
        return iRet;
    }
    GR_LOGI("get admin cmd from master process:%d", channleMsg.iCommand);
    if (GR_OK != this->ProcAdminMsg(channleMsg))
    {
        GR_LOGE("process admin msg failed:%d,%d", this->m_iFD, channleMsg.iCommand);
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_WorkChannelEvent::ProcAdminMsg(GR_ChannelMsg &channleMsg)
{
    /*GR_GInfo* pInfo = GR_ProxyShareInfo::Instance()->Info;
    int iIdx = GR_Proxy::Instance()->m_iChildIdx;
    GR_WorkProcessInfo *pWorkInfo = &pInfo->Works[iIdx];
    switch (channleMsg.iCommand)
    {
        case GR_CHANNEL_MSG_EXPANDS:
        {
            GR_ClusterRoute *pRoute = (GR_ClusterRoute*)GR_RedisMgr::Instance()->m_pRoute;
            if (GR_OK != pRoute->ExpandStart())
            {
                // TODO 告警
                GR_LOGE("expand start failed.");
                break;
            }
            // 将进程的状态变成扩容中,并向主进程发送完成消息
            pWorkInfo->iStatus = PROXY_STATUS_EXPAND;
            GR_EPOLL_INSTANCE()->AddEventWrite(this);
            break;
        }
        case GR_CHANNEL_MSG_EXPANDE:
        {
            // 将进程的状态变成扩容结束,并向主进程发送完成消息
            pWorkInfo->iStatus = PROXY_STATUS_RUN;
           GR_EPOLL_INSTANCE()->AddEventWrite(this);
            break;
        }
        default:
        {
            GR_LOGE("got invalid msg fro channel:%d", channleMsg.iCommand);
            return GR_OK;
        }
    }*/
    return GR_OK;
}

int GR_WorkChannelEvent::Error()
{
    return GR_OK;
}

int GR_WorkChannelEvent::Close()
{
    return GR_OK;
}

GR_MasterChannelEvent::GR_MasterChannelEvent()
{
}

GR_MasterChannelEvent::~GR_MasterChannelEvent()
{
}

int GR_MasterChannelEvent::Write()
{
    if (this->m_iFD <= 0)
    {
        return GR_ERROR;
    }
    if (this->m_iMsgNum == 0)
    {
        GR_Epoll::Instance()->DelEventWrite(this);
        return GR_OK;
    }
    ssize_t n;
    msghdr msg;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = this->m_vIovs;
    msg.msg_iovlen = this->m_iMsgNum;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    n = sendmsg(this->m_iFD, &msg, 0);
    this->m_iMsgNum = 0;
    if (n == -1) {
        if (errno == EAGAIN) {
            return GR_EAGAIN;
        }
        GR_LOGE("send cmd failed:%d, %s", errno, strerror(errno));
        this->Close();
        return GR_ERROR;
    }
    GR_Epoll::Instance()->DelEventWrite(this);
    return GR_OK;
}

int GR_MasterChannelEvent::Read()
{
    int iRet;
    // 获取消息，然后做出相应的处理
    GR_ChannelMsg channleMsg;
    iRet = GR_Channel::Read(this->m_iFD, &channleMsg);
    if (iRet != GR_OK)
    {
        if (iRet != GR_EAGAIN)
        {
            // TODO 结束子进程
            return iRet;
        }
        return iRet;
    }
    if (GR_OK != this->ProcAdminMsg(channleMsg))
    {
        GR_LOGE("process admin msg failed:%d,%d", this->m_iFD, channleMsg.iCommand);
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_MasterChannelEvent::ProcAdminMsg(GR_ChannelMsg &channleMsg)
{
    GR_GInfo* pInfo = GR_ProxyShareInfo::Instance()->Info;
    GR_WorkProcessInfo *pWorkInfo = &pInfo->Works[this->m_iChildIdx];
    switch (channleMsg.iCommand)
    {
        case GR_CHANNEL_MSG_EXPANDS:
        {
            // 检查是否所有进程都收到扩容命令，收到则向客户端返回ok
            GR_WorkProcessInfo *pTmpWorkInfo;
            for (int i=0; i<GR_Proxy::Instance()->m_iChildIdx; i++)
            {
                pTmpWorkInfo = &pInfo->Works[i];
                if (pTmpWorkInfo->iStatus != PROXY_STATUS_EXPAND) // 还有进程没有准备好
                {
                    return GR_OK;
                }
            }
            // 向客户端返回扩容结束流程完成
            GR_Proxy *pProxy = GR_Proxy::Instance();
            GR_AdminMgr *pMgr = &pProxy->m_AdminMgr;
            GR_MasterAdminEvent* pEvent;
            for(auto it=pMgr->eventMap.begin(); it!=pMgr->eventMap.end(); it++)
            {
                pEvent = (GR_MasterAdminEvent*)it->second;
                if (IS_EXPANDS_CMD(pEvent->m_cPreCmd))
                {
                    pEvent->ExpandStartOK();
                }
            }
            
            break;
        }
        case GR_CHANNEL_MSG_EXPANDE:
        {
            // 检查是否所有进程都收到扩容结束命令，收到则向客户端返回ok
            GR_WorkProcessInfo *pTmpWorkInfo;
            for (int i=0; i<GR_Proxy::Instance()->m_iChildIdx; i++)
            {
                pTmpWorkInfo = &pInfo->Works[i];
                if (pTmpWorkInfo->iStatus != PROXY_STATUS_RUN) // 还有进程没有准备好
                {
                    return GR_OK;
                }
            }
            // 向客户端返回扩容结束流程完成
            GR_Proxy *pProxy = GR_Proxy::Instance();
            GR_AdminMgr *pMgr = &pProxy->m_AdminMgr;
            GR_MasterAdminEvent* pEvent;
            for(auto it=pMgr->eventMap.begin(); it!=pMgr->eventMap.end(); it++)
            {
                pEvent = (GR_MasterAdminEvent*)it->second;
                if (IS_EXPANDE_CMD(pEvent->m_cPreCmd))
                {
                    pEvent->ExpandEndOK();
                }
            }
            
            break;
        }
        default:
        {
            GR_LOGE("got invalid msg fro channel:%d", channleMsg.iCommand);
            return GR_OK;
        }
    }
    return GR_OK;
}

int GR_MasterChannelEvent::Error()
{
    return GR_OK;
}

int GR_MasterChannelEvent::Close()
{
    GR_Event::Close();
    return GR_OK;
}

int GR_MasterChannelEvent::PrepareSendCmd(int iCmd)
{
    if (this->m_iMsgNum >= GR_MAX_CHANNEL_MSG_POOL)
    {
        return GR_ERROR;
    }

    this->m_vChannelMsgs[this->m_iMsgNum].iCommand = iCmd;
    this->m_vIovs[this->m_iMsgNum].iov_base = (char*)&this->m_vChannelMsgs[this->m_iMsgNum];
    this->m_vIovs[this->m_iMsgNum].iov_len = sizeof(GR_ChannelMsg);
    this->m_iMsgNum += 1;

    GR_Epoll::Instance()->AddEventWrite(this);

    return GR_OK;
}

GR_MasterAdminEvent::GR_MasterAdminEvent(int iFD, sockaddr &sa, socklen_t &salen, GR_AdminMgr* pMgr)
{
    this->m_iFD = iFD;
    sockaddr_in *s = (sockaddr_in *)&sa;
    inet_ntop(AF_INET, (void*)&(s->sin_addr), this->m_szAddr, NET_IP_STR_LEN);
    this->m_uiPort = ntohs(s->sin_port);
    this->m_pMgr = pMgr;

    this->Init();
}

GR_MasterAdminEvent::GR_MasterAdminEvent(int iFD)
{
    this->m_iFD = iFD;
}

GR_MasterAdminEvent::~GR_MasterAdminEvent()
{
}

int GR_MasterAdminEvent::Init()
{
    this->m_ReadCache.Init(1024);
    this->m_WriteCache.Init(1024);
    
    // 将缓存的起始地址赋值给msg
    this->m_ReadMsg.Init((char*)this->m_ReadCache.m_pData->m_uszData);
    return GR_OK;
}

int GR_MasterAdminEvent::Read()
{
    // 已经关闭了
    if (this->m_iFD <= 0)
    {
        return GR_ERROR;
    }

    int iMaxMsgRead = 1000; // 每次最多接收1000条消息则让出cpu
    int iRead;              // 本次读取的字节数
    int iLeft = 0;
    int iRet = 0;
    int iProcNum = 0;
    do {
        iLeft = m_ReadCache.LeftCapcityToEnd();
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
            this->Close();
            GR_LOGE("client close the connection");
            return GR_ERROR;
        }
        if (iRead > 0)
        {
            REDISMSG_EXPAND(this->m_ReadMsg, iRead);
            MSGBUFFER_WRITE(this->m_ReadCache, iRead);
        }
        iProcNum = 0;
        iRet = this->ProcessMsg(iProcNum);
        if (iRet != GR_OK)
        {
            if (iRet == GR_FULL)
            {
                return GR_OK;
            }
            return iRet;
        }
        if (iRead < iLeft) // 数据已经读完了
        {
            break;
        }
        // 空间不够存储当前消息，将消息拷贝到开始
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
        iMaxMsgRead -= iProcNum;
    } while(iMaxMsgRead > 0);

    return GR_OK;
}

int GR_MasterAdminEvent::ProcessMsg(int iNum)
{
    int iParseRet = 0;
    GR_MsgIdenty *pIdenty;
    char        *szCmd = nullptr;
    int         iCmdLen = 0;
    int         iTmpLen = 0;
    for (;;)
    {
        // 解析消息
        iParseRet = this->m_ReadMsg.ParseRsp();
        if (iParseRet != GR_OK)
        {
            GR_LOGE("parse redis msg failed:%d", iParseRet);
            this->Close();
            return GR_ERROR;
        }
        if (this->m_ReadMsg.m_Info.nowState != GR_END)
        {
            break;
        }
        szCmd = this->m_ReadMsg.m_Info.szCmd;
        iCmdLen = this->m_ReadMsg.m_Info.iCmdLen;
        // expands
        // expande
        iTmpLen = iCmdLen;
        if (iTmpLen > GR_MAX_ADMIN_CMD_LEN)
        {
            iTmpLen = GR_MAX_ADMIN_CMD_LEN;
        }
        memcpy(this->m_cPreCmd, szCmd, iTmpLen);
        if (iCmdLen == 7 && IS_EXPANDS_CMD(szCmd))
        {
            this->ExpandStart();
        }
        else if (iCmdLen == 7 && IS_EXPANDE_CMD(szCmd))
        {
            this->ExpandEnd();
        }
        
        this->StartNext();
        continue;
    }
    return GR_OK;
}

int GR_MasterAdminEvent::ExpandStart()
{
    // 开始更新状态
    GR_Proxy *pProxy = GR_Proxy::Instance();
    if (pProxy->m_iStatus == PROXY_STATUS_EXPAND)
    {
        this->ExpandStartOK();
        return GR_OK;
    }
    
    // 向所有子进程发送正在扩容请求,确认所有子进程收到结束扩容的通知之后返回ok
    this->SendMsgToChild(GR_CHANNEL_MSG_EXPANDS);
    return GR_OK;
}

int GR_MasterAdminEvent::ExpandEnd()
{
    GR_Proxy *pProxy = GR_Proxy::Instance();
    if (pProxy->m_iStatus != PROXY_STATUS_EXPAND)
    {
        this->ExpandEndOK();
        return GR_OK;
    }
    
    // 向所有子进程发送扩容结束,确认所有子进程收到结束扩容的通知之后返回ok
    this->SendMsgToChild(GR_CHANNEL_MSG_EXPANDE);
    return GR_OK;
}

int GR_MasterAdminEvent::SendMsgToChild(int iCmd)
{
    GR_ProxyShareInfo *pShareInfo = GR_ProxyShareInfo::Instance();
    GR_MasterChannelEvent *pEvent;
    for (int i=0; i<MAX_WORK_PROCESS_NUM; i++)
    {
        if (pShareInfo->Info->Works[i].pid > 0)
        {
            pEvent = (GR_MasterChannelEvent *)pShareInfo->Info->Works[i].pEvent;
            pEvent->PrepareSendCmd(iCmd);
        }
    }

    return GR_OK;
}

int GR_MasterAdminEvent::ClearPreCmd()
{
    this->m_cPreCmd[0] = '0';
    return GR_OK;
}

int GR_MasterAdminEvent::ExpandStartOK()
{
    // 将ok消息发送给客户端
    GR_MemPoolData* pData = GR_MsgProcess::Instance()->GetErrMsg(REDIS_RSP_OK);
    this->m_WriteCache.Write(pData);
    GR_Epoll::Instance()->AddEventWrite(this);
    this->ClearPreCmd();
    
    return GR_OK;
}

int GR_MasterAdminEvent::ExpandEndOK()
{
    // 将ok消息发送给客户端
    GR_MemPoolData* pData = GR_MsgProcess::Instance()->GetErrMsg(REDIS_RSP_OK);
    this->m_WriteCache.Write(pData);
    GR_Epoll::Instance()->AddEventWrite(this);
    this->ClearPreCmd();
    
    return GR_OK;
}

int GR_MasterAdminEvent::ExpandStartFailed(int iErr)
{
    // 将ok消息发送给客户端
    GR_MemPoolData* pData = GR_MsgProcess::Instance()->GetErrMsg(iErr);
    this->m_WriteCache.Write(pData);
    GR_Epoll::Instance()->AddEventWrite(this);
    this->ClearPreCmd();

    return GR_OK;
}

int GR_MasterAdminEvent::ExpandEndFailed(int iErr)
{
    // 将ok消息发送给客户端
    GR_MemPoolData* pData = GR_MsgProcess::Instance()->GetErrMsg(iErr);
    this->m_WriteCache.Write(pData);
    GR_Epoll::Instance()->AddEventWrite(this);
    this->ClearPreCmd();

    return GR_OK;
}

int GR_MasterAdminEvent::StartNext()
{
    this->m_ReadCache.Read(this->m_ReadMsg.m_Info.iLen);
    this->m_ReadMsg.StartNext();
    
    return GR_OK;
}

int GR_MasterAdminEvent::Write()
{
    // 将消息发送给redis
    int iMaxTry = 0;
    int iLen = 0;
    bool bHasRing = false;
    int iWriteLen = 0;
    for(iMaxTry=0;iMaxTry<2;iMaxTry++)
    {
        iLen = this->m_WriteCache.MsgLenToEnd(bHasRing);
        if (iLen <= 0)
        {
            ASSERT(iLen==0);
            if (this->m_iFD>0)
            {
                GR_Epoll::Instance()->DelEventWrite(this);
            }
            break;
        }
        iWriteLen = write(this->m_iFD, this->m_WriteCache.m_szMsgStart, iLen);
        if (iWriteLen < 0)
        {
            if (errno == EINTR)
            {
                GR_LOGD("send msg to redis is not ready - eintr, %s:%d", this->m_szAddr, this->m_uiPort);
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                GR_LOGD("send msg to redis is not ready - eagain,%s:%d", this->m_szAddr, this->m_uiPort);
                return GR_EAGAIN;
            }
            // TODO 和redis连接出问题
            GR_LOGE("connect with redis tt");
            this->Close();
            return GR_ERROR;
        }
        if (iWriteLen == 0)
        {
            GR_LOGW("write on sd %d return zero, %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return GR_OK;
        }
        this->m_WriteCache.Read(iWriteLen);
        if (this->m_WriteCache.Empty())
        {
            GR_Epoll::Instance()->DelEventWrite(this);
            break;
        }
    }
    
    return GR_OK;
}

int GR_MasterAdminEvent::Error()
{
    return GR_OK;
}

int GR_MasterAdminEvent::Close()
{
    try
    {
        GR_Event::Close();
        if (this->m_pMgr != nullptr)
        {
            this->m_pMgr->CloseEvent(this);
        }
    }
    catch(exception &e)
    {
       GR_LOGE("close event %d %s:%d got exception:%s", this->m_iFD, this->m_szAddr, this->m_uiPort, e.what());
    }

    return GR_OK;
}

GR_AdminMgr::GR_AdminMgr()
{
}

GR_AdminMgr::~GR_AdminMgr()
{
}

int GR_AdminMgr::Init()
{
    // 启动监听端口
    int iRet = GR_OK;
    GR_Config &pConfig = GR_Proxy::Instance()->m_Config;
    iRet = this->Listen(pConfig.m_strAdminIP.c_str(), pConfig.m_usAdminPort, 128);
    if (iRet != GR_OK)
    {
        GR_LOGE("listen failed:%s", pConfig.m_strAdminAddr.c_str());
        return iRet;
    }

    return GR_OK;
}

int GR_AdminMgr::SendExpandEResult(int iRet)
{
    return GR_OK;
}

int GR_AdminMgr::SendExpandSResult(int iRet)
{
    return GR_OK;
}

GR_Event* GR_AdminMgr::CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen)
{
    GR_MasterAdminEvent *pEvent = nullptr;
    try
    {
        pEvent = new GR_MasterAdminEvent(iFD, sa, salen, this);
        this->eventMap[pEvent->m_uiEventId] = pEvent;
    }
    catch(exception &e)
    {
        GR_LOGE("create client got exception:%s", e.what());
        return nullptr;
    }

    return pEvent;
}

int GR_AdminMgr::CloseEvent(GR_MasterAdminEvent *pEvent)
{
    ASSERT(pEvent!=nullptr);
    try
    {
        this->eventMap.erase(pEvent->m_uiEventId);
        delete pEvent;
    }
    catch(exception &e)
    {
        GR_LOGE("destroy admin event got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

GR_ListenEvent* GR_AdminMgr::CreateListenEvent()
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

