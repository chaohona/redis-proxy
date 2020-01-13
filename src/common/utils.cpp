#include "utils.h"
#include "define.h"
#include <iostream>
using namespace std;

# define GR_HAVE_BACKTRACE 1

#ifdef GR_HAVE_BACKTRACE
# include <execinfo.h>
#endif

pid_t gr_work_pid = 0;
int gr_process = 0;

string trim(string &strIn, char *szSet)
{
    return strIn.erase(strIn.find_last_not_of(szSet)+1);
}

vector<string> split(const string& str, const string& delim) {
	vector<string> res;
	if("" == str) return res;
	//先将要切割的字符串从string类型转换为char*类型
	char * strs = new char[str.length() + 1] ; //不要忘了
	strcpy(strs, str.c_str()); 
 
	char * d = new char[delim.length() + 1];
	strcpy(d, delim.c_str());
 
	char *p = strtok(strs, d);
	while(p) {
		string s = p; //分割得到的字符串转换为string类型
		res.push_back(s); //存入结果数组
		p = strtok(NULL, d);
	}
 
	return res;
}

vector<string> split(char *szSrc, int iSrcLen, char *szDelim, int iDelimLen)
{
    vector<string> res;
	if(iSrcLen==0) return res;

	//先将要切割的字符串从string类型转换为char*类型
	char * strs = new char[iSrcLen + 1] ; //不要忘了
	strs[iSrcLen] = '\n';
	strncpy(strs, szSrc, iSrcLen);
 
	char * d = new char[iDelimLen + 1];
	d[iDelimLen] = '\n';
	strncpy(d, szDelim, iDelimLen);
 
	char *p = strtok(strs, d);
	while(p) {
		string s = p; //分割得到的字符串转换为string类型
		res.push_back(s); //存入结果数组
		p = strtok(NULL, d);
	}
 
	return res;
}

vector<GR_String> GR_SplitLine(char *szSrc, int iSrcLen, char dot)
{
    vector<GR_String> res;
    if (iSrcLen==0) return res;

    GR_String tmpRet;
    tmpRet.szChar = szSrc;
    for (int i=0; i<iSrcLen; i++)
    {
        if (szSrc[i] == dot)
        {
            res.push_back(tmpRet);
            if (i!=iSrcLen-1)
            {
                tmpRet.szChar = szSrc+i+1;
                tmpRet.iLen = 0;
            }
        }
        else
        {
            tmpRet.iLen += 1;
        }
    }
    if (tmpRet.iLen != 0)
    {
        res.push_back(tmpRet);
    }

    return res;
}

int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int hex_digit_to_int(char c) {
    switch(c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;
    }
}

int CharToInt(char *szData, int iLen, int &iRet)
{
    if (iLen <= 0)
    {
        iRet = GR_ERROR;
        return 0;
    }
    int symbol = 1;
    int idx = 0;
    if (szData[0] == '-')
    {
        idx = 1;
        symbol = -1;
    }
    int iResult = 0;
    for(; idx<iLen; idx++)
    {
        iResult = iResult*10 + szData[idx]-'0';
    }

    return iResult * symbol;
}

int AddrToLong(char *szIP, uint16 usPort, uint64 &ulResult)
{
    ulResult = 0;
    struct in_addr addr;
    if (!inet_aton(szIP, &addr))
    {
        return GR_ERROR;
    }
    ulResult = long(addr.s_addr)<<32 | usPort;
    return GR_OK;
}

int AddrToLong(char *szAddr, uint64 &ulResult)
{
    try
    {
        char *szResult = strchr(szAddr, ':');
        if (szResult == nullptr)
        {
            return GR_ERROR;
        }
        *szResult = '\n';
        ulResult = 0;
        struct in_addr addr;
        if (!inet_aton(szAddr, &addr))
        {
            return GR_ERROR;
        }
        int iPort = atoi(szResult++);
        if (iPort == 0)
        {
            return GR_ERROR;
        }
        ulResult = long(addr.s_addr)<<32 | iPort;
        return GR_OK;
    }
    catch(exception &e)
    {
        cout << "AddrToLong got an exception:" << e.what() << endl;
        return GR_ERROR;
    }

}

int
_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int n;

    if (size > 1024)
    {
        size = 1024;
    }
    n = vsnprintf(buf, size, fmt, args);

    /*
     * The return value is the number of characters which would be written
     * into buf not including the trailing '\0'. If size is == 0 the
     * function returns 0.
     *
     * On error, the function also returns 0. This is to allow idiom such
     * as len += _vscnprintf(...)
     *
     * See: http://lwn.net/Articles/69419/
     */
    if (n <= 0) {
        return 0;
    }

    if (n < (int) size) {
        return n;
    }

    return (int)(size - 1);
}


void aeGetTime(long *seconds, long *milliseconds)
{
    timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec/1000;
}

//
uint64  GR_GetNowMS()
{
    timeval tv;

    gettimeofday(&tv, NULL); // linux系统实现是从共享内存中获取时间，不涉及到系统调用，速度满足要求
    return tv.tv_sec * 1000 + tv.tv_usec/1000;
}

void
GR_Stacktrace(int skip_count)
{
#ifdef GR_HAVE_BACKTRACE
    void *stack[64];
    char **symbols;
    int size, i, j;

    size = backtrace(stack, 64);
    symbols = backtrace_symbols(stack, size);
    if (symbols == NULL) {
        return;
    }

    skip_count++; /* skip the current frame also */

    for (i = skip_count, j = 0; i < size; i++, j++) {
        //loga("[%d] %s", j, symbols[i]);
        cout <<"["<< j <<"]" <<  symbols[i] << endl;
    }

    free(symbols);
#endif
}

void GR_LogStacktrace(void)
{

}

void GR_Assert(const char *cond, const char *file, int line, int panic)
{
    if (panic) {
        GR_Stacktrace(1);
        abort();
    }
    return;
}

int ParseAddr(string &strListenAddr, string &strIP, uint16 &usPort)
{
    auto results = split(strListenAddr, ":");
    if (results.size() != 2)
    {
        return GR_ERROR;
    }
    strIP = results[0];
    usPort = stoi(results[1], 0, 10);
    
    return GR_OK;
}

/* Return the number of digits of 'v' when converted to string in radix 10.
 * See ll2string() for more information. */
uint32_t digits10(uint64_t v) {
    if (v < 10) return 1;
    if (v < 100) return 2;
    if (v < 1000) return 3;
    if (v < 1000000000000UL) {
        if (v < 100000000UL) {
            if (v < 1000000) {
                if (v < 10000) return 4;
                return 5 + (v >= 100000);
            }
            return 7 + (v >= 10000000UL);
        }
        if (v < 10000000000UL) {
            return 9 + (v >= 1000000000UL);
        }
        return 11 + (v >= 100000000000UL);
    }
    return 12 + digits10(v / 1000000000000UL);
}

/* Convert a long long into a string. Returns the number of
 * characters needed to represent the number.
 * If the buffer is not big enough to store the string, 0 is returned.
 *
 * Based on the following article (that apparently does not provide a
 * novel approach but only publicizes an already used technique):
 *
 * https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920
 *
 * Modified in order to handle signed integers since the original code was
 * designed for unsigned integers. */
int ll2string(char *dst, size_t dstlen, long long svalue) {
    static const char digits[201] =
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";
    int negative;
    unsigned long long value;

    /* The main loop works with 64bit unsigned integers for simplicity, so
     * we convert the number here and remember if it is negative. */
    if (svalue < 0) {
        if (svalue != LLONG_MIN) {
            value = -svalue;
        } else {
            value = ((unsigned long long) LLONG_MAX)+1;
        }
        negative = 1;
    } else {
        value = svalue;
        negative = 0;
    }

    /* Check length. */
    uint32_t const length = digits10(value)+negative;
    if (length >= dstlen) return 0;

    /* Null term. */
    uint32_t next = length;
    dst[next] = '\0';
    next--;
    while (value >= 100) {
        int const i = (value % 100) * 2;
        value /= 100;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
        next -= 2;
    }

    /* Handle last 1-2 digits. */
    if (value < 10) {
        dst[next] = '0' + (uint32_t) value;
    } else {
        int i = (uint32_t) value * 2;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
    }

    /* Add sign. */
    if (negative) dst[0] = '-';
    return length;
}

#define SDS_LLSTR_SIZE 21
int sdsll2str(char *s, long long value) {
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    v = (value < 0) ? -value : value;
    p = s;
    do {
        *p++ = '0'+(v%10);
        v /= 10;
    } while(v);
    if (value < 0) *p++ = '-';

    /* Compute length and add null term. */
    l = p-s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}


