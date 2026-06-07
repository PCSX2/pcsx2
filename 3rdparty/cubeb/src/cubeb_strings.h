/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_STRINGS_H
#define CUBEB_STRINGS_H

#include "cubeb/cubeb.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** Opaque handle referencing interned string storage. */
typedef struct cubeb_strings cubeb_strings;

/** Initialize an interned string structure.
    @param strings An out param where an opaque pointer to the
    interned string storage will be returned.
    @retval CUBEB_OK in case of success.
    @retval CUBEB_ERROR in case of error. */
CUBEB_EXPORT int
cubeb_strings_init(cubeb_strings ** strings);

/** Destroy an interned string structure freeing all associated memory.
    @param strings An opaque pointer to the interned string storage to
                   destroy. */
CUBEB_EXPORT void
cubeb_strings_destroy(cubeb_strings * strings);

/** Add string to internal storage.
    @param strings Opaque pointer to interned string storage.
    @param s String to add to storage.
    @retval CUBEB_OK
    @retval CUBEB_ERROR
 */
CUBEB_EXPORT char const *
cubeb_strings_intern(cubeb_strings * strings, char const * s);

#if defined(__cplusplus)
}
#endif

#endif // !CUBEB_STRINGS_H
