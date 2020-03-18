#include <iostream>

#include "store.h"

using namespace std;

GR_Store *GR_Store::m_pInstance = new GR_Store;

GR_Store::~GR_Store()
{
}

GR_Store::GR_Store()
{
}

GR_Store* GR_Store::Instance()
{
    return m_pInstance;
}

int GR_Store::Init()
{
    
    return GR_OK;
}

int main()
{
    cout << "test makefile" << endl;

    return 0;
}
