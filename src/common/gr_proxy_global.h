#ifndef _GR_PROXY_GLOBAL_H__
#define _GR_PROXY_GLOBAL_H__

#include "include.h"
#include "gr_atomic.h"
#include "gr_channel.h"
#include "event.h"

class GR_WorkProcessInfo
{
public:
    int Init();
public:
    volatile pid_t      pid = 0;
    GR_Atomic           iStatus = 0;
    GR_Event            *pEvent = nullptr;        // 需要保存的其它信息
};

class GR_ShmLock
{
public:
    int TryLock(uint64 ulLockValue);
    void UnLock(uint64 ulLockValue);
    uint64  GetValue();
    void SetValue(uint64 ulValue);
private:
    GR_Atomic   m_iLock = 0;
};

struct GR_GInfo
{
    GR_ShmLock          ShareLock;                      // 跨进程锁
    GR_Atomic           ulCurrentMS = 0;                // 当前毫秒数
    GR_WorkProcessInfo  Works[MAX_WORK_PROCESS_NUM+1];  // 子进程信息
};

// 提高效率，不调用函数
#define CURRENT_MS()\
GR_ProxyShareInfo::pInstance->Info!=nullptr?GR_ProxyShareInfo::pInstance->Info->ulCurrentMS:GR_GetNowMS()

// 进程间共享的信息操作接口
class GR_ProxyShareInfo
{
public:
    ~GR_ProxyShareInfo();

    static GR_ProxyShareInfo *Instance();
    int Bind(void *szShareAddr);
    int Size();
    void UpdateTimes();
    int64  GetCurrentMS(bool bRealTime=false);

    int ActiveExpandStart();
    int ActiveExpandEnd();
public:
    GR_ProxyShareInfo();
    static GR_ProxyShareInfo *pInstance;

public:
    GR_GInfo *Info = nullptr;
};

#endif

