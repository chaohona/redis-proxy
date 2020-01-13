#ifndef _GR_LOAD_EVENT_H__
#define _GR_LOAD_EVENT_H__
#include "event.h"
#include "include.h"
#include "msgbuffer.h"
#include "accesslayer.h"

#define GR_LOAD_START   0
#define GR_LOAD_FINISH_READ 1
#define GR_LOAD_FINISH  2
#define GR_LOAD_ERROR   3
// rdb格式的同步与在线同步，通过开源的redis-port实现
class GR_LoadRdbEvent : public GR_AccessEvent
{
public:
    GR_LoadRdbEvent(int iFD);
    virtual ~GR_LoadRdbEvent();

    int Loading();
public:
    virtual int GetReply(int iErr, GR_MsgIdenty *pIdenty);
    virtual int GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
};

class GR_LoadAofEvent: public GR_AccessEvent
{
public:
    GR_LoadAofEvent();
    virtual ~GR_LoadAofEvent();

    int Loading(FILE *fp, string &strAof);
    int Init();
    
    virtual int Write();
    virtual int Read();
    virtual int Error();
    virtual int Close();

    int ClearPending();
public:
    virtual int GetReply(int iErr, GR_MsgIdenty *pIdenty);
    virtual int GetReply(GR_MemPoolData *pData, GR_MsgIdenty *pIdenty);
private:
    int ProcessMsg();
    int StartNext();
public:
    int         m_iProcFlag = GR_LOAD_START;
    int         m_iPending = 0;
private:
    FILE        *fp = nullptr;
    string      m_strAof = string("");
    int         m_iSelectNum = 0;
};

#endif
