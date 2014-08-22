#include <CoreFoundation/CoreFoundation.h>

#if defined __cplusplus
    extern "C" {
#endif

    CF_RETURNS_RETAINED CGImageRef grabViaOpenGL(CGDirectDisplayID display,
                             CGRect srcRect);

#if defined __cplusplus
    }
#endif

