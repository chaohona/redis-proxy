#include "gr_rdb.h"
#include "include.h"
#include "define.h"
#include "gr_redis_define.h"

#define RDB_NOW_POS_STEP(step)\
this->m_szNowPos += (step);

#define RDB_NOW_LEN(szNowPos)\
(this->m_pReadCache->m_szMsgEnd - (szNowPos))

#define RDB_FINISH_LOOP()               \
this->m_lExpireTime = 0;                \
this->m_iStatus = GR_RDB_LOAD_OPCODE;   \
this->m_rdbValueType =RDB_TYPE_STRING;  \
this->m_iValueItems = 0;                \
if (this->m_pKey)                       \
{                                       \
    this->m_pKey->Release();            \
    this->m_pKey = nullptr;             \
}                                       \
if (this->m_pValue)                     \
{                                       \
    this->m_pValue->Release();          \
    this->m_pValue = nullptr;           \
}

int GR_Rdb::Init(GR_MsgBufferMgr *pReadCache)
{
    this->m_pReadCache = pReadCache;
    this->m_szNowPos = this->m_pReadCache->m_szMsgStart;
    this->m_iStatus = GR_RDB_INIT;
    this->m_rdbOPCode = 0;
    this->m_rdbValueType = RDB_TYPE_STRING;
    this->m_lExpireTime = 0;
    this->m_iRdbVer = 0;
    this->m_ulLFUFreq = 0;
    this->m_ulLRUIdle = 0;
    this->m_pKey = nullptr;
    this->m_pValue = nullptr;
    return GR_OK;
}

// 循环执行
int GR_Rdb::Parse()
{
    int iRet = GR_OK;
    while(this->m_pReadCache->m_szMsgEnd > this->m_szNowPos)
    {
        switch (this->m_iStatus)
        {
            case GR_RDB_LOAD_OPCODE:
            {
                this->m_rdbOPCode = (uchar)(*this->m_szNowPos);
                RDB_NOW_POS_STEP(1);
                this->m_iStatus = GR_RDB_LOADED_OPCODE;
                break;
            }
            case GR_RDB_LOADED_OPCODE:
            {
                // 
                iRet = this->ParseOPCode(this->m_szNowPos, this->m_rdbOPCode, this->m_lExpireTime, this->m_ulLFUFreq, 
                    this->m_ulLRUIdle, this->m_iStatus);
                if (GR_OK != iRet)
                {
                    return iRet;
                }
                break;
            }
            case GR_RDB_LOAD_VALUE_TYPE:
            {
                this->m_rdbValueType = (uchar)(*this->m_szNowPos);
                RDB_NOW_POS_STEP(1);
                this->m_iStatus = GR_RDB_WAIT_GET_KEY;
                break;
            }
            case GR_RDB_WAIT_GET_KEY:
            {
                int iLen;
                int iSkip;
                iRet = this->RdbLoadStringObject(this->m_szNowPos, RDB_LOAD_NONE, iLen, iSkip, &this->m_pKey);
                if (iRet != GR_OK)
                {
                    return iRet;
                }
                RDB_NOW_POS_STEP(iSkip);
                this->m_iStatus = GR_RDB_WAIT_GET_VALUE;
                break;
            }
            case GR_RDB_WAIT_GET_VALUE:
            {
                int iSkip;
                iRet = this->RdbLoadObject(this->m_szNowPos, this->m_rdbValueType, iSkip, &this->m_pValue);
                if (iRet != GR_OK)
                {
                    return iRet;
                }
                RDB_NOW_POS_STEP(iSkip);
                int64 lNow = GR_GetNowMS();
                if (this->m_lExpireTime != 0 && this->m_lExpireTime < lNow)
                {
                    RDB_FINISH_LOOP();
                    break;
                }
                // TODO 将数据转换成redis请求，发送给后端redis
                
                // 一条数据处理完,初始化数据
                RDB_FINISH_LOOP();
                break;
            }
            case GR_RDB_PS_LOADTIME:
            {
                break;
            }
            case GR_RDB_INIT:
            {
                if (RDB_NOW_LEN(this->m_szNowPos) < 9)
                {
                    return GR_OK;
                }
                // 校验头
                this->m_szNowPos[9] = '\0';
                if (memcmp(this->m_szNowPos, "REDIS", 5))
                {
                    GR_LOGE("Wrong signature trying to load DB from file");
                    return GR_ERROR;
                }
                m_iRdbVer = atoi(this->m_szNowPos+5);
                if (m_iRdbVer<1 || m_iRdbVer>RDB_VERSION)
                {
                    GR_LOGE("Can't handle RDB format version %d", m_iRdbVer);
                    return GR_ERROR;
                }
                RDB_NOW_POS_STEP(9);
                this->m_iStatus = GR_RDB_LOAD_OPCODE;
                break;
            }
        }
    };
    
    return GR_OK;
}

int GR_Rdb::ParseOPCode(char *&szNowPos, int iOpCode, int64 &lExpireTime, uint64 &ulLFUFreq, uint64 &ulLRUIdle, GR_RDB_STATUS &iStatus)
{
    int iRet = GR_OK;
    iStatus = GR_RDB_LOAD_OPCODE;
    switch (iOpCode)
    {
        case RDB_OPCODE_EXPIRETIME:
        {
            iRet = RDBLoadTime(szNowPos, lExpireTime);
            if (GR_OK == iRet)
            {
                lExpireTime *= 1000;
                RDB_NOW_POS_STEP(4);
                return GR_OK;
            }
            if (GR_EAGAIN != iRet)
            {
                GR_LOGE("load expiretime failed.");
                return iRet;
            }
            return iRet;
        }
        case RDB_OPCODE_EXPIRETIME_MS:
        {
            iRet = RdbLoadMillisecondTime(szNowPos, lExpireTime);
            if (GR_OK == iRet)
            {
                RDB_NOW_POS_STEP(8);
                return GR_OK;
            }
            if (GR_EAGAIN != iRet)
            {
                GR_LOGE("load expiretime failed.");
                return iRet;
            }
            return iRet;
        }
        case RDB_OPCODE_FREQ:
        {
            ulLFUFreq = *szNowPos;
            RDB_NOW_POS_STEP(1);
            break;
        }
        case RDB_OPCODE_IDLE:
        {
            int iSkip = 0;
            int iSencoded;
            iRet = this->RdbLoadLenByRef(szNowPos, ulLRUIdle, iSkip, iSencoded);
            if (GR_OK != iRet)
            {
                GR_LOGE("rdb load idle failed.");
                return iRet;
            }
            RDB_NOW_POS_STEP(iSkip);
            break;
        }
        case RDB_OPCODE_EOF:
        {
            /* EOF: End of file, exit the main loop. */
            return GR_RDB_END;
        }
        case RDB_OPCODE_SELECTDB:
        {
            uint64 ulDBId = 0;
            int iSkip = 0;
            int iSencoded;
            iRet = this->RdbLoadLenByRef(szNowPos, ulDBId, iSkip, iSencoded);
            if (iRet != GR_OK)
            {
                return iRet;
            }
            if (ulDBId != 0)
            {
                GR_LOGE("rdb load selectdb failed, dbid %d", ulDBId);
                return GR_ERROR;
            }
            RDB_NOW_POS_STEP(iSkip);
            break;
        }
        case RDB_OPCODE_RESIZEDB:
        {
            // 跳过16个字符
            /* RESIZEDB: Hint about the size of the keys in the currently
             * selected data base, in order to avoid useless rehashing. */
            uint64_t db_size, expires_size;
            int iSkip = 0;
            int iSencoded;
            int iTmpSkip = 0;
            iRet = this->RdbLoadLenByRef(szNowPos, db_size, iSkip, iSencoded);
            if (iRet != GR_OK)
            {
                return iRet;
            }
            iRet = this->RdbLoadLenByRef(szNowPos+iSkip, expires_size, iTmpSkip, iSencoded);
            if (iRet != GR_OK)
            {
                return iRet;
            }
            RDB_NOW_POS_STEP(iSkip + iTmpSkip);
            break;
        }
        case RDB_OPCODE_AUX:
        {
            /* AUX: generic string-string fields. Use to add state to RDB
             * which is backward compatible. Implementations of RDB loading
             * are requierd to skip AUX fields they don't understand.
             *
             * An AUX field is composed of two strings: key and value. */
            // 跳过两个字符串长度
            int iLen;
            int iSkip = 0;
            int iTmpSkip = 0;
            iRet = RdbLoadStringObject(szNowPos, RDB_LOAD_NONE, iLen, iSkip, nullptr);
            if (iRet != GR_OK)
            {
                return iRet;
            }
            iRet = RdbLoadStringObject(szNowPos+iSkip, RDB_LOAD_NONE, iLen, iTmpSkip, nullptr);
            if (iRet != GR_OK)
            {
                return iRet;
            }
            RDB_NOW_POS_STEP(iSkip + iTmpSkip);
            break;
        }
        case RDB_OPCODE_MODULE_AUX: // 暂时不支持
        {
            GR_LOGE("load rdb, not support modeule_aux.");
            return GR_ERROR;
        }
        default:
        {
            iStatus = GR_RDB_LOAD_VALUE_TYPE;
        }
    }
    return GR_OK;
}

int GR_Rdb::RDBLoadTime(char *szNowPos, int64& lTime)
{
    if (RDB_NOW_LEN(szNowPos) < 4)
    {
        return GR_EAGAIN;
    }
    lTime = (int32)szNowPos[0]<<3 | (int32)szNowPos[1]<<2 | (int32)szNowPos[2]<<1 | (int32)szNowPos[3]; 

    return GR_OK;
}

int GR_Rdb::RdbLoadMillisecondTime(char *szNowPos, int64& lTime)
{
    if (RDB_NOW_LEN(szNowPos) < 8)
    {
        return GR_EAGAIN;
    }
    GR_BYTE_TO_UINT64(szNowPos, lTime);

    return GR_OK;
}

// lLen为获取的结果,iSkip为数据占用的位数
int GR_Rdb::RdbLoadLenByRef(char *szNowPos, uint64 &lLen, int &iSkip, int &iSencoded)
{
    iSencoded = 0;
    iSkip = 0;
    int type = (szNowPos[0]&0xc0)>>6;
    if (type == RDB_ENCVAL) {
        /* Read a 6 bit encoding type. */
        lLen = szNowPos[0]&0x3F;
        iSencoded = 1;
        iSkip = 1;
    } else if (type == RDB_6BITLEN) {
        /* Read a 6 bit len. */
        lLen = szNowPos[0]&0x3F;
        iSkip = 1;
    } else if (type == RDB_14BITLEN) {
        /* Read a 14 bit len. */
        if (RDB_NOW_LEN(szNowPos) < 2)
        {
            return GR_EAGAIN;
        }
        lLen = ((szNowPos[0]&0x3F)<<8)|szNowPos[1];
        iSkip = 2;
    } else if (szNowPos[0] == RDB_32BITLEN) {
        /* Read a 32 bit len. */
        if (RDB_NOW_LEN(szNowPos) < 5)
        {
            return GR_EAGAIN;
        }
        uint32 len;
        GR_BYTE_TO_UINT32(szNowPos+1, len);
        lLen = ntohl(len);
        iSkip = 5;
    } else if (szNowPos[0] == RDB_64BITLEN) {
        if (RDB_NOW_LEN(szNowPos) < 9)
        {
            return GR_EAGAIN;
        }
        /* Read a 64 bit len. */
        uint64_t len;
        GR_BYTE_TO_UINT64(szNowPos+1, len);
        lLen = len;
        iSkip = 9;
    } else {
        GR_LOGE("Unknown length encoding %d in rdbLoadLen()",type);
        return GR_ERROR; /* Never reached. */
    }
    return GR_OK;
}

int GR_Rdb::RdbLoadStringObject(char *szNowPos, int flags, int &iLen, int &iSkip, GR_MemPoolData **pValue)
{
    iLen = 0;
    iSkip = 0;
    int encode = flags & RDB_LOAD_ENC;
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    int isencoded;
    uint64 ulStrLen;
    int iRet = GR_OK;
    int iTmpSkip;

    iRet = this->RdbLoadLenByRef(szNowPos, ulStrLen, iTmpSkip, isencoded);
    if (GR_OK != iRet)
    {
        return iRet;
    }
    iSkip += iTmpSkip;

    iRet = GR_OK;
    if (isencoded)
    {
        switch(ulStrLen)
        {
            case RDB_ENC_INT8:
            case RDB_ENC_INT16:
            case RDB_ENC_INT32:
            {
                iRet = this->RdbLoadIntegerObject(szNowPos+iSkip, (int)ulStrLen, flags, iTmpSkip, pValue);
                iSkip += iTmpSkip;
                return iRet;
            }
            case RDB_ENC_LZF:
            {
                iRet = this->RdbLoadLzfStringObject(szNowPos+iSkip, flags, iTmpSkip, pValue);
                iSkip += iTmpSkip;
                return iRet;
            }
            default:
            {
                GR_LOGE("Unknown RDB string encoding type %d", ulStrLen);
                return GR_ERROR;
            }
        }
    }
    if (GR_OK != iRet)
    {
        return iRet;
    }
    if (RDB_NOW_LEN(szNowPos+iSkip) < ulStrLen)
    {
        return GR_EAGAIN;
    }
    iSkip += ulStrLen;
    // TODO 参考 rdbGenericLoadStringObject 加载字符串
    
    return GR_OK;
}

int GR_Rdb::RdbLoadIntegerObject(char *szNowPos, int enctype, int flags, int &iSkip, GR_MemPoolData **pValue)
{
    ASSERT(pValue!=nullptr);
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    int encode = flags & RDB_LOAD_ENC;
    unsigned char enc[4];
    long long val;

    if (enctype == RDB_ENC_INT8) {
        if (RDB_NOW_LEN(szNowPos)<1)
        {
            return GR_EAGAIN;
        }
        val = (signed char)szNowPos[0];
        iSkip = 1;
    } else if (enctype == RDB_ENC_INT16) {
        if (RDB_NOW_LEN(szNowPos)<2)
        {
            return GR_EAGAIN;
        }
        uint16_t v;
        v = uint16_t(szNowPos[0])|uint16_t(szNowPos[1]<<8);
        val = (int16_t)v;
        iSkip = 2;
    } else if (enctype == RDB_ENC_INT32) {
        if (RDB_NOW_LEN(szNowPos)<4)
        {
            return GR_EAGAIN;
        }
        uint32_t v;
        v = szNowPos[0]|(szNowPos[1]<<8)|(szNowPos[2]<<16)|(szNowPos[3]<<24);
        val = (int32_t)v;
        iSkip = 4;
    } else {
        GR_LOGE("Unknown RDB integer encoding type %d", enctype);
        return GR_ERROR; /* Never reached. */
    }
    if (plain || sds)
    {
        /**pValue = GR_MEMPOOL_GETDATA(1+LONG_STR_SIZE);
        char *szData = (*pValue)->m_uszData;
        szData[0] = ':';
        int len = ll2string(szData+1,sizeof(buf),val);
        szData[1+len] = '\r';
        szData[2+len] = '\n';*/
    }
    else if (encode)
    {
        /**pValue = GR_MEMPOOL_GETDATA(1+LONG_STR_SIZE);
        char *szData = (*pValue)->m_uszData;
        szData[0] = '$';
        int len = ll2string(szData+1,sizeof(buf),val);
        szData[1+len] = '\r';
        szData[2+len] = '\n';*/
    }
    else
    {
    }
    // TODO 参照rdbLoadIntegerObject，解析integer数据
    return GR_OK;
}

int GR_Rdb::RdbLoadLzfStringObject(char *szNowPos, int flags, int &iSkip, GR_MemPoolData **pValue)
{
    int plain = flags & RDB_LOAD_PLAIN;
    int sds = flags & RDB_LOAD_SDS;
    uint64_t len, clen;
    unsigned char *c = NULL;
    char *val = NULL;
    int iSencoded;

    int iTmpSkip = 0;
    int iRet = GR_OK;
    iRet = this->RdbLoadLenByRef(szNowPos, clen, iSkip, iSencoded);
    if (iRet != GR_OK)
    {
        return iRet;
    }
    iRet = this->RdbLoadLenByRef(szNowPos, len, iTmpSkip, iSencoded);
    if (iRet != GR_OK)
    {
        return iRet;
    }
    iSkip += iTmpSkip;
    // TODO 参照rdbLoadLzfStringObject加载数据
    iSkip += clen;
    return GR_OK;
}

int GR_Rdb::RdbLoadObject(char *szNowPos, int rdbtype, int &iSkip, GR_MemPoolData **pValue)
{
    int iRet = GR_OK;
    int iLen;
    if (rdbtype == RDB_TYPE_STRING) 
    {
        /* Read string value */
        iRet = RdbLoadStringObject(szNowPos, RDB_LOAD_ENC, iLen, iSkip, pValue);
        if (iRet != GR_OK)
        {
            return iRet;
        }
        
        return GR_OK;
    } 
    else if (rdbtype == RDB_TYPE_LIST) 
    {
        // 获取list个数
        int isencoded;
        uint64 ulLen = 0;
        iRet = this->RdbLoadLenByRef(szNowPos, ulLen, iSkip, isencoded);
        if (iRet != GR_OK)
        {
            return iRet;
        }
        szNowPos += iSkip;
        int iTmpSkip = 0;
        while(ulLen--)
        {
            iRet = this->RdbLoadStringObject(szNowPos, RDB_LOAD_ENC, iLen, iTmpSkip, nullptr);
            if (iRet != GR_OK)
            {
                return iRet;
            }
            szNowPos += iTmpSkip;
            iSkip += iTmpSkip;
        }
    } 
    else if (rdbtype == RDB_TYPE_SET) 
    {
        int isencoded;
        uint64 ulLen = 0;
        iRet = this->RdbLoadLenByRef(szNowPos, ulLen, iSkip, isencoded);
        if (iRet != GR_OK)
        {
            return iRet;
        }
        szNowPos += iSkip;
        for(int i=0; i<ulLen; i++)
        {
            //(char *szNowPos, int flags, int &iLen, int &iSkip, size_t *lenptr)
            int iStrLen = 0;
            int iTmpSkip = 0;
            iRet = this->RdbLoadStringObject(szNowPos, RDB_LOAD_SDS, iStrLen, iTmpSkip, nullptr);
            if (GR_OK != iRet)
            {
                return iRet;
            }
            iSkip += iTmpSkip;
        }
        return GR_OK;
    } 
    else if (rdbtype == RDB_TYPE_ZSET_2 || rdbtype == RDB_TYPE_ZSET) 
    {
        
    } 
    else if (rdbtype == RDB_TYPE_HASH) 
    {
    } 
    else if (rdbtype == RDB_TYPE_LIST_QUICKLIST) 
    {
    } 
    else if (rdbtype == RDB_TYPE_HASH_ZIPMAP  ||
               rdbtype == RDB_TYPE_LIST_ZIPLIST ||
               rdbtype == RDB_TYPE_SET_INTSET   ||
               rdbtype == RDB_TYPE_ZSET_ZIPLIST ||
               rdbtype == RDB_TYPE_HASH_ZIPLIST)
    {
    } 
    else if (rdbtype == RDB_TYPE_STREAM_LISTPACKS) 
    {
    } 
    else if (rdbtype == RDB_TYPE_MODULE || rdbtype == RDB_TYPE_MODULE_2) // 赞不支持
    { 
        return GR_ERROR;
    } 
    else 
    {
        return GR_ERROR;
    }
    return GR_OK;
}

int GR_Rdb::RdbParseString(char *szNowPos, int iInLen, int &iSkip)
{
    int iRet = GR_OK;
    int iLen;
    /* Read string value */
    iRet = RdbLoadStringObject(szNowPos, RDB_LOAD_ENC, iLen, iSkip, nullptr);
    if (iRet != GR_OK)
    {
        return iRet;
    }
    
    return GR_OK;
}


