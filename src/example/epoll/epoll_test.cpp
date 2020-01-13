#include "define.h"
#include "event.h"
#include "listenevent.h"
#include "socket.h"


int main()
{
    GR_Epoll::Instance()->Init(MAX_EVENT_POOLS);
    int iListenFD = GR_Socket::CreateAndListen("0.0.0.0", 12345, 100);

    GR_ListenMgr listenMgr;
    GR_ListenEvent listenEvent(&listenMgr);
    listenEvent.m_iFD = iListenFD;
    GR_Epoll::Instance()->AddEventRead(&listenEvent);

    timeval tv, *tvp;
    for(;;)
    {
        GR_Epoll::Instance()->EventLoopProcess(tvp);
    }
    return 0;
}
