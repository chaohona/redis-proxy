#ifndef _GR_RING_BUFFER_H__
#define _GR_RING_BUFFER_H__
#include "include.h"

#define RINGBUFFER_POOLFULL(pool)\
pool->m_iDataNum == pool->m_iPoolLen

#define GR_RB_POPFRONT(result_type, result, rb) \
{                                               \
    if (rb->m_iDataNum == 0)                    \
    {                                           \
        result = nullptr;                       \
    }                                           \
    else                                        \
    {                                           \
        rb->m_iDataNum -= 1;                    \
        char *pRet = *(rb->m_pStart);           \
        char **tmp = rb->m_pStart;              \
        rb->m_pStart += 1;                      \
        *tmp = nullptr;                         \
        if (rb->m_pStart >= rb->m_pWaitMsgEnd)  \
        {                                       \
            rb->m_pStart = rb->m_pWaitMsgPool;  \
        }                                       \
        result = (result_type)pRet;             \
    }                                           \
}

#define GR_RB_GETFRONT(result_type, result, rb) \
{                                               \
    if (rb->m_iDataNum == 0)                    \
    {                                           \
        result =  nullptr;                      \
    }                                           \
    result = (result_type)*(rb->m_pStart);      \
}

#define GR_RB_GETDATA(result_type, result, ulNextIndex, rb)\
result=(result_type)rb->m_pWaitMsgPool[ulNextIndex%(rb->m_iPoolLen)];


class RingBuffer
{
public:
    RingBuffer(int iPoolLen);
    RingBuffer();
    ~RingBuffer();
public:
    bool AddData(char *pData);// 将返回标记放入pool中
    bool AddData(char *pData, uint64 ulIndex);  
    bool PoolFull();                 // pool是否已经满了
    int  GetNum();  // 返回元素个数
    char *PopFront();
    char *PopFront(int iNum);
    char *GetFront();
    char *GetFront(char **pNow);
    char *GetBack();
    // bValid表示返回的是否是有效数据
    char *GetData(uint64 ulIndex, bool &bValid);
    char *GetPre(char **pData, bool &bValid);
    char *GetPre(uint64 ulIndex, bool &bValid);
    char *GetNext(char **pData, bool &bValid);
    char *GetNext(uint64 ulIndex, bool &bValid);
    char *GetData(uint64 ulIndex);
    char *GetPre(char **pData);
    char *GetPre(uint64 ulIndex);
    char *GetNext(char **pData);
    char *GetNext(uint64 ulIndex);

    int ReInit();
private:
    bool Init(int iPoolLen);
public:
    int         m_iPoolLen = 0;
    char        **m_pStart = nullptr;         // 池子的起点(固定的起点)
    char        **m_pEnd = nullptr;           // 池子的尾点(固定的结尾)
    char        **m_pWaitMsgPool = nullptr;   // 移动的有效数据的起点
    char        **m_pWaitMsgEnd = nullptr;    // 移动的有效数据的尾点
    int         m_iDataNum = 0;
};

#endif
