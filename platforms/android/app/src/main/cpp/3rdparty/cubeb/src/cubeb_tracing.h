/*
 * Copyright © 2022 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_TRACING_H
#define CUBEB_TRACING_H

/* Empty header to allow hooking up a frame profiler. */

// To be called once on a thread to register for tracing.
#define CUBEB_REGISTER_THREAD(name)
// To be called once before a registered threads exits.
#define CUBEB_UNREGISTER_THREAD()
// Insert a tracing marker, with a particular name.
// Phase can be 'x': instant marker, start time but no duration
//              'b': beginning of a marker with a duration
//              'e': end of a marker with a duration
#define CUBEB_TRACE(name, phase)

#endif // CUBEB_TRACING_H
