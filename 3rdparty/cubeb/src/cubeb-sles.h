/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef _CUBEB_SLES_H_
#define _CUBEB_SLES_H_
#include <SLES/OpenSLES.h>

static SLresult
cubeb_get_sles_engine(SLObjectItf * pEngine, SLuint32 numOptions,
                      const SLEngineOption * pEngineOptions,
                      SLuint32 numInterfaces,
                      const SLInterfaceID * pInterfaceIds,
                      const SLboolean * pInterfaceRequired)
{
  return slCreateEngine(pEngine, numOptions, pEngineOptions, numInterfaces,
                        pInterfaceIds, pInterfaceRequired);
}

static void
cubeb_destroy_sles_engine(SLObjectItf * self)
{
  if (*self != NULL) {
    (**self)->Destroy(*self);
    *self = NULL;
  }
}

static SLresult
cubeb_realize_sles_engine(SLObjectItf self)
{
  return (*self)->Realize(self, SL_BOOLEAN_FALSE);
}

#endif
