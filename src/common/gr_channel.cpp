#include "gr_channel.h"
#include "socket.h"
#include "event.h"
#include "include.h"

GR_Channel::GR_Channel()
{
    this->fds[0] = 0;
    this->fds[1] = 0;
}

GR_Channel::~GR_Channel()
{
}

int GR_Channel::Init()
{
    try
    {
        if (this->fds[0] > 0)
        {
            close(this->fds[0]);
            this->fds[0] = 0;
        }
        if (this->fds[1] > 0)
        {
            close(this->fds[1]);
            this->fds[1] = 0;
        }
        
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, this->fds) == -1)
        {
            GR_LOGE("socketpair failed, errno %d,  errmsg %s", errno, strerror(errno));
            return GR_ERROR;
        }
        if (GR_Socket::SetNonBlocking(this->fds[0]) == -1
            || GR_Socket::SetNonBlocking(this->fds[1]) == -1)
        {
            this->DeAlloc();
            return GR_ERROR;
        }
        if (GR_Socket::SetReuseAddr(this->fds[0]) < 0
            || GR_Socket::SetReuseAddr(this->fds[1]) < 0)
         {
            this->DeAlloc();
            return GR_ERROR;
         }
        return GR_OK;
    }
    catch(exception &e)
    {
        GR_LOGE("alloc channel got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Channel::Close()
{
    if (this->fds[0] > 0)
    {
        close(this->fds[0]);
    }
    if (this->fds[1] > 0)
    {
        close(this->fds[1]);
    }
    this->fds[0] = 0;
    this->fds[1] = 0;
    return GR_OK;
}

int GR_Channel::Read(int fd, GR_ChannelMsg *pMsg)
{
    ssize_t n;
    iovec iov[1];
    msghdr msg;

    iov[0].iov_base = (char *) pMsg;
    iov[0].iov_len = sizeof(*pMsg);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    n = recvmsg(fd, &msg, 0);
    if (n == -1) {
        if (errno == EAGAIN) {
            return GR_EAGAIN;
        }
        GR_LOGE("read channle failed:%d,%d,%s", fd, errno, strerror(errno));
        return GR_ERROR;
    }
    if (n == 0) {
        GR_LOGE("read channle failed:%d,%d,%s", fd, errno, strerror(errno));
        return GR_ERROR;
    }
    if (n < (int)sizeof(*pMsg)) {
        GR_LOGE("read channle failed:%d,%d,%s", fd, errno, strerror(errno));
        return GR_ERROR;
    }
    
    return GR_OK;
}

int GR_Channel::Write(int fd, GR_ChannelMsg *pMsg)
{
    ssize_t n;
    iovec iov[1];
    msghdr msg;

    iov[0].iov_base = (char *) pMsg;
    iov[0].iov_len  = sizeof(*pMsg);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    n = sendmsg(fd, &msg, 0);
    if (n == -1) {
        if (errno == EAGAIN) {
            return GR_EAGAIN;
        }
        GR_LOGE("write channle failed:%d,%d,%s", fd, errno, strerror(errno));
        return GR_ERROR;
    }

    return GR_OK;
}

int GR_Channel::DeAlloc()
{
    try
    {
        this->Close();
        free(this);
    }
    catch(exception &e)
    {
        GR_LOGE("dealloc channel got exception:%s", e.what());
    }

    return GR_OK;
}

