#include "dispatch.h"
#include "hash.h"

#include <random>
#include <cmath>

#define KETAMA_CONTINUUM_ADDITION   10  /* # extra slots to build into continuum */
#define KETAMA_POINTS_PER_SERVER    160 /* 40 points per hash */
#define KETAMA_MAX_HOSTLEN          86

#define MODULA_CONTINUUM_ADDITION   10  /* # extra slots to build into continuum */
#define MODULA_POINTS_PER_SERVER    1


uint32
ketama_hash(const char *key, size_t key_length, uint32 alignment)
{
    unsigned char results[16];

    GR_Hash::Md5Signature((unsigned char*)key, key_length, results);

    return ((uint32) (results[3 + alignment * 4] & 0xFF) << 24)
        | ((uint32) (results[2 + alignment * 4] & 0xFF) << 16)
        | ((uint32) (results[1 + alignment * 4] & 0xFF) << 8)
        | (results[0 + alignment * 4] & 0xFF);
}

int
ketama_item_cmp(const void *t1, const void *t2)
{
    const continuum *ct1 = (const continuum *)t1, *ct2 = (const continuum *)t2;

    if (ct1->value == ct2->value) {
        return 0;
    } else if (ct1->value > ct2->value) {
        return 1;
    } else {
        return -1;
    }

}

int GR_Dispatch::KetamaUpdate(GR_RedisServer     **vRedisServers, continuum *&vServers, int iLen, int &ncontinuum)
{
    uint32 nserver;             /* # server - live and dead */
    uint32 nlive_server;        /* # live server */
    uint32 pointer_per_server;  /* pointers per server proportional to weight */
    uint32 pointer_per_hash;    /* pointers per hash */
    uint32 pointer_counter;     /* # pointers on continuum */
    uint32 pointer_index;       /* pointer index */
    uint32 continuum_index;     /* continuum index */
    uint32 server_index;        /* server index */
    uint32 value;               /* continuum value */
    uint32 total_weight;        /* total live server weight */

    nserver = iLen;
    nlive_server = nserver;
    total_weight = 0;
    for (server_index = 0; server_index < nserver; server_index++) {
        GR_RedisServer *server = vRedisServers[server_index];
        total_weight += server->iWeight;
    }

    /*
    * Allocate the continuum for the pool, the first time, and every time we
    * add a new server to the pool
    */
    uint32_t nserver_continuum = nlive_server + KETAMA_CONTINUUM_ADDITION;
    ncontinuum = nserver_continuum * KETAMA_POINTS_PER_SERVER;
    vServers = new continuum[ncontinuum];
    
    /*
     * Build a continuum with the servers that are live and points from
     * these servers that are proportial to their weight
     */
    continuum_index = 0;
    pointer_counter = 0;
    for (server_index = 0; server_index < nserver; server_index++) {
        struct GR_RedisServer *server;
        float pct;

        server = vRedisServers[server_index];

        pct = (float)server->iWeight / (float)total_weight;
        pointer_per_server = (uint32) ((floorf((float) (pct * KETAMA_POINTS_PER_SERVER / 4 * (float)nlive_server + 0.0000000001))) * 4);
        pointer_per_hash = 4;

        for (pointer_index = 1;
             pointer_index <= pointer_per_server / pointer_per_hash;
             pointer_index++) {

            char host[KETAMA_MAX_HOSTLEN]= "";
            size_t hostlen;
            uint32 x;

            hostlen = snprintf(host, KETAMA_MAX_HOSTLEN, "%.*s-%u",
                               server->strAddr.length(), server->strAddr.c_str(),
                               pointer_index - 1);

            for (x = 0; x < pointer_per_hash; x++) {
                value = ketama_hash(host, hostlen, x);
                vServers[continuum_index].index = server_index;
                vServers[continuum_index++].value = value;
            }
        }
        pointer_counter += pointer_per_server;
    }

    ncontinuum = pointer_counter;
    qsort(vServers, pointer_counter, sizeof(continuum),
          ketama_item_cmp);
    return GR_OK;
}

GR_RedisEvent* GR_Dispatch::Ketama(GR_RedisServer **vRedisServers, continuum *vServers, int iLen, uint32 uiKey)
{
    continuum *begin, *end, *left, *right, *middle;
    begin = left = vServers;
    end = right = vServers + iLen;
    while (left < right) 
    {
        middle = left + (right - left) / 2;
        if (middle->value < uiKey) 
        {
          left = middle + 1;
        } 
        else 
        {
          right = middle;
        }
    }

    if (right == end) 
    {
        right = begin;
    }

    return vRedisServers[right->index]->pEvent;
}

int GR_Dispatch::ModulaUpdate(GR_RedisServer     **vRedisServers, continuum *&vServers, int iLen, int &ncontinuum)
{
    uint32 nserver;             /* # server - live and dead */
    uint32 continuum_index = 0;     /* continuum index */
    uint32 server_index;        /* server index */
    uint32 weight_index;        /* weight index */
    uint32 total_weight;        /* total live server weight */
    uint32 pointer_counter = 0;     /* # pointers on continuum */
    uint32 pointer_per_server;  /* pointers per server proportional to weight */

    nserver = iLen;

    total_weight = 0;

    for (server_index = 0; server_index < nserver; server_index++) {
        GR_RedisServer *server = vRedisServers[server_index];
        total_weight += server->iWeight;
    }

    uint32_t nserver_continuum = total_weight + MODULA_CONTINUUM_ADDITION;
    ncontinuum = nserver_continuum *  MODULA_POINTS_PER_SERVER;

    vServers = new continuum[ncontinuum];

    /* update the continuum with the servers that are live */
    for (server_index = 0; server_index < nserver; server_index++) {
        GR_RedisServer *server = vRedisServers[server_index];

        for (weight_index = 0; weight_index < server->iWeight; weight_index++) {
            pointer_per_server = 1;

            vServers[continuum_index].index = server_index;
            vServers[continuum_index++].value = 0;

            pointer_counter += pointer_per_server;
        }
    }
    ncontinuum = pointer_counter;
    return GR_OK;
}

GR_RedisEvent* GR_Dispatch::Modula(GR_RedisServer **vRedisServers, continuum *vServers, int iLen, uint32 uiKey)
{
    continuum *pServer;
    pServer = vServers + uiKey%iLen;
    return vRedisServers[pServer->index]->pEvent;
}

GR_RedisEvent* GR_Dispatch::Random(GR_RedisServer **vRedisServers, continuum *vServers, int iLen, uint32 uiKey)
{
    static default_random_engine random;
    continuum *pServer;
    pServer = vServers + random()%iLen;
    return vRedisServers[pServer->index]->pEvent;
}


