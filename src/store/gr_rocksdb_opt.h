#ifndef _GR_ROCKSDB_OPT__
#define _GR_ROCKSDB_OPT__

#include "include.h"

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"



class GR_RocksDBOPT
{
public:
    GR_RocksDBOPT();
    ~GR_RocksDBOPT();

    int Init(GR_Config &config);

    int Get(char *szKey, string &strValue);
    int Put(char *szKey, char *szValue);
    int Delete(char *szKey);


private:
    // rocksdb操作句柄
    DB *m_pDB = nullptr;
    Options  *m_pOpt = nullptr;  

    // 配置
    string m_strDBPath;
    string m_strDBBackPath;
};


#endif
