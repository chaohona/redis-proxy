#ifndef _GR_CHANNEL_H__
#define _GR_CHANNEL_H__
#include "define.h"

enum GR_CHANNEL_MSG
{
    GR_CHANNEL_MSG_EXPANDS,
    GR_CHANNEL_MSG_EXPANDE
};

struct GR_ChannelMsg
{
    int iCommand;
};

class GR_Channel
{
public:
    GR_Channel();
    ~GR_Channel();

    int Init();
    int DeAlloc();

    int Close();
    static int Read(int fd, GR_ChannelMsg *pMsg);
    static int Write(int fd, GR_ChannelMsg *pMsg);
public:
    int fds[2]; // fds[0]父进程使用，fds[1]子进程使用
};

#endif
