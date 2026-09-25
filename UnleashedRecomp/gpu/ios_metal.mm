#include "ios_metal.h"

#import <Metal/Metal.h>

namespace ios_metal
{
    bool SupportsBCTextureCompression()
    {
        @autoreleasepool
        {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            if (device == nil)
                return false;

            bool supported = false;

#if defined(__IPHONE_16_4) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_4)
            if (@available(iOS 16.4, *))
                supported = [device supportsBCTextureCompression];
#endif

#if !__has_feature(objc_arc)
            [device release];
#endif

            return supported;
        }
    }
}
