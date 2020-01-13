#ifndef _GR_UTILS_H__
#define _GR_UTILS_H__

#include <iostream>
#include <vector>
#include <string>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "define.h"
#include "gr_string.h"


using namespace std;

//////////////////////////////////字符串比较函数//////////////////////////////////
#define str3icmp(m, c0, c1, c2)                                                             \
    ((m[0] == c0 || m[0] == (c0 ^ 0x20)) &&                                                 \
     (m[1] == c1 || m[1] == (c1 ^ 0x20)) &&                                                 \
     (m[2] == c2 || m[2] == (c2 ^ 0x20)))

#define str4icmp(m, c0, c1, c2, c3)                                                         \
    (str3icmp(m, c0, c1, c2) && (m[3] == c3 || m[3] == (c3 ^ 0x20)))

#define str5icmp(m, c0, c1, c2, c3, c4)                                                     \
    (str4icmp(m, c0, c1, c2, c3) && (m[4] == c4 || m[4] == (c4 ^ 0x20)))

#define str6icmp(m, c0, c1, c2, c3, c4, c5)                                                 \
    (str5icmp(m, c0, c1, c2, c3, c4) && (m[5] == c5 || m[5] == (c5 ^ 0x20)))

#define str7icmp(m, c0, c1, c2, c3, c4, c5, c6)                                             \
    (str6icmp(m, c0, c1, c2, c3, c4, c5) &&                                                 \
     (m[6] == c6 || m[6] == (c6 ^ 0x20)))

#define str8icmp(m, c0, c1, c2, c3, c4, c5, c6, c7)                                         \
    (str7icmp(m, c0, c1, c2, c3, c4, c5, c6) &&                                             \
     (m[7] == c7 || m[7] == (c7 ^ 0x20)))

#define str9icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8)                                     \
    (str8icmp(m, c0, c1, c2, c3, c4, c5, c6, c7) &&                                         \
     (m[8] == c8 || m[8] == (c8 ^ 0x20)))

#define str10icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9)                                \
    (str9icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8) &&                                     \
     (m[9] == c9 || m[9] == (c9 ^ 0x20)))

#define str11icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10)                           \
    (str10icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9) &&                                \
     (m[10] == c10 || m[10] == (c10 ^ 0x20)))

#define str12icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)                      \
    (str11icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10) &&                           \
     (m[11] == c11 || m[11] == (c11 ^ 0x20)))

#define str13icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12)                 \
    (str12icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11) &&                      \
     (m[12] == c12 || m[12] == (c12 ^ 0x20)))

#define str14icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13)            \
    (str13icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12) &&                 \
     (m[13] == c13 || m[13] == (c13 ^ 0x20)))

#define str15icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14)       \
    (str14icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13) &&            \
     (m[14] == c14 || m[14] == (c14 ^ 0x20)))

#define str16icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15)  \
    (str15icmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14) &&       \
     (m[15] == c15 || m[15] == (c15 ^ 0x20)))
//////////////////////////////////字符串比较函数//////////////////////////////////


/*
 * Wrappers to read or write data to/from (multiple) buffers
 * to a file or socket descriptor.
 */
#define GR_Read(_d, _b, _n)     \
    read(_d, _b, (size_t)(_n))

#define GR_Readv(_d, _b, _n)    \
    readv(_d, _b, (int)(_n))

#define GR_Write(_d, _b, _n)    \
    write(_d, _b, (size_t)(_n))

#define GR_Rritev(_d, _b, _n)   \
    writev(_d, _b, (int)(_n))


vector<string> split(const string& str, const string& delim);
vector<string> split(char *szSrc, int iSrcLen, char *szDelim, int iDelimLen);
vector<GR_String> GR_SplitLine(char *szSrc, int iSrcLen, char dot);

string trim(string &strIn, char *szSet);

/* Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
int is_hex_digit(char c);

/* Helper function for sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c);
int CharToInt(char *szData, int iLen, int &iRet);
int AddrToLong(char *szIP, uint16 usPort, uint64 &ulResult);
int AddrToLong(char *szAddr, uint64 &ulResult);

int
_vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

#define GR_VscnPrintf(_s, _n, _f, _a)   \
    _vscnprintf((char *)(_s), (size_t)(_n), _f, _a)



//////////////////////////////时间操作函数///////////////
void aeGetTime(long *seconds, long *milliseconds);
// 当前毫秒时间
uint64  GR_GetNowMS();

void GR_Stacktrace(int skip_count);
void GR_LogStacktrace(void);


void GR_Assert(const char *cond, const char *file, int line, int panic);

#define ASSERT(_x)do{\
    if (!(_x)){\
        GR_Assert(#_x, __FILE__, __LINE__, 1);\
    }\
}while(0)

#define NOT_REACHED() ASSERT(0)

#define USE_SETPROCTITLE
#define INIT_SETPROCTITLE_REPLACEMENT
void spt_init(int argc, char *argv[]);
void setproctitle(const char *fmt, ...);

// 传入ip:port得到ip和port
int ParseAddr(string &strListenAddr, string &strIP, uint16 &usPort);

extern pid_t gr_work_pid;
extern int gr_process;

#define GR_VALUE_HELPER(n)   #n
#define GR_VALUE(n)          GR_VALUE_HELPER(n)

#define GR_SIGNAL_VALUE_HELPER(n)     SIG##n
#define GR_SIGNAL_VALUE(n)      GR_SIGNAL_VALUE_HELPER(n)

// 获取CPU核心数
#define GR_CPU_CORE_NUM get_nprocs()

#define GR_BYTE_TO_UINT32(bytes,num)\
num = uint32(*(bytes))<<24 | uint32(*(bytes+1))<<16 | uint32(*(bytes+2))<<8 | uint32(*(bytes+3))

#define GR_BYTE_TO_UINT64(bytes,num)\
num = uint64(*(bytes))<<56 | uint64(*(bytes+1))<<48 | uint64(*(bytes+2))<<40 | uint64(*(bytes+3))<<32 |\
uint64(*(bytes+4))<<24 | uint64(*(bytes+5))<<16 | uint64(*(bytes+6))<<8 | uint64(*(bytes+7))

int ll2string(char *dst, size_t dstlen, long long svalue);
uint32_t digits10(uint64_t v);
int sdsll2str(char *s, long long value);

#endif
