#pragma once

namespace ios_metal
{
    // Whether the GPU can sample BC1-BC7 textures natively. Most iOS devices can't.
    bool SupportsBCTextureCompression();
}
