#ifndef _GR_MASTER_INFO_CFG_H__
#define _GR_MASTER_INFO_CFG_H__
#include "include.h"
#include <unordered_map>

class GR_MasterInfoCfg
{
public:
    int Init(const char *szPath);

    unordered_map<string, string> m_mapMasters;
};
#endif
