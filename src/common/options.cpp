#include "options.h"

GR_Options *GR_Options::m_pInstance = new GR_Options();

GR_Options* GR_Options::Instance()
{
	return m_pInstance;
}

GR_Options::GR_Options()
{
    this->daemonize = 0;
}

