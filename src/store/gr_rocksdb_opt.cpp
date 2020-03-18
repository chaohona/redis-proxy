#include "gr_rocksdb_opt.h"
#include "log.h"

GR_RocksDBOPT::GR_RocksDBOPT()
{
}

GR_RocksDBOPT::~GR_RocksDBOPT()
{
    try
    {
        if (this->m_pDB != nullptr)
        {
            delete this->m_pDB;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("destroy rocksdb opt failed.");
    }
}

int GR_RocksDBOPT::Init(GR_Config &config)
{
    try
    {
        this->m_strDBPath = config.m_storeInfo.strDBPath;
        Options options;
        // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction();
        // create the DB if it's not already present
        options.create_if_missing = true;

        // open DB
        Status s = DB::Open(options, this->m_strDBPath, &this->m_pDB);
        if (!s.ok())
        {
            GR_LOGE("open database failed, dbpath:%s", this->m_strDBPath.c_str());
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        GR_LOGE("init rocksdb opt failed.");
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_RocksDBOPT::Put(char *szKey, char *szValue)
{
    try
    {
        char *szErr = nullptr;
        Status s = this->m_pDB->Put(WriteOptions(), szKey, szValue);
        if (!s.ok())
        {
            // TODO 告警
            GR_LOGE("put data failed, key:%s", szKey);
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        // TODO 告警
        GR_LOGE("set data to rocksdb got exception, key:%s, err:%s", szKey, e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

int GR_RocksDBOPT::Get(char *szKey, string &strValue)
{
    try
    {
        Status s = this->m_pDB->Get(ReadOptions(), szKey, strValue);
        if (!s.ok())
        {
            // TODO 告警
            GR_LOGE("got value got failed, key:%s", szKey);
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        // TODO 告警
        GR_LOGE("get value got exception:%s", e.what());
        return GR_ERROR;
    }

    return GR_OK;
}

int GR_RocksDBOPT::Delete(char * szKey)
{
    try
    {
        Status s = this->m_pDB->SingleDelete(WriteOptions(), szKey);
        if (!s.ok())
        {
            GR_LOGE("delete key from rocksdb failed, key:%s", szKey);
            return GR_ERROR;
        }
    }
    catch(exception &e)
    {
        // TODO 告警
        GR_LOGE("delete data got exception:%s", e.what());
        return GR_ERROR;
    }
    return GR_OK;
}

