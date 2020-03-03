#include "listenevent.h"
#include "socket.h"

GR_ListenMgr::GR_ListenMgr()
{
}

GR_ListenMgr::~GR_ListenMgr()
{
}

int GR_ListenMgr::Accept(int iFD)
{
    sockaddr sa;
    socklen_t salen = sizeof(sa);

    int fd = -1;
    for(;;)
    {
        fd = accept(iFD, &sa, &salen);
        if (fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED)
            {
                return GR_EAGAIN;
            }
            else
            {
                GR_LOGE("accept err:%d, errmsg:%s", errno, strerror(errno));
                return GR_ERROR;
            }
        }
        break;
    }
    
    // 创建连接事件
    GR_Event *pEvent = CreateClientEvent(fd, sa, salen);
    if (pEvent == nullptr)
    {
        GR_LOGE("create client event failed");
        return GR_ERROR;
    }
    
    if (GR_OK != GR_Socket::SetNonBlocking(fd))
    {
        GR_LOGE("set nonblock failed:%d", fd);
        delete pEvent;
        return GR_ERROR;
    }
    if (GR_OK != GR_Socket::SetTcpNoDelay(fd))
    {
        GR_LOGE("set tcpnodelay failed:%d", fd);
        delete pEvent;
        return GR_ERROR;
    }
    if (GR_OK != GR_Socket::SetReuseAddr(fd))
    {
        GR_LOGE("set reuseraddr failed:%d", fd);
        delete pEvent;
        return GR_ERROR;
    }

    pEvent->m_ulActiveMS = GR_GetNowMS();
    pEvent->AddToTimer();
    // 将客户端加入触发器
    GR_EPOLL_ADDREAD(pEvent);
    this->m_iClientNum+=1;
    GR_LOGI("client accept success, fd %d, addr %s:%d, now client num %d", fd, pEvent->m_szAddr, pEvent->m_uiPort, this->m_iClientNum);
    
    return GR_OK;
}

int GR_ListenMgr::Listen(string strIP, uint16 usPort, int iTcpBack, bool bSystemBa)
{
    this->m_iListenFD = GR_Socket::CreateAndListen(strIP.c_str(), usPort, iTcpBack, bSystemBa);
    if (this->m_iListenFD <= 0)
    {
        GR_LOGE("create listen socket failed, port:%d, errno:%d, errmsg:%s", usPort, errno, strerror(errno));
        return GR_ERROR;
    }

    // 将监听事件加入事件管理列表

    this->m_pListenEvent = CreateListenEvent();
    if (this->m_pListenEvent == nullptr)
    {
        GR_LOGE("create listen event failed");
        return GR_ERROR;
    }
    this->m_pListenEvent->m_iFD = this->m_iListenFD;
    GR_LOGI("workprocess listen success %s:%d", strIP.c_str(), usPort);

    // TODO 改成所有redis都连接上了之后再加入事件处理器
    GR_EPOLL_ADDREAD(this->m_pListenEvent);
    return GR_OK;
}


GR_Event* GR_ListenMgr::CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen)
{
    GR_ClientEvent *pEvent = new GR_ClientEvent(iFD, sa, salen);
    if (pEvent == nullptr)
    {
        GR_LOGE("create client event failed");
        return nullptr;
    }

    return pEvent;
}

GR_ListenEvent* GR_ListenMgr::CreateListenEvent()
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

GR_ListenEvent::GR_ListenEvent()
{
    this->m_eEventType = GR_LISTEN_EVENT;
}

GR_ListenEvent::GR_ListenEvent(GR_ListenMgr *pListenMgr)
{
    this->m_pListenMgr = pListenMgr;
    this->m_eEventType = GR_LISTEN_EVENT;
}

GR_ListenEvent::~GR_ListenEvent()
{
}

int GR_ListenEvent::Write()
{
    return 0;
}

// 接收连接
int GR_ListenEvent::Read()
{
    int iStatus;
    int iMaxTry = 5; // 每次最多同时接收5个连接
    while(iMaxTry-- > 0)
    {
        iStatus = this->m_pListenMgr->Accept(this->m_iFD);
        if (iStatus != GR_OK)
        {
            return iStatus;
        }
    };

    return GR_OK;
}

int GR_ListenEvent::Error()
{
    return 0;
}

int GR_ListenEvent::Close()
{
    if (this->m_iFD > 0)
    {
        close(m_iFD);
        this->m_iFD = -1;
    }
    return 0;
}

