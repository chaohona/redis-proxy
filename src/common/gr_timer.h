#ifndef _GR_TIMER_H__
#define _GR_TIMER_H__

#include "include.h"
#include <list>

struct GR_TimerMeta;
#define GR_TIMER_CB_MAX 64
typedef int (*GR_TimerCB)(GR_TimerMeta *pMeta, void *pCB);   // 定时器回掉函数


class GR_Timer;
struct GR_TimerMeta
{
public:
friend class GR_Timer;

    void        Finish(); // 
public:
    void        Use(GR_TimerCB pCBFunc, void *pCB, uint64 ulMS, bool bReuse);
    bool        bReuse = false;     // 是否需要重复使用
    bool        bPoolMeta = false;
    bool        bInUse = false;
    uint64      ulMS = 0;       // 间隔毫秒数
    uint64      ulNextMS = 0;   // 下一次触发事件
    void        *pCBData = nullptr;
    GR_TimerCB  pCBFunc = nullptr;
    int64       ulIndex = -1;

    GR_TimerMeta *pPreMeta = nullptr;
    GR_TimerMeta *pNextMeta = nullptr;
};

#define GR_TIMER_POOL_SIZE 0x3FFF
// TODO改成时间轮方式
class GR_Timer
{
public:
    ~GR_Timer();
    static GR_Timer* Instance();

    // 返回定时事件的标记，如果需要删除，修改定时器通过返回的标记操作
    GR_TimerMeta* AddTimer(GR_TimerCB pCBFunc, void *pCB, uint64 ulMS, bool bReuse=false);
    int Loop(uint64 ulNow, uint64 &ulMix);
    int AddTriggerMS(uint64 ulIndex, uint64 ulMS);
    int AddTriggerMS(GR_TimerMeta *pMeta, uint64 ulMS);
    int SetTriggerMS(GR_TimerMeta *pMeta, uint64 ulMS);
private:
    int DelTimer(GR_TimerMeta *pMeta);
    
    static GR_Timer *m_pInstance;
    GR_Timer();

    GR_TimerMeta*   m_vMetaPool = nullptr;    // meta池子
    GR_TimerMeta*   m_pFreeMeta = nullptr;    // 缓存的meta数据
    GR_TimerMeta*  m_pUsingMeta = nullptr;    // 需要触发的定时器数据,长连接，需要查找的需求少，使用链表
};

#endif
