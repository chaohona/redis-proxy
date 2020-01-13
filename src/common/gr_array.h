#ifndef _GR_ARRAY_H__
#define _GR_ARRAY_H__

#include "include.h"

template<class T>
class GR_Array
{
public:
    ~GR_Array()
    {
        try
        {
            if (this->m_pTArray != nullptr)
            {
                delete []this->m_pTArray;
            }
        }
        catch(exception &e)
        {
        }
    }
    int Init(int iMax)
    {
        try
        {
            this->m_pTArray = new T[iMax+1];
            this->m_iMaxTotal = iMax;
        }
        catch(exception &e)
        {
            GR_LOGD("init array got exception:%s", e.what());
            return GR_ERROR;
        }
        return GR_OK;
    }
public:
    T *m_pTArray;
    int m_iMaxTotal;
    int m_iIndex;
};

#define GR_ARRAY_PUSH(array, data)\
if ((array).m_iIndex == (array).m_iMaxTotal) \
delete data;\
else\
(array).m_pTArray[(array).m_iIndex] = data;\
++((array).m_iIndex);

#define GR_ARRAY_POP(array)\
((array).m_pTArray[--((array).m_iIndex)])

//((array)->m_iMaxTotal>(array)->m_iIndex && (array)->m_iIndex>0)?((array)->m_pTArray[--((array)->m_iIndex)]):nullptr;

#define GR_ARRAY_EMPTY(array)\
((array).m_iIndex==0)

#define GR_ARRAY_FULL(array)\
((array).m_iIndex==(array).m_iMaxTotal)


#endif

