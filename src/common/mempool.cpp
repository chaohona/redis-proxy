#include "mempool.h"
#include "utils.h"

GR_MemPoolData::GR_MemPoolData(char cStaticFlag)
{
    this->m_cStaticFlag = cStaticFlag;
}

bool GR_MemPoolData::Release()
{
    if (this->m_cStaticFlag == 1)
    {
        return true;
    }
    ASSERT(this->m_uszData != nullptr && this->m_uszEnd != nullptr && this->m_sCapacity != 0);
    GR_MemPool::Instance()->Release(this);
    return true;
}

int GR_MemPoolData::ReInit()
{
    ASSERT(this->m_cStaticFlag!=1);
    this->m_sUsedSize = 0;
    return GR_OK;
}

GR_MemPool::GR_MemPool()
{
    //this->m_pFreeMetaData = new GR_MemPoolData*[MAX_FREE_META_DATA + 1];
    for(int i=0; i<POOL_DATA_TOTAL_TYPE; i++)
    {
        auto pPool = m_FreePoolList+i;
        if (GR_OK != pPool->Init(2048/(i+1)))
        {
            throw("init mempool failed.");
        }
    }
    this->m_iFreeMetaNum = 0;
    this->m_iMetaSize = sizeof(GR_MemPoolData);
    // 预计算4个规格的池子可以申请内存的大小
    int iSize = POOL_DATA_MIN_LEN;
    for (int i=0; i<POOL_DATA_TOTAL_TYPE; i++)
    {
        this->m_aSize[i] = iSize;
        this->m_iMax = iSize;
        iSize = iSize << 1;
    }
}

// 单实例模式，析构函数暂时啥也不做
GR_MemPool::~GR_MemPool()
{
    
}

GR_MemPool* GR_MemPool::m_pInstance = new GR_MemPool();

GR_MemPool* GR_MemPool::Instance()
{
    return m_pInstance;
}

GR_MemPoolData *GR_MemPool::GetData(char *szData, int iLen)
{
    GR_MemPoolData *pData = this->GetData(iLen);
    if (pData == nullptr)
    {
        return nullptr;
    }
    memcpy(pData->m_uszData, szData, iLen);
    pData->m_sUsedSize = iLen;
    return pData;
}

GR_MemPoolData *GR_MemPool::GetData(size_t sSize)
{
    int iIndex = POOL_DATA_INVALID_IDX;
    int iNewSize = sSize;
    // 计算index
    for(int i=0; i<POOL_DATA_TOTAL_TYPE; i++)
    {
        if (sSize <= this->m_aSize[i])
        {
            iIndex = i;
            iNewSize = m_aSize[i];
            break;
        }
    }
    if (iIndex < POOL_DATA_TOTAL_TYPE && !GR_ARRAY_EMPTY(this->m_FreePoolList[iIndex])) // 从free链表中获取
    {
        GR_MemPoolData* ret = GR_ARRAY_POP(this->m_FreePoolList[iIndex]);
        ASSERT(ret!=nullptr && ret->m_uszData!=nullptr && ret->m_sCapacity==iNewSize);
        return ret;
    }

    GR_MemPoolData *pData = nullptr;
    if (this->m_iFreeMetaNum > 0)
    {
        --this->m_iFreeMetaNum;
        pData = this->m_pFreeMetaData[this->m_iFreeMetaNum];
    }
    else
    {
        pData = new GR_MemPoolData();
    }

    pData->m_uiIndex = iIndex;
    pData->m_uszData = new char[iNewSize];
    pData->m_uszEnd = pData->m_uszData + iNewSize;
    pData->m_sCapacity = iNewSize;
    ASSERT(pData!=nullptr && pData->m_uszData!=nullptr && pData->m_uiIndex<=POOL_DATA_TOTAL_TYPE);
    return pData;
}

bool GR_MemPool::Release(GR_MemPoolData * pData)
{
    ASSERT(pData != nullptr && pData->m_uiIndex<=POOL_DATA_TOTAL_TYPE && pData->m_uszData!=nullptr && 
        pData->m_cStaticFlag==0 );
    if (pData == nullptr)
    {
        return true;
    }
    
    pData->m_sUsedSize = 0;
    if (pData->m_uiIndex < POOL_DATA_TOTAL_TYPE &&
        !GR_ARRAY_FULL(this->m_FreePoolList[pData->m_uiIndex]))
    {
        GR_ARRAY_PUSH( this->m_FreePoolList[pData->m_uiIndex], pData);
        ASSERT(pData->m_uszData!=nullptr && pData->m_uszEnd!=nullptr && pData->m_sCapacity!=0 && pData->m_uiIndex<POOL_DATA_TOTAL_TYPE);
        return true;
    } 
    else if (pData->m_uszData != nullptr)
    {
        delete []pData->m_uszData;
        pData->m_uszData = nullptr;
        pData->m_uszEnd = nullptr;
        pData->m_uiIndex = POOL_DATA_INVALID_IDX;
        pData->m_sCapacity = 0;
    }

    if (this->m_iFreeMetaNum < MAX_FREE_META_DATA)
    {
        this->m_pFreeMetaData[this->m_iFreeMetaNum] = pData;
        ++this->m_iFreeMetaNum;
    }
    else 
    {
        delete pData;
    }
    return true;
}

