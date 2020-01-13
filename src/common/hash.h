#ifndef _GR_HASH_H__
#define _GR_HASH_H__
#include "define.h"

class GR_Hash
{
public:
    static GR_Hash* Instance();
    ~GR_Hash();

    static uint32 Md5(const char *szKey, size_t iKeyLen);
    static uint32 Crc16(const char *szKey, size_t iKeyLen);
    static uint32 Crc32(const char *szKey, size_t iKeyLen);
    static uint32 Crc32a(const char *szKey, size_t iKeyLen);
    static uint32 Fnv164(const char *szKey, size_t iKeyLen);
    static uint32 Fnv1a64(const char *szKey, size_t iKeyLen);
    static uint32 Fnv132(const char *szKey, size_t iKeyLen);
    static uint32 Fnv1a32(const char *szKey, size_t iKeyLen);
    static uint32 Hsieh(const char *szKey, size_t iKeyLen);
    static uint32 Jenkins(const char *szKey, size_t iKeyLen);
    static uint32 Murmur(const char *szKey, size_t iKeyLen);
public:
    static void Md5Signature(unsigned char *key, unsigned long length, unsigned char *result);

private:
    GR_Hash();
    static GR_Hash* m_pInstance;
};

#endif
