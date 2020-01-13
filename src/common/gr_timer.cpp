#include "gr_timer.h"
#include "gr_proxy_global.h"

void GR_TimerMeta::Finish()
{
    this->bReuse = false;
    this->bInUse = false;
}


void GR_TimerMeta::Use(GR_TimerCB pCBFunc, void *pCB, uint64 ulMS, bool bReuse)
{
    uint64 ulNow = CURRENT_MS();
    this->pCBFunc = pCBFunc;
    this->pCBData = pCB;
    this->ulMS = ulMS;
    this->ulNextMS = ulNow + ulMS;
    this->bReuse = bReuse;
    this->bInUse = true;
}

GR_Timer *GR_Timer::m_pInstance = new GR_Timer();

GR_Timer *GR_Timer::Instance()
{
    return m_pInstance;
}

GR_Timer::GR_Timer()
{
    m_vMetaPool = new GR_TimerMeta[GR_TIMER_POOL_SIZE];
    this->m_pFreeMeta = m_vMetaPool;
    m_vMetaPool[0].pNextMeta = m_vMetaPool + 1;
    m_vMetaPool[0].ulIndex = 0;
    m_vMetaPool[0].bPoolMeta = true;
    for (int i=1; i<GR_TIMER_POOL_SIZE; i++)
    {
        m_vMetaPool[i].bPoolMeta = true;
        m_vMetaPool[i].ulIndex = i;
        m_vMetaPool[i].pPreMeta = &m_vMetaPool[i-1];
        if (i!=GR_TIMER_POOL_SIZE -1)
        {
            m_vMetaPool[i].pNextMeta = &m_vMetaPool[i+1];            
        }
    }
}

GR_Timer::~GR_Timer()
{
    try
    {
        GR_TimerMeta *pMeta;
        for(pMeta=this->m_pFreeMeta; pMeta!=nullptr;)
        {
            if (pMeta->bPoolMeta)
            {
                pMeta = pMeta->pNextMeta;
                continue;
            }
            pMeta = pMeta->pNextMeta;
            delete pMeta->pPreMeta;
        }

        delete []m_vMetaPool;
    }
    catch(exception &e)
    {
        GR_LOGE("destroy timer failed, %s", e.what());
    }
}

GR_TimerMeta* GR_Timer::AddTimer(GR_TimerCB pCBFunc, void *pCB, uint64 ulMS, bool bReuse)
{
    GR_LOGD("add timer:%ld", ulMS);
    ASSERT(pCBFunc!=nullptr);
    GR_TimerMeta *pMeta;
    try
    {
        pMeta = this->m_pFreeMeta;
        if (pMeta==nullptr)
        {
            pMeta = new GR_TimerMeta();
            pMeta->ulIndex = GR_TIMER_POOL_SIZE+1;
        }
        else
        {
            this->m_pFreeMeta = pMeta->pNextMeta;
            if (this->m_pFreeMeta != nullptr)
            {
                this->m_pFreeMeta->pPreMeta = nullptr;
            }
        }
        pMeta->Use(pCBFunc, pCB, ulMS, bReuse);
        pMeta->pNextMeta = this->m_pUsingMeta;
        pMeta->pPreMeta = nullptr;
        if (this->m_pUsingMeta != nullptr)
        {
            this->m_pUsingMeta->pPreMeta = pMeta;
        }
        this->m_pUsingMeta = pMeta;
    }
    catch(exception &e)
    {
        GR_LOGE("add timer got exception:%s", e.what());
        return 0;
    }
    ASSERT(pMeta!=nullptr);
    return pMeta;
}

int GR_Timer::DelTimer(GR_TimerMeta *pMeta)
{
    if (pMeta==nullptr)
    {
        return GR_OK;
    }

    pMeta->bInUse = false;
    // 把pMeta从链表中删除
    if (pMeta->pPreMeta != nullptr)
    {
        pMeta->pPreMeta->pNextMeta = pMeta->pNextMeta;
    }
    if (pMeta->pNextMeta != nullptr)
    {
        pMeta->pNextMeta->pPreMeta = pMeta->pPreMeta;
    }
    if (pMeta == this->m_pUsingMeta)
    {
        this->m_pUsingMeta = pMeta->pNextMeta;
    }
    // 回收Meta
    if (pMeta->bPoolMeta)
    {
        pMeta->pPreMeta = nullptr;
        if (this->m_pFreeMeta != nullptr)
        {
            pMeta->pNextMeta = this->m_pFreeMeta;
            this->m_pFreeMeta->pPreMeta = pMeta;
        }
        this->m_pFreeMeta = pMeta;
    }
    else
    {
        delete pMeta;
        pMeta = nullptr;
    }
    return GR_OK;
}

int GR_Timer::Loop(uint64 ulNow, uint64 &ulMix)
{
    int iTotal = 0;
    GR_TimerMeta *pTmpMeta;
    try
    {
        ulMix = ulNow+GR_EVENT_TT;
        for(GR_TimerMeta *pMeta = this->m_pUsingMeta; pMeta!=nullptr && iTotal<10;)
        {
            // 已经不使用了
            if (!pMeta->bInUse)
            {
                pTmpMeta = pMeta->pNextMeta;
                this->DelTimer(pMeta);
                pMeta = pTmpMeta;
                continue;
            }
            if (pMeta->ulNextMS <= ulNow) // 到时间执行了
            {
                ASSERT(pMeta->pCBFunc!=nullptr);
                pMeta->pCBFunc(pMeta, pMeta->pCBData);
                iTotal+=1;
                pTmpMeta = pMeta->pNextMeta;
                if (pMeta->bReuse)
                {
                    pMeta->ulNextMS = ulNow + pMeta->ulMS;
                    if (pMeta->ulNextMS < ulMix)
                    {
                        ulMix = pMeta->ulNextMS;
                    }
                }
                else // 回收meta
                {
                   this->DelTimer(pMeta); 
                }
                pMeta = pTmpMeta;
                continue;
            }
            else if (pMeta->ulNextMS < ulMix)
            {
                ulMix = pMeta->ulNextMS;
            }
            pMeta = pMeta->pNextMeta;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("process timer got exception:%s", e.what());
    }

    return iTotal;
}

int GR_Timer::AddTriggerMS(uint64 ulIndex, uint64 ulMS)
{
    ASSERT(ulIndex>0);
    if (ulIndex < GR_TIMER_POOL_SIZE)
    {
        this->m_vMetaPool[ulIndex].ulNextMS += ulMS;
        return GR_OK;
    }

    
    return GR_OK;
}

int GR_Timer::AddTriggerMS(GR_TimerMeta *pMeta, uint64 ulMS)
{
    ASSERT(pMeta!=nullptr);
    pMeta->ulNextMS+=ulMS;

    return GR_OK;
}

int GR_Timer::SetTriggerMS(GR_TimerMeta *pMeta, uint64 ulMS)
{
    ASSERT(pMeta!=nullptr);
    pMeta->ulNextMS=ulMS;
    return GR_OK;
}

