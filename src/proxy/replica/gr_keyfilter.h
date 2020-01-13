#ifndef _GR_KEY_FILTER_H__
#define _GR_KEY_FILTER_H__
#include "define.h"
#include "config.h"

// 字符串比较

struct GR_FilterSrc
{
    int     iLen;   // 被比较的字符串的长度
    char*   szSrc;  // 被比较的字符串
    char*   szUpperSrc;
};

struct GR_FilterList
{
public:
    GR_FilterList();
    int Expand();
    int AddFilter(string &strSrc, string &strUpperSrc);
    bool Match(char *szSrc);
    int             iNum;
    int             iCapt;
    GR_FilterSrc    *vFilters;
};

// 将源分为大写与小写的两种字符串，比较的时候针对每个字符比较大写或者小写，有一种相同则认为相同
// upper: ABCDE
// lower: abcde
// src会一次和A(a),B(b),C(c),D(d),E(e)比较，有一个不相等则认为不相等
class GR_Filter
{
public:
    ~GR_Filter();
    int Init(GR_Config *pConfig);
    bool Match(char *szSrc, int iLen);  // 

    static GR_Filter* Instance();
public:
    int iMaxLen = 0;    // 比较源的最大长度
    GR_FilterList   *vFilters;      // 下标为i，则保存了(<=i+1)的长度的所有源数据
    vector<string>  vUpCaseFilter;  // 大写的源

private:
    GR_Filter();
    static GR_Filter* m_pInstance;
};

#endif
