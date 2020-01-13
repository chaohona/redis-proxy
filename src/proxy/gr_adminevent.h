#ifndef _GR_ADMIN_EVENT_H__
#define _GR_ADMIN_EVENT_H__
#include "include.h"
#include "msgbuffer.h"
#include "gr_channel.h"
#include "redismsg.h"
#include "event.h"
#include "listenevent.h"

#define GR_MAX_CHANNEL_MSG_POOL 16
#define GR_MAX_ADMIN_CMD_LEN 256

class GR_AdminMgr;

// 子进程使用的channleevent
class GR_WorkChannelEvent: public GR_Event
{
public:
    GR_WorkChannelEvent(int iFD);
    virtual ~GR_WorkChannelEvent();

    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();

private:
    int ProcAdminMsg(GR_ChannelMsg &channleMsg);
};

// 父进程使用的channel
class GR_MasterChannelEvent: public GR_Event
{
public:
    GR_MasterChannelEvent();
    virtual ~GR_MasterChannelEvent();

    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();

    int PrepareSendCmd(int iCmd);
public:
    int m_iChildIdx = 0;

private:
    int ProcAdminMsg(GR_ChannelMsg &channleMsg);

    GR_ChannelMsg   m_vChannelMsgs[GR_MAX_CHANNEL_MSG_POOL];
    iovec           m_vIovs[GR_MAX_CHANNEL_MSG_POOL];
    int             m_iMsgNum = 0;
};

// 管理接口
class GR_MasterAdminEvent: public GR_Event
{
public:
    GR_MasterAdminEvent(int iFD, sockaddr &sa, socklen_t &salen, GR_AdminMgr* pMgr);
    GR_MasterAdminEvent(int iFD);
    virtual ~GR_MasterAdminEvent();

    int Init();
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();
    
    int ExpandStartOK();
    int ExpandEndOK();
    int ExpandStartFailed(int iErr);
    int ExpandEndFailed(int iErr);
private:
    int ProcessMsg(int iNum);
    int StartNext();
    int ExpandStart();
    int ExpandEnd();
    int ClearPreCmd();
    int SendMsgToChild(int iCmd);

public:
    char m_cPreCmd[GR_MAX_ADMIN_CMD_LEN];
private:
    GR_MsgBufferMgr         m_ReadCache;    // 客户端读消息缓存初始化大小为128K
    GR_RedisMsg             m_ReadMsg;      // 消息解析器
    GR_RingMsgBufferMgr     m_WriteCache;   // 客户端写消息缓存初始化大小为128K
    GR_AdminMgr             *m_pMgr = nullptr;
};

// 监听管理器
class GR_AdminMgr: public GR_ListenMgr
{
public:
    GR_AdminMgr();
    virtual ~GR_AdminMgr();

    int SendExpandEResult(int iRet);
    int SendExpandSResult(int iRet);
public:
    int Init();
    virtual GR_Event* CreateClientEvent(int iFD, sockaddr &sa, socklen_t salen);
    virtual GR_ListenEvent* CreateListenEvent();
    int CloseEvent(GR_MasterAdminEvent *pEvent);

public:
    unordered_map<uint32, GR_MasterAdminEvent*> eventMap;
};

#endif
