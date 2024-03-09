#include "rc_version.h"

uint32_t rc_version(void)
{
  return RCHEEVOS_VERSION;
}

const char* rc_version_string(void)
{
  return RCHEEVOS_VERSION_STRING;
}
