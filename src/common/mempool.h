#ifndef _MEM_POOL_H__
#define _MEM_POOL_H__
#include "define.h"
#include "gr_array.h"
#include <list>

using namespace std;

#define POOL_DATA_TOTAL_TYPE 12
#define POOL_DATA_INVALID_IDX POOL_DATA_TOTAL_TYPE
#define POOL_DATA_MIN_LEN   0x200 // 512B

#define GR_MEMDATA_RELEASE(POOL_DATA)           \
if (POOL_DATA->m_cStaticFlag != 1)              \
{                                               \
    ASSERT(POOL_DATA->m_uszData != nullptr && POOL_DATA->m_uszEnd != nullptr && POOL_DATA->m_sCapacity != 0);\
    GR_MEMPOOL_INSTANCE()->Release(POOL_DATA);  \
}


// 内存
struct GR_MemPoolData {
public:
    GR_MemPoolData(char cStaticFlag=0);
    bool Release();           // 回收内存到内存池
    int ReInit();
public:
    char            m_cStaticFlag = 0;  // 为1则不释放
    char           *m_uszData = nullptr;     // 内存起始地址
    char           *m_uszEnd = nullptr;      // 可使用的内存结束的地方
    uint16          m_uiIndex = 0;       // 在内存池中的下标,-1为大内存
    size_t          m_sCapacity = 0;    // 内存容量
    size_t          m_sUsedSize = 0;    // 有效内容的使用量
    GR_MemPoolData  *m_pNext = nullptr;//   下一个
};

// 由于要适用网络层数据包缓存与命令缓存两种规格的内存使用场景
// 内存规格较多的较多小内存给命令使用，大内存给网络层使用
#define GR_MEMPOOL_INSTANCE()\
GR_MemPool::m_pInstance

#define GR_MEMPOOL_GETDATA(len)\
GR_MemPool::m_pInstance->GetData((len))


#define MAX_FREE_META_DATA 0xFFFF
class GR_MemPool {
private:
    GR_MemPool();
public:
    static GR_MemPool *Instance();
    ~GR_MemPool();

public:
    GR_MemPoolData *GetData(char *szData, int iLen);        // 申请内存，并把szData的数据拷贝到内存中
    GR_MemPoolData *GetData(size_t iSize);                // 申请sSize大小的内存
    bool Release(GR_MemPoolData *pData);                  // 回收申请的内存池

    static GR_MemPool *m_pInstance;
private:
    int                     m_aSize[POOL_DATA_TOTAL_TYPE];
    GR_Array<GR_MemPoolData*> m_FreePoolList[POOL_DATA_TOTAL_TYPE];
    //list<GR_MemPoolData*>   m_FreePoolList[POOL_DATA_TOTAL_TYPE];               // 内存池 0:512B 1:1K 2:2K 3:4K 4:8K 5:16K    6:32K 7:64K 8:128k 9:256k 10:512k 11:1024k
    int                     m_iMax;
    int                     m_iMetaSize;                     // 元数据的结构体大小

    GR_MemPoolData*        m_pFreeMetaData[MAX_FREE_META_DATA];// 缓存一万条数据(TODO和内存数据放在一起，不用单独管理)
    int                     m_iFreeMetaNum;
};

#endif
