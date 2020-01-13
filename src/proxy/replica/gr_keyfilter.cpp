#include "gr_keyfilter.h"
#include "include.h"

#include <algorithm>
#include <cctype>

GR_FilterList::GR_FilterList()
{
    //iNum    = 0;
    //iCapt   = 16;
    //vFilters = new GR_FilterSrc[iCapt];
}

int GR_FilterList::Expand()
{
    this->iCapt *= 2;
    GR_FilterSrc    *vTmpFilters = new GR_FilterSrc[this->iCapt];
    memcpy(vTmpFilters, this->vFilters, this->iCapt/2);
    delete []this->vFilters;
    this->vFilters = vTmpFilters;
    
    return GR_OK;
}

int GR_FilterList::AddFilter(string &strSrc, string &strUpperSrc)
{
    try
    {
        if (iNum == this->iCapt)
        {
            if (GR_OK != this->Expand())
            {
                GR_LOGE("expand filter pool failed.");
                return GR_ERROR;
            }
        }
        //this->vFilters[iNum].szUpperSrc = (char*)strUpperSrc.c_str();
        this->vFilters[iNum].szSrc = (char*)strSrc.c_str();
        this->vFilters[iNum].iLen = strSrc.length();
        ++iNum;
    }
    catch(exception &e)
    {
        GR_LOGE("add filter got exception:%s", e.what());
        return GR_ERROR;
    }
    
    return GR_OK;
}

// TODO改为字典匹配
bool GR_FilterList::Match(char *szSrc)
{
    GR_FilterSrc *pSrc = &(this->vFilters[0]);
    bool bNotMatch;
    char *szMatch;
    char *szTmpSrc;
    for (int i=0; i<this->iNum; ++pSrc)
    {
        bNotMatch = false;
        szMatch = pSrc->szSrc;
        szTmpSrc = szSrc;
        for(int j=0; j<pSrc->iLen; ++j, ++szMatch, ++szTmpSrc)
        {
            if (szTmpSrc == szMatch)
            {
                continue;
            }
            bNotMatch = true;
            break;
        }
        if (!bNotMatch)
        {
            return true;
        }
    }

    return false;
}

GR_Filter* GR_Filter::m_pInstance = new GR_Filter();

GR_Filter::GR_Filter()
{
}

GR_Filter::~GR_Filter()
{
}

GR_Filter *GR_Filter::Instance()
{
    return m_pInstance;
}

int GR_Filter::Init(GR_Config *pConfig)
{
    vector<string> &vCfgFilter = pConfig->m_ReplicateInfo.m_vFilters;
    // 便利规则，将规则中字符串全部转为小写保存在原处
    // 将规则中所有字符串转换为大写保存在vUpCaseFilter中
    for(auto itr=vCfgFilter.begin(); itr!=vCfgFilter.end(); itr++)
    {
        if (itr->length() > this->iMaxLen)
        {
            this->iMaxLen = itr->length();
            if (this->iMaxLen > 256)
            {
                GR_LOGE("invalid pre-filters:%s", itr->c_str());
                return GR_ERROR;
            }
        }
        //transform(itr->begin(), itr->end(), itr->begin(), ::tolower);
        //string strTmp = *itr;
        //transform(strTmp.begin(), strTmp.end(), strTmp.begin(), ::toupper);
        //this->vUpCaseFilter.push_back(strTmp);
    }
    if(this->iMaxLen == 0)
    {
        return GR_OK;
    }
    this->iMaxLen += 1;
    if (this->iMaxLen == 0)
    {
        return GR_OK;
    }
    this->vFilters = new GR_FilterList[this->iMaxLen];
    // 下标为i,则将(<=i+1)的字符串保存在,i对应的元素中
    for(int i=1; i<this->iMaxLen; i++)
    {
        GR_FilterList   *pFilter = this->vFilters + i;
        pFilter->iNum = 0;
        pFilter->iCapt = 16;
        pFilter->vFilters = new GR_FilterSrc[pFilter->iCapt];
        for (int j=0; j<vCfgFilter.size(); j++)
        {
            if (vCfgFilter[j].length() > i)
            {
                continue;
            }
            this->vFilters[i].AddFilter(vCfgFilter[j], this->vUpCaseFilter[j]);
        }
    }
    
    return GR_OK;
}

bool GR_Filter::Match(char *szSrc, int iLen)
{
    // 不需要过滤数据
    if (iMaxLen <= 1)
    {
        return true;
    }
    if (iLen >= iMaxLen)
    {
        iLen = iMaxLen - 1;
    }
    if (iLen == 0)
    {
        return false;
    }
    
    return this->vFilters[iLen].Match(szSrc);
}