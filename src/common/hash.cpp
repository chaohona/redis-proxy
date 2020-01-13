#include "hash.h"
#include "define.h"

static const uint16 crc16tab[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
  0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
  0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
  0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
  0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
  0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
  0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
  0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
  0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
  0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
  0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
  0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
  0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
};

static const uint32 crc32tab[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

GR_Hash* GR_Hash::m_pInstance = new GR_Hash();

GR_Hash* GR_Hash::Instance()
{
    return m_pInstance;
}

GR_Hash::GR_Hash()
{
}

GR_Hash::~GR_Hash()
{
}

uint32 GR_Hash::Crc16(const char * szKey, size_t iKeyLen)
{
    uint64 x;
    uint32 crc = 0;

    for (x=0; x < iKeyLen; x++) {
        crc = (crc << 8) ^ crc16tab[((crc >> 8) ^ *szKey++) & 0x00ff];
    }

    return crc;
}

uint32
GR_Hash::Crc32(const char *szKey, size_t iKeyLen)
{
    uint64 x;
    uint32 crc = UINT32_MAX;

    for (x = 0; x < iKeyLen; x++) {
        crc = (crc >> 8) ^ crc32tab[(crc ^ (uint64)szKey[x]) & 0xff];
    }

    return ((~crc) >> 16) & 0x7fff;
}

uint32
GR_Hash::Crc32a(const char *key, size_t key_length)
{
    const char *p = key;
    uint32 crc;

    crc = ~0U;
    while (key_length--) {
        crc = crc32tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ ~0U;
}


uint32
GR_Hash::Murmur(const char *szKey, size_t iKeyLen)
{
    const unsigned int m = 0x5bd1e995;
    const uint32 seed = (0xdeadbeef * (uint32)iKeyLen);
    const int r = 24;


    /* Initialize the hash to a 'random' value */

    uint32 h = seed ^ (uint32)iKeyLen;

    /* Mix 4 bytes at a time into the hash */

    const unsigned char * data = (const unsigned char *)szKey;

    while (iKeyLen >= 4) {
        unsigned int k = *(unsigned int *)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        iKeyLen -= 4;
    }

    /* Handle the last few bytes of the input array */

    switch(iKeyLen) {
    case 3:
        h ^= ((uint32)data[2]) << 16;

    case 2:
        h ^= ((uint32)data[1]) << 8;

    case 1:
        h ^= data[0];
        h *= m;

    default:
        break;
    };

    /*
     * Do a few final mixes of the hash to ensure the last few bytes are
     * well-incorporated.
     */

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

///////////////////////////////////////////////Md5 begin////////
typedef unsigned int MD5_u32plus;

typedef struct {
    MD5_u32plus lo, hi;
    MD5_u32plus a, b, c, d;
    unsigned char buffer[64];
    MD5_u32plus block[16];
} MD5_CTX;


/*
 * The basic MD5 functions.
 *
 * F and G are optimized compared to their RFC 1321 definitions for
 * architectures that lack an AND-NOT instruction, just like in Colin Plumb's
 * implementation.
 */
#define F(x, y, z)			((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)			((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)			((x) ^ (y) ^ (z))
#define I(x, y, z)			((y) ^ ((x) | ~(z)))

/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s)                            \
	(a) += f((b), (c), (d)) + (x) + (t);                        \
	(a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s))));  \
	(a) += (b);

/*
 * SET reads 4 input bytes in little-endian byte order and stores them
 * in a properly aligned word in host byte order.
 *
 * The check for little-endian architectures that tolerate unaligned
 * memory accesses is just an optimization.  Nothing will break if it
 * doesn't work.
 */
#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define SET(n) \
    (*(MD5_u32plus *)&ptr[(n) * 4])
#define GET(n) \
    SET(n)
#else
#define SET(n)                                  \
    (ctx->block[(n)] =                          \
    (MD5_u32plus)ptr[(n) * 4] |                 \
    ((MD5_u32plus)ptr[(n) * 4 + 1] << 8) |      \
    ((MD5_u32plus)ptr[(n) * 4 + 2] << 16) |     \
    ((MD5_u32plus)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
    (ctx->block[(n)])
#endif


static void *
body(MD5_CTX *ctx, void *data, unsigned long size)
{
    unsigned char *ptr;
    MD5_u32plus a, b, c, d;
    MD5_u32plus saved_a, saved_b, saved_c, saved_d;

    ptr = (unsigned char*)data;

    a = ctx->a;
    b = ctx->b;
    c = ctx->c;
    d = ctx->d;

	do {
        saved_a = a;
        saved_b = b;
        saved_c = c;
        saved_d = d;

        /* Round 1 */
        STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
        STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
        STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
        STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
        STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
        STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
        STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
        STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
        STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
        STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
        STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
        STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
        STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
        STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
        STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
        STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)

        /* Round 2 */
        STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
        STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
        STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
        STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
        STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
        STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
        STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
        STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
        STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
        STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
        STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
        STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
        STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
        STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
        STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
        STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)

        /* Round 3 */
        STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
        STEP(H, d, a, b, c, GET(8), 0x8771f681, 11)
        STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
        STEP(H, b, c, d, a, GET(14), 0xfde5380c, 23)
        STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
        STEP(H, d, a, b, c, GET(4), 0x4bdecfa9, 11)
        STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
        STEP(H, b, c, d, a, GET(10), 0xbebfbc70, 23)
        STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
        STEP(H, d, a, b, c, GET(0), 0xeaa127fa, 11)
        STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
        STEP(H, b, c, d, a, GET(6), 0x04881d05, 23)
        STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
        STEP(H, d, a, b, c, GET(12), 0xe6db99e5, 11)
        STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
        STEP(H, b, c, d, a, GET(2), 0xc4ac5665, 23)

        /* Round 4 */
        STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
        STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
        STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
        STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
        STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
        STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
        STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
        STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
        STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
        STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
        STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
        STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
        STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
        STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
        STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
        STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

        a += saved_a;
        b += saved_b;
        c += saved_c;
        d += saved_d;

        ptr += 64;
    } while (size -= 64);

    ctx->a = a;
    ctx->b = b;
    ctx->c = c;
    ctx->d = d;

    return ptr;
}

void
MD5_Init(MD5_CTX *ctx)
{
    ctx->a = 0x67452301;
    ctx->b = 0xefcdab89;
    ctx->c = 0x98badcfe;
    ctx->d = 0x10325476;

    ctx->lo = 0;
    ctx->hi = 0;
}

void
MD5_Update(MD5_CTX *ctx, void *data, unsigned long size)
{
    MD5_u32plus saved_lo;
    unsigned long used, free;

    saved_lo = ctx->lo;
    if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo) {
        ctx->hi++;
    }
    ctx->hi += size >> 29;

    used = saved_lo & 0x3f;

    if (used) {
        free = 64 - used;

        if (size < free) {
            memcpy(&ctx->buffer[used], data, size);
            return;
        }

        memcpy(&ctx->buffer[used], data, free);
        data = (unsigned char *)data + free;
        size -= free;
        body(ctx, ctx->buffer, 64);
    }

    if (size >= 64) {
        data = body(ctx, data, size & ~(unsigned long)0x3f);
        size &= 0x3f;
    }

    memcpy(ctx->buffer, data, size);
}

void
MD5_Final(unsigned char *result, MD5_CTX *ctx)
{
    unsigned long used, free;

    used = ctx->lo & 0x3f;

    ctx->buffer[used++] = 0x80;

    free = 64 - used;

    if (free < 8) {
        memset(&ctx->buffer[used], 0, free);
        body(ctx, ctx->buffer, 64);
        used = 0;
        free = 64;
    }

    memset(&ctx->buffer[used], 0, free - 8);

    ctx->lo <<= 3;
    ctx->buffer[56] = ctx->lo;
    ctx->buffer[57] = ctx->lo >> 8;
    ctx->buffer[58] = ctx->lo >> 16;
    ctx->buffer[59] = ctx->lo >> 24;
    ctx->buffer[60] = ctx->hi;
    ctx->buffer[61] = ctx->hi >> 8;
    ctx->buffer[62] = ctx->hi >> 16;
    ctx->buffer[63] = ctx->hi >> 24;

    body(ctx, ctx->buffer, 64);

    result[0] = ctx->a;
    result[1] = ctx->a >> 8;
    result[2] = ctx->a >> 16;
    result[3] = ctx->a >> 24;
    result[4] = ctx->b;
    result[5] = ctx->b >> 8;
    result[6] = ctx->b >> 16;
    result[7] = ctx->b >> 24;
    result[8] = ctx->c;
    result[9] = ctx->c >> 8;
    result[10] = ctx->c >> 16;
    result[11] = ctx->c >> 24;
    result[12] = ctx->d;
    result[13] = ctx->d >> 8;
    result[14] = ctx->d >> 16;
    result[15] = ctx->d >> 24;

    memset(ctx, 0, sizeof(*ctx));
}

uint32 GR_Hash::Md5(const char *szKey, size_t iKeyLen)
{
    
    unsigned char results[16];
    GR_Hash::Md5Signature((unsigned char*)szKey, (unsigned long)iKeyLen, results);

    return ((uint32) (results[3] & 0xFF) << 24) |
           ((uint32) (results[2] & 0xFF) << 16) |
           ((uint32) (results[1] & 0xFF) << 8) |
           (results[0] & 0xFF);
}

void GR_Hash::Md5Signature(unsigned char *key, unsigned long length, unsigned char *result)
{
    MD5_CTX my_md5;

    MD5_Init(&my_md5);
    (void)MD5_Update(&my_md5, key, length);
    MD5_Final(result, &my_md5);
}
///////////////////////////////////////////////Md5 end////////

///////////////////////////////////////////////Fnv begin////////


static uint64 FNV_64_INIT = UINT64_C(0xcbf29ce484222325);
static uint64 FNV_64_PRIME = UINT64_C(0x100000001b3);
static uint32 FNV_32_INIT = 2166136261UL;
static uint32 FNV_32_PRIME = 16777619;

uint32
GR_Hash::Fnv164(const char *key, size_t key_length)
{
    uint64 hash = FNV_64_INIT;
    size_t x;

    for (x = 0; x < key_length; x++) {
      hash *= FNV_64_PRIME;
      hash ^= (uint64)key[x];
    }

    return (uint32)hash;
}

uint32
GR_Hash::Fnv1a64(const char *key, size_t key_length)
{
    uint32 hash = (uint32) FNV_64_INIT;
    size_t x;

    for (x = 0; x < key_length; x++) {
      uint32 val = (uint32)key[x];
      hash ^= val;
      hash *= (uint32) FNV_64_PRIME;
    }

    return hash;
}

uint32
GR_Hash::Fnv132(const char *key, size_t key_length)
{
    uint32 hash = FNV_32_INIT;
    size_t x;

    for (x = 0; x < key_length; x++) {
      uint32 val = (uint32)key[x];
      hash *= FNV_32_PRIME;
      hash ^= val;
    }

    return hash;
}

uint32
GR_Hash::Fnv1a32(const char *key, size_t key_length)
{
    uint32 hash = FNV_32_INIT;
    size_t x;

    for (x= 0; x < key_length; x++) {
      uint32 val = (uint32)key[x];
      hash ^= val;
      hash *= FNV_32_PRIME;
    }

    return hash;
}
///////////////////////////////////////////////Fnv end//////////

////////////////////////////////////////////////Hsieh begin/////////
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__))
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                      +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32 GR_Hash::Hsieh(const char *szKey, size_t iKeyLen)
{
    uint32_t hash = 0, tmp;
    int rem;

    if (iKeyLen <= 0 || szKey == NULL) {
        return 0;
    }

    rem = iKeyLen & 3;
    iKeyLen >>= 2;

    /* Main loop */
    for (;iKeyLen > 0; iKeyLen--) {
        hash += get16bits (szKey);
        tmp = (get16bits (szKey+2) << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        szKey += 2*sizeof (uint16);
        hash += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
    case 3:
        hash += get16bits (szKey);
        hash ^= hash << 16;
        hash ^= (uint32)szKey[sizeof (uint16)] << 18;
        hash += hash >> 11;
        break;

    case 2:
        hash += get16bits (szKey);
        hash ^= hash << 11;
        hash += hash >> 17;
        break;

    case 1:
        hash += (unsigned char)(*szKey);
        hash ^= hash << 10;
        hash += hash >> 1;

    default:
        break;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

//////////////////////////////////////////////Hsieh end////////////

//////////////////////////////////////////////Jenkins begin//////////
#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

#define JENKINS_INITVAL 13


uint32 GR_Hash::Jenkins(const char *szKey, size_t iKeyLen)
{  
    uint32 a,b,c;                                          /* internal state */
    union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)iKeyLen) + JENKINS_INITVAL;

  u.ptr = szKey;
#ifndef WORDS_BIGENDIAN
  if ((u.i & 0x3) == 0)
  {
    const uint32 *k = (const uint32 *)szKey;         /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (iKeyLen > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      iKeyLen -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticeably faster for short strings (like English words).
     */
    switch(iKeyLen)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;              /* zero length strings require no mixing */
    default: return c;
    }

  }
  else if ((u.i & 0x1) == 0)
  {
    const uint16 *k = (const uint16 *)szKey;         /* read 16-bit chunks */
    const uint8  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (iKeyLen > 12)
    {
      a += k[0] + (((uint32)k[1])<<16);
      b += k[2] + (((uint32)k[3])<<16);
      c += k[4] + (((uint32)k[5])<<16);
      mix(a,b,c);
      iKeyLen -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8 *)k;
    switch(iKeyLen)
    {
    case 12: c+=k[4]+(((uint32)k[5])<<16);
             b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 11: c+=((uint32)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 7 : b+=((uint32)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 3 : a+=((uint32)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    default: return c;
    }

  }
  else
  {                        /* need to read the key one byte at a time */
#endif /* little endian */
    const uint8 *k = (const uint8 *)szKey;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (iKeyLen > 12)
    {
      a += k[0];
      a += ((uint32)k[1])<<8;
      a += ((uint32)k[2])<<16;
      a += ((uint32)k[3])<<24;
      b += k[4];
      b += ((uint32)k[5])<<8;
      b += ((uint32)k[6])<<16;
      b += ((uint32)k[7])<<24;
      c += k[8];
      c += ((uint32)k[9])<<8;
      c += ((uint32)k[10])<<16;
      c += ((uint32)k[11])<<24;
      mix(a,b,c);
      iKeyLen -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(iKeyLen)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32)k[11])<<24;
    case 11: c+=((uint32)k[10])<<16;
    case 10: c+=((uint32)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((uint32)k[7])<<24;
    case 7 : b+=((uint32)k[6])<<16;
    case 6 : b+=((uint32)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((uint32)k[3])<<24;
    case 3 : a+=((uint32)k[2])<<16;
    case 2 : a+=((uint32)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    default : return c;
    }
#ifndef WORDS_BIGENDIAN
  }
#endif

  final(a,b,c);
  return c;
}
//////////////////////////////////////////////Jenkins end////////////


