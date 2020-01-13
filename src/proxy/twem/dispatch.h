#ifndef _GR_DISPATCH_H__
#define _GR_DISPATCH_H__

#include "route.h"
#include "include.h"

struct continuum {
    uint32 index;  /* server index */
    uint32 value;  /* hash value */
};

class GR_Dispatch
{
public:
    static GR_RedisEvent* Ketama(GR_RedisServer **vRedisServers, continuum *vServers, int iLen, uint32 uiKey);
    static int KetamaUpdate(GR_RedisServer     **vRedisServers, continuum *&vServers, int iLen, int &ncontinuum);
    
    static GR_RedisEvent* Modula(GR_RedisServer **vRedisServers, continuum *vServers, int iLen, uint32 uiKey);
    static int ModulaUpdate(GR_RedisServer     **vRedisServers, continuum *&vServers, int iLen, int &ncontinuum);
    
    static GR_RedisEvent* Random(GR_RedisServer **vRedisServers, continuum *vServers, int iLen, uint32 uiKey);
};

#endif
