#include "gr_proxy_global.h"
#include "utils.h"
#include "event.h"

int GR_WorkProcessInfo::Init()
{
    return GR_OK;
}

int GR_ShmLock::TryLock(uint64 ulLockValue)
{
    return (this->m_iLock == 0 && GR_AtomicCmpSet(&this->m_iLock, 0, ulLockValue));
}

void GR_ShmLock::UnLock(uint64 ulLockValue)
{
    GR_AtomicCmpSet(&this->m_iLock, ulLockValue, 0);
}

uint64 GR_ShmLock::GetValue()
{
    return this->m_iLock;
}

void GR_ShmLock::SetValue(uint64 ulValue)
{
    this->m_iLock = ulValue;
}

GR_ProxyShareInfo *GR_ProxyShareInfo::pInstance = new GR_ProxyShareInfo();

GR_ProxyShareInfo::GR_ProxyShareInfo()
{
}

GR_ProxyShareInfo::~GR_ProxyShareInfo()
{
}

GR_ProxyShareInfo *GR_ProxyShareInfo::Instance()
{
    return pInstance;
}

int GR_ProxyShareInfo::Bind(void * pShareAddr)
{
    this->Info = new (pShareAddr)GR_GInfo();
    memset(&this->Info->Works, 0, sizeof(GR_WorkProcessInfo)*(MAX_WORK_PROCESS_NUM+1));
    return GR_OK;
}

int GR_ProxyShareInfo::Size()
{
    return sizeof(GR_GInfo);
}

void GR_ProxyShareInfo::UpdateTimes()
{
    this->Info->ulCurrentMS = GR_GetNowMS();
}
int64 GR_ProxyShareInfo::GetCurrentMS(bool bRealTime)
{
    //return GR_GetNowMS();
    // 还没有绑定共享内存
    if (this->Info == nullptr)
    {
        return GR_GetNowMS();
    }
    if (!bRealTime)
    {
        return this->Info->ulCurrentMS;
    }
    this->Info->ulCurrentMS = GR_GetNowMS();
    return this->Info->ulCurrentMS;
}


