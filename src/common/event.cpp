#include "event.h"
#include "define.h"
#include "gr_proxy_global.h"

uint32 GR_Event::global_event_id = 0;

GR_Event::GR_Event(int iFD)
{
    this->m_szAddr[0] = 0;
    this->m_iFD = iFD;
    this->m_uiEventId = ++GR_Event::global_event_id;
}

GR_Event::GR_Event()
{
    this->m_szAddr[0] = 0;
    this->m_uiEventId = ++GR_Event::global_event_id;
}

GR_Event::~GR_Event()
{
    //this->Reset();
}

bool GR_Event::Reset()
{
    this->Close();
    this->m_iMask       = 0;
    this->m_uiEventId   = 0;
    this->m_iFD = 0;
    this->m_Status = GR_CONNECT_INIT;
    memset(this->m_szAddr, 0, NET_IP_STR_LEN);
    return true;
}

bool GR_Event::operator == (const GR_Event &event)
{
    return this->m_iFD == event.m_iFD && this->m_uiEventId == event.m_uiEventId;
}

int GR_Event::AddToTimer()
{
    //this->m_pTimerMeta = GR_Timer::Instance()->AddTimer(CloseUnActiveEvent, this, GR_EVENT_UN_ACTIVE_MS, true);
    return GR_OK;
}

int GR_Event::Write()
{
    return 0;
}

int GR_Event::Read()
{
    return 0;
}

int GR_Event::Error()
{
    return 0;
}

int GR_Event::Close()
{
    try
    {
        this->m_Status = GR_CONNECT_CLOSED;
        if (this->m_iFD > 0)
        {
            GR_EPOLL_INSTANCE()->DelEventRW(this);
            close(this->m_iFD);
            GR_LOGD("event close %d %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            this->m_iFD = 0;
        }

        if (this->m_pTimerMeta != nullptr)
        {
            this->m_pTimerMeta->Finish();
            this->m_pTimerMeta = nullptr;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("close event %d %s:%d got exception:%s", this->m_iFD, this->m_szAddr, this->m_uiPort, e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Event::DoSendv(iovec *iov, int nsend)
{
    if (nsend >512)
    {
        nsend = 512;
    }
    ssize_t n;
    for (int i=0; i<2; i++) {
        n = writev(this->m_iFD, iov, nsend);
        if (n > 0) {
            return n;
        }

        if (n == 0) {
            GR_LOGD("sendv on sd %d returned zero %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return 0;
        }

        if (errno == EINTR) {
            GR_LOGD("sendv on sd %d not ready - eintr %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            n = 0;
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            GR_LOGD("sendv on sd %d not ready - eagain %s:%d", this->m_iFD, this->m_szAddr, this->m_uiPort);
            return GR_EAGAIN;
        } else {
            GR_LOGE("sendv on sd %d failed, num %d, address %s:%d, errmsg %s", this->m_iFD, nsend, this->m_szAddr, this->m_uiPort, strerror(errno));
            return GR_ERROR;
        }
    }

    return GR_ERROR;
}

int GR_Event::GetReply(int iErr, GR_MsgIdenty *pIdenty)
{
    return GR_OK;
}

int GR_Event::GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    pIdenty->pData = pData;
    return this->DoReply(pData, pIdenty);
}

int GR_Event::DoReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty)
{
    return GR_OK;
}

int GR_Event::ConnectCheck()
{
    if (this->m_Status != GR_CONNECT_CONNECTING && this->m_Status != GR_CONNECT_CONNECTED)
    {
        return this->m_Status;
    }
    int optval, optlen = sizeof(int);
    getsockopt(this->m_iFD, SOL_SOCKET, SO_ERROR,(void*) &optval, (socklen_t*)&optlen);  
    if (optval == 0)
    {
        return GR_CONNECT_CONNECTED;
    }
    return GR_CONNECT_CLOSED;
}

bool GR_Event::ConnectOK()
{
    return this->m_Status == GR_CONNECT_CONNECTING || this->m_Status == GR_CONNECT_CONNECTED;
}

GR_Epoll *GR_Epoll::m_pInstance  = new GR_Epoll();

GR_Epoll *GR_Epoll::Instance()
{
    return m_pInstance;
}

GR_Epoll::GR_Epoll()
{
    this->m_iEpFd = 0;
}

GR_Epoll::~GR_Epoll()
{
    try
    {
        if (this->m_aEpollEvents != nullptr)
        {
            delete []this->m_aEpollEvents;
            this->m_aEpollEvents = nullptr;
        }
        if (this->m_aFiredEvents != nullptr)
        {
            delete []this->m_aFiredEvents;
            this->m_aFiredEvents = nullptr;
        }
        close(this->m_iEpFd);
    }
    catch(exception &e)
    {
        GR_LOGE("destroy epoll got exception:%s", e.what());
    }

}

bool GR_Epoll::Init(int iEventNum)
{
    try
    {
        this->m_iEventNum = iEventNum;
        if (this->m_iEpFd == 0)
        {
            this->m_iEpFd = epoll_create(this->m_iEventNum);
            if (this->m_iEpFd < 0)
            {
                cout << "create epoll base failed" << strerror(errno) << endl;
                return false;
            }
            this->m_aFiredEvents = new GR_Event*[this->m_iEventNum];
            this->m_aEpollEvents = new epoll_event[this->m_iEventNum];
            
            this->m_aNetEvents = new GR_Event*[MAX_EVENT_POOLS];
        }
        else
        {
            close(this->m_iEpFd);
            this->m_iEpFd = epoll_create(this->m_iEventNum);
            if (this->m_iEpFd < 0)
            {
                cout << "create epoll base failed" << strerror(errno) << endl;
                return false;
            }
        }
    }
    catch(exception &e)
    {
        cout << "init epoll got exception:" << e.what() << endl;
        return false;
    }

    return true;
}

int GR_Epoll::AddEventRead(GR_Event *pEvent)
{
    return this->AddEvent(pEvent, GR_EVNET_READABLE);
}

int GR_Epoll::AddEventWrite(GR_Event *pEvent)
{
    return this->AddEvent(pEvent, GR_EVENT_WRITABLE);
}

int GR_Epoll::DelEventRead(GR_Event *pEvent)
{
    return this->DelEvent(pEvent, GR_EVNET_READABLE);
}

int GR_Epoll::DelEventWrite(GR_Event *pEvent)
{
    return this->DelEvent(pEvent, GR_EVENT_WRITABLE);
}
int GR_Epoll::AddEventRW(GR_Event *pEvent)
{
    return this->AddEvent(pEvent, GR_EVNET_READABLE|GR_EVENT_WRITABLE);
}

int GR_Epoll::DelEventRW(GR_Event *pEvent)
{
    return this->DelEvent(pEvent, GR_EVNET_READABLE|GR_EVENT_WRITABLE);
}

// TODO 已经加入事件触发器了，就不要再加入了
int GR_Epoll::AddEvent(GR_Event *pEvent, int iMask)
{
    if (pEvent->m_iFD<=0)
    {
        return GR_OK;
    }

    epoll_event ee = {0};

    /*auto existEvent = this->GetExistEvent(pEvent->m_iFD);
    if (existEvent == nullptr)
    {
        existEvent = this->SetEvent(pEvent);
    }*/
    int op = pEvent->m_iMask == GR_EVNET_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    pEvent->m_iMask |= iMask;
    // 水平触发
    if (pEvent->m_iMask & GR_EVNET_READABLE) {
        ee.events |= (uint32_t)(EPOLLIN);
    }
    if (pEvent->m_iMask & GR_EVENT_WRITABLE) {
        ee.events |= (uint32_t)(EPOLLIN | EPOLLOUT);
    }
    //ee.data.fd = pEvent->m_iFD;
    ee.data.ptr = pEvent;
    return epoll_ctl(this->m_iEpFd, op, pEvent->m_iFD, &ee);
}

int GR_Epoll::DelEvent(GR_Event * pEvent, int iMask)
{
    if (pEvent->m_iFD<=0)
    {
        return GR_OK;
    }
    //auto existEvent = this->GetExistEvent(pEvent->m_iFD);
    struct epoll_event ee = {0}; /* avoid valgrind warning */
    pEvent->m_iMask = pEvent->m_iMask & (~iMask);

    ee.events = 0;
    if (pEvent->m_iMask & GR_EVNET_READABLE) ee.events |= EPOLLIN;
    if (pEvent->m_iMask & GR_EVENT_WRITABLE) ee.events |= EPOLLOUT;
    //ee.data.fd = pEvent->m_iFD;
    ee.data.ptr = pEvent;
    if (pEvent->m_iMask != GR_EVNET_NONE) {
        epoll_ctl(this->m_iEpFd,EPOLL_CTL_MOD,pEvent->m_iFD,&ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(this->m_iEpFd,EPOLL_CTL_DEL,pEvent->m_iFD,&ee);
    }
}

int GR_Epoll::DelEvent(int iFD)
{
    struct epoll_event ee = {0};
    epoll_ctl(this->m_iEpFd,EPOLL_CTL_DEL,iFD,&ee);
}

GR_Event* GR_Epoll::SetEvent(GR_Event *pEvent)
{
    if (pEvent->m_iFD < this->m_iEventNum)
    {
        this->m_aNetEvents[pEvent->m_iFD] = pEvent;
    } else {
        this->m_mapNetEvents[pEvent->m_iFD] = pEvent;
    }

    return pEvent;
}

GR_Event *GR_Epoll::GetExistEvent(int iFD)
{
    if (iFD < this->m_iEventNum)
    {
        return this->m_aNetEvents[iFD];
    }

    return m_mapNetEvents[iFD];
}

int GR_Epoll::EventLoopProcess(uint64 ulMS)
{
    int retval = 0;

    this->m_iNumEventsActive = 0;
    if (ulMS < 20)
    {
        ulMS = 20;
    }
    retval = epoll_wait(this->m_iEpFd,this->m_aEpollEvents,this->m_iEventNum,ulMS);
    if (retval > 0) {
        this->m_iNumEventsActive = retval;
    }
    else if (retval < 0)
    {
        if (errno == EINTR || errno == EAGAIN)
        {
            return GR_OK;
        }
        GR_LOGE("epoll_wait failed:%d,%s", errno, strerror(errno));
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Epoll::ProcEvent(GR_EVENT_TYPE iType)
{
    int j;
    int iRet = 0;
    int num = 0;
    GR_Event *pEvent = nullptr;
    epoll_event *e;
    for (j = 0; j < this->m_iNumEventsActive; j++) {
        e = this->m_aEpollEvents+j;
        pEvent = (GR_Event*)e->data.ptr;
        if (pEvent==nullptr || pEvent->m_eEventType != iType)
        {
            continue;
        }
        num += 1;
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLOUT) )
        {
            iRet = pEvent->Write();
            if (GR_OK != iRet && GR_EAGAIN != iRet)
            {
                e->data.ptr = nullptr;
                continue;
            }
        }
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLIN) )
        {
            iRet = pEvent->Read();
            if (GR_OK != iRet && GR_EAGAIN != iRet)
            {
                e->data.ptr = nullptr;
                continue;
            }
        }
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLERR) )
        {
            pEvent->Error();
            e->data.ptr = nullptr;
            continue;
        }
    }
    return num;
}

int GR_Epoll::ProcEventNotType(GR_EVENT_TYPE iType)
{
    int j;
    int iRet = 0;
    GR_Event *pEvent = nullptr;
    epoll_event *e;
    for (j = 0; j < this->m_iNumEventsActive; j++) {
        e = this->m_aEpollEvents+j;
        pEvent = (GR_Event*)e->data.ptr;
        if (pEvent==nullptr || pEvent->m_eEventType == iType)
        {
            continue;
        }
        
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLOUT) )
        {
            iRet = pEvent->Write();
            if (GR_OK != iRet && GR_EAGAIN != iRet)
            {
                e->data.ptr = nullptr;
                continue;
            }
        }
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLIN) )
        {
            iRet = pEvent->Read();
            if (GR_OK != iRet && GR_EAGAIN != iRet)
            {
                e->data.ptr = nullptr;
                continue;
            }
        }
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events & EPOLLERR) )
        {
            pEvent->Error();
            e->data.ptr = nullptr;
            continue;
        }
    }

    return GR_OK;
}

int GR_Epoll::ProcAllEvents()
{
    int j;
    int iRet = 0;
    GR_Event *pEvent = nullptr;
    epoll_event *e;
    for (j = 0; j < this->m_iNumEventsActive; j++) {
        e = this->m_aEpollEvents+j;
        pEvent = (GR_Event*)e->data.ptr;
        if (pEvent==nullptr)
        {
            continue;
        }
        
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLOUT) )
        {
            iRet = pEvent->Write();
            if (GR_OK != iRet && GR_EAGAIN != iRet)
            {
                e->data.ptr = nullptr;
                continue;
            }
        }
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events&EPOLLIN) )
        {
            iRet = pEvent->Read();
            if (GR_OK != iRet && GR_EAGAIN != iRet)
            {
                e->data.ptr = nullptr;
                continue;
            }
        }
        if (e->data.ptr!=nullptr && pEvent!=nullptr && (e->events & EPOLLERR) )
        {
            pEvent->Error();
            e->data.ptr = nullptr;
            continue;
        }
    }

    return GR_OK;
}

