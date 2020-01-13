#ifndef _GR_RDB_H__
#define _GR_RDB_H__

#include "include.h"
#include "define.h"
#include "msgbuffer.h"

#define RDB_VERSION 9

enum GR_RDB_STATUS
{
    GR_RDB_INIT,
    GR_RDB_LOAD_OPCODE,     // 开始加载OPCODE, 对应redis中rdbLoadType
    GR_RDB_LOADED_OPCODE,   // 加载完OPCODE，根据OPCode执行下一步
    GR_RDB_LOAD_VALUE_TYPE, // 加载value类型
    GR_RDB_WAIT_GET_KEY,    // 开始加载key
    GR_RDB_WAIT_GET_VALUE,  // 开始加载value
    GR_RDB_PS_LOADTIME,     
};

/* The current RDB version. When the format changes in a way that is no longer
 * backward compatible this number gets incremented. */
#define RDB_VERSION 9

/* Defines related to the dump file format. To store 32 bits lengths for short
 * keys requires a lot of space, so we check the most significant 2 bits of
 * the first byte to interpreter the length:
 *
 * 00|XXXXXX => if the two MSB are 00 the len is the 6 bits of this byte
 * 01|XXXXXX XXXXXXXX =>  01, the len is 14 byes, 6 bits + 8 bits of next byte
 * 10|000000 [32 bit integer] => A full 32 bit len in net byte order will follow
 * 10|000001 [64 bit integer] => A full 64 bit len in net byte order will follow
 * 11|OBKIND this means: specially encoded object will follow. The six bits
 *           number specify the kind of object that follows.
 *           See the RDB_ENC_* defines.
 *
 * Lengths up to 63 are stored using a single byte, most DB keys, and may
 * values, will fit inside. */
#define RDB_6BITLEN 0
#define RDB_14BITLEN 1
#define RDB_32BITLEN 0x80
#define RDB_64BITLEN 0x81
#define RDB_ENCVAL 3
#define RDB_LENERR UINT64_MAX

/* When a length of a string object stored on disk has the first two bits
 * set, the remaining six bits specify a special encoding for the object
 * accordingly to the following defines: */
#define RDB_ENC_INT8 0        /* 8 bit signed integer */
#define RDB_ENC_INT16 1       /* 16 bit signed integer */
#define RDB_ENC_INT32 2       /* 32 bit signed integer */
#define RDB_ENC_LZF 3         /* string compressed with FASTLZ */

/* Map object types to RDB object types. Macros starting with OBJ_ are for
 * memory storage and may change. Instead RDB types must be fixed because
 * we store them on disk. */
#define RDB_TYPE_STRING 0
#define RDB_TYPE_LIST   1
#define RDB_TYPE_SET    2
#define RDB_TYPE_ZSET   3
#define RDB_TYPE_HASH   4
#define RDB_TYPE_ZSET_2 5 /* ZSET version 2 with doubles stored in binary. */
#define RDB_TYPE_MODULE 6
#define RDB_TYPE_MODULE_2 7 /* Module value with annotations for parsing without
                               the generating module being loaded. */
/* NOTE: WHEN ADDING NEW RDB TYPE, UPDATE rdbIsObjectType() BELOW */

/* Object types for encoded objects. */
#define RDB_TYPE_HASH_ZIPMAP    9
#define RDB_TYPE_LIST_ZIPLIST  10
#define RDB_TYPE_SET_INTSET    11
#define RDB_TYPE_ZSET_ZIPLIST  12
#define RDB_TYPE_HASH_ZIPLIST  13
#define RDB_TYPE_LIST_QUICKLIST 14
#define RDB_TYPE_STREAM_LISTPACKS 15
/* NOTE: WHEN ADDING NEW RDB TYPE, UPDATE rdbIsObjectType() BELOW */

/* Test if a type is an object type. */
#define rdbIsObjectType(t) ((t >= 0 && t <= 7) || (t >= 9 && t <= 15))

/* Special RDB opcodes (saved/loaded with rdbSaveType/rdbLoadType). */
#define RDB_OPCODE_MODULE_AUX 247   /* Module auxiliary data. */
#define RDB_OPCODE_IDLE       248   /* LRU idle time. */
#define RDB_OPCODE_FREQ       249   /* LFU frequency. */
#define RDB_OPCODE_AUX        250   /* RDB aux field. */
#define RDB_OPCODE_RESIZEDB   251   /* Hash table resize hint. */
#define RDB_OPCODE_EXPIRETIME_MS 252    /* Expire time in milliseconds. */
#define RDB_OPCODE_EXPIRETIME 253       /* Old expire time in seconds. */
#define RDB_OPCODE_SELECTDB   254   /* DB number of the following keys. */
#define RDB_OPCODE_EOF        255   /* End of the RDB file. */

/* Module serialized values sub opcodes */
#define RDB_MODULE_OPCODE_EOF   0   /* End of module value. */
#define RDB_MODULE_OPCODE_SINT  1   /* Signed integer. */
#define RDB_MODULE_OPCODE_UINT  2   /* Unsigned integer. */
#define RDB_MODULE_OPCODE_FLOAT 3   /* Float. */
#define RDB_MODULE_OPCODE_DOUBLE 4  /* Double. */
#define RDB_MODULE_OPCODE_STRING 5  /* String. */

/* rdbLoad...() functions flags. */
#define RDB_LOAD_NONE   0
#define RDB_LOAD_ENC    (1<<0)
#define RDB_LOAD_PLAIN  (1<<1)
#define RDB_LOAD_SDS    (1<<2)

#define RDB_SAVE_NONE 0
#define RDB_SAVE_AOF_PREAMBLE (1<<0)

// rdb格式解析，如果传入的rdb文件则需要先解析rdb文件头，如果传入的是单个数据对应的rdb则直接解析数据对应的rdb文件格式
class GR_Rdb
{
public:
    int Init(GR_MsgBufferMgr *pReadCache);
    
    int Parse();
public:
    int ParseOPCode(char *&szNowPos, int iOpCode, int64 &lExpireTime, uint64 &ulLFUFreq, uint64 &ulLRUIdle, GR_RDB_STATUS &iStatus);
    int RDBLoadTime(char *szNowPos, int64& lTime);
    int RdbLoadMillisecondTime(char *szNowPos, int64& lTime);
    int RdbLoadLenByRef(char *szNowPos, uint64 &lLen, int &iSkip, int &iSencoded);
    // 将字符串转换成正常的编码格式，并赋值给pValue
    int RdbLoadStringObject(char *szNowPos, int flags, int &iLen, int &iSkip, GR_MemPoolData **pValue);
    int RdbLoadLzfStringObject(char *szNowPos, int flags, int &iSkip, GR_MemPoolData **pValue);
    int RdbLoadEncodeStringObject(char *szNowPos, int flags, int &iLen, int &iSkip, GR_MemPoolData **pValue);
    // redis::rdbLoadIntegerObject
    int RdbLoadIntegerObject(char *szNowPos, int enctype, int flags, int &iSkip, GR_MemPoolData **pValue);
    int RdbLoadObject(char *szNowPos, int rdbtype, int &iSkip, GR_MemPoolData **pValue);

    // 解析上层的数据结构=================begin===========================
    // string类型
    int RdbParseString(char *szNowPos, int iInLen, int &iSkip);
    // 解析上层的数据结构=================end  =============================
private:
    GR_MsgBufferMgr*    m_pReadCache = nullptr;
    char*               m_szNowPos = nullptr;               // 当前解析所处的位置
    GR_RDB_STATUS       m_iStatus = GR_RDB_INIT;            // 解析当前所处状态
    int                 m_rdbOPCode = 0;                    // 获取的opcode
    int                 m_rdbValueType = RDB_TYPE_STRING;   // 数据类型
    int64               m_lExpireTime = 0;                  // 超时时间
    int                 m_iRdbVer = 0;                      // rdb版本号
    uint64              m_ulLFUFreq = 0;
    uint64              m_ulLRUIdle = 0;
    GR_MemPoolData      *m_pKey = nullptr;      // 每个元素的key
    GR_MemPoolData      *m_pValue = nullptr;    // 每个元素的value
    int                 m_iValueItems = 0;      // 每个数据的值中包含的元素的个数
};

#endif
