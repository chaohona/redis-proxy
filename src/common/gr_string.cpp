#include "gr_string.h"

GR_String::GR_String(char *szChar, int iLen)
{
    this->szChar = szChar;
    this->iLen = iLen;
}

GR_String::GR_String(char *szChar)
{
    this->szChar = szChar;
    this->iLen = strlen(szChar);
}

GR_String::GR_String()
{
}

