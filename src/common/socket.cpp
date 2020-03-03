#include "socket.h"

GR_Socket::GR_Socket()
{
}

GR_Socket::GR_Socket(int iFD)
{
    this->m_iFD = iFD;
}

GR_Socket::~GR_Socket()
{
}

void GR_Socket::SetFD(int iFD)
{
    this->m_iFD = iFD;
}

int GR_Socket::CreateAndListen(const char *szIp, uint16 usPort, int iBackLog, bool bSysBA)
{
    int iFD = socket(AF_INET, SOCK_STREAM, 0);
    if (iFD <= 0)
    {
        GR_LOGE("create socket failed:%d, errmsg:%s", errno, strerror(errno));
        return iFD;
    }
    if (GR_Socket::SetReuseAddr(iFD, bSysBA) != 0)
    {
        GR_LOGE("set resuse addr failed:%d,errmsg:%s", errno, strerror(errno));
        return GR_ERROR;
    }
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(szIp);
    server_addr.sin_port = htons(usPort);

    int iRet = bind(iFD, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(iRet == -1) {
        GR_LOGE("bind failed:%d, errmsg:%s", errno, strerror(errno));
        return iRet;
    }
    if (iBackLog < 1)
    {
        iBackLog = 128;
    }
    iRet = listen(iFD, iBackLog);
    if(iRet == -1) {
        GR_LOGE("listen failed:%d, errmsg:%s", errno, strerror(errno));
        return iRet;
    }
    if (GR_Socket::SetNonBlocking(iFD) != 0)
    {
        GR_LOGE("set socket nonblock failed:%d,errmsg:%s", errno, strerror(errno));
        return GR_ERROR;
    }
    GR_LOGD("create listen socket success, port:%d, fd:%d", usPort, iFD);
    return iFD;
}

int GR_Socket::SetBlocking()
{
    int flags;

    flags = fcntl(this->m_iFD, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(this->m_iFD, F_SETFL, flags & ~O_NONBLOCK);
}

int GR_Socket::SetBlocking(int iFD)
{
    int flags;

    flags = fcntl(iFD, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(iFD, F_SETFL, flags & ~O_NONBLOCK);
}


int GR_Socket::SetNonBlocking()
{
    int flags;

    flags = fcntl(this->m_iFD, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(this->m_iFD, F_SETFL, flags | O_NONBLOCK);
}

int GR_Socket::SetNonBlocking(int iFD)
{
    int flags;

    flags = fcntl(iFD, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(iFD, F_SETFL, flags | O_NONBLOCK);
}


int GR_Socket::SetReuseAddr()
{
    int reuse;
    socklen_t len;

    reuse = 1;
    len = sizeof(reuse);

    int iRet = 0;
    iRet = setsockopt(this->m_iFD, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
    if (iRet < 0)
    {
        return iRet;
    }
#ifdef SO_REUSEPORT
    iRet = setsockopt(this->m_iFD, SOL_SOCKET, SO_REUSEPORT, &reuse, len);
    if (iRet < 0)
    {
        return iRet;
    }
#endif

    return 0;

}

int GR_Socket::SetReuseAddr(int iFD, bool bSysBA)
{
    int reuse;
    socklen_t len;

    reuse = 1;
    len = sizeof(reuse);

    int iRet = 0;
    iRet = setsockopt(iFD, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
    if (iRet < 0)
    {
        return iRet;
    }
    if (bSysBA)
    {
#ifdef SO_REUSEPORT
        iRet = setsockopt(iFD, SOL_SOCKET, SO_REUSEPORT, &reuse, len);
        if (iRet < 0)
        {
            return iRet;
        }
#endif  
    }

    return 0;
}

int GR_Socket::SetReuserPort(int iFD)
{
    int reuse = 1;
    socklen_t len = sizeof(reuse);

    int iRet = setsockopt(iFD, SOL_SOCKET, SO_REUSEPORT, &reuse, len);
    if (iRet < 0)
    {
        return iRet;
    }
    return 0;
}

int GR_Socket::SetTcpNoDelay()
{
    int nodelay;
    socklen_t len;

    nodelay = 1;
    len = sizeof(nodelay);

    return setsockopt(this->m_iFD, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

int GR_Socket::SetTcpNoDelay(int iFD)
{
    int nodelay;
    socklen_t len;

    nodelay = 1;
    len = sizeof(nodelay);

    return setsockopt(iFD, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}


int GR_Socket::SetLinger(int iTimeOut)
{
    linger linger;
    socklen_t len;

    linger.l_onoff = 1;
    linger.l_linger = iTimeOut;

    len = sizeof(linger);

    return setsockopt(this->m_iFD, SOL_SOCKET, SO_LINGER, &linger, len);
}

int GR_Socket::SetLinger(int iFD, int iTimeOut)
{
    linger linger;
    socklen_t len;

    linger.l_onoff = 1;
    linger.l_linger = iTimeOut;

    len = sizeof(linger);

    return setsockopt(iFD, SOL_SOCKET, SO_LINGER, &linger, len);
}


int GR_Socket::SetTcpKeepAlive()
{
    int val = 1;
    return setsockopt(this->m_iFD, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}

int GR_Socket::SetTcpKeepAlive(int iFD)
{
    int val = 1;
    return setsockopt(iFD, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}


int GR_Socket::SetSndBuff(int iSize)
{
    socklen_t len;

    len = sizeof(iSize);

    return setsockopt(this->m_iFD, SOL_SOCKET, SO_SNDBUF, &iSize, len);
}

int GR_Socket::SetSndBuff(int iFD, int iSize)
{
    socklen_t len;

    len = sizeof(iSize);

    return setsockopt(iFD, SOL_SOCKET, SO_SNDBUF, &iSize, len);
}


int GR_Socket::SetRevBuf(int iSize)
{
    socklen_t len;

    len = sizeof(iSize);

    return setsockopt(this->m_iFD, SOL_SOCKET, SO_RCVBUF, &iSize, len);
}

int GR_Socket::SetRevBuf(int iFD, int iSize)
{
    socklen_t len;
    
    len = sizeof(iSize);

    return setsockopt(iFD, SOL_SOCKET, SO_RCVBUF, &iSize, len);
}

int GR_Socket::SetSndTimeO(int iFD, int iMS)
{
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = iMS * 1000;
    setsockopt(iFD, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof( timeval ) );

    return 0;
}

int GR_Socket::SetRcvTimeO(int iFD, int iMS)
{
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = iMS * 1000;
    setsockopt(iFD, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof( timeval ) );

    return 0;
}

int GR_Socket::SetSynCnt(int iFD, int iCnt)
{
    setsockopt(iFD, IPPROTO_TCP, TCP_SYNCNT, &iCnt, sizeof(iCnt));
    return 0;
}

bool GR_Socket::ConnectedCheck(int iFD)
{
    struct tcp_info info;
    int len = sizeof(info);
    getsockopt(iFD, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *) & len);
    if ((info.tcpi_state == TCP_ESTABLISHED)) {
        return true;
    }

    return false;
}

int GR_Socket::GetSoError()
{
    int status, err;
    socklen_t len;

    err = 0;
    len = sizeof(err);

    status = getsockopt(this->m_iFD, SOL_SOCKET, SO_ERROR, &err, &len);
    if (status == 0) {
        errno = err;
    }

    return status;
}

int GR_Socket::GetSoError(int iFD)
{
    int status, err;
    socklen_t len;

    err = 0;
    len = sizeof(err);

    status = getsockopt(iFD, SOL_SOCKET, SO_ERROR, &err, &len);
    if (status == 0) {
        errno = err;
    }

    return status;
}


int GR_Socket::GetSndBuff()
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(this->m_iFD, SOL_SOCKET, SO_SNDBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

int GR_Socket::GetSndBuff(int iFD)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(iFD, SOL_SOCKET, SO_SNDBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}


int GR_Socket::GetRcvBuff()
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(this->m_iFD, SOL_SOCKET, SO_RCVBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

int GR_Socket::GetRcvBuff(int iFD)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(iFD, SOL_SOCKET, SO_RCVBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

int GR_Socket::GetError(int iFD)
{
    int err = -1;
    socklen_t len = sizeof(int);
    if (getsockopt(iFD,  SOL_SOCKET, SO_ERROR ,&err, &len) < 0 )
    {
        return -1;
    }

    return err;
}

