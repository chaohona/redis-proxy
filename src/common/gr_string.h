#ifndef _GR_STRING_H__
#define _GR_STRING_H__
#include "define.h"

class GR_String
{
public:
    GR_String(char *szChar, int iLen);
    GR_String(char *szChar);
    GR_String();

    char *szChar = nullptr;
    int iLen = 0;
};

#endif
