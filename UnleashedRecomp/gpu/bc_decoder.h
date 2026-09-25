#pragma once

#include <cstddef>
#include <cstdint>

// Software decoders for block compressed (BC1-BC7) textures. These are used on
// devices whose GPU cannot sample BC formats natively (e.g. most iOS devices).
namespace bc
{
    enum class Format
    {
        BC1,
        BC2,
        BC3,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UF16,
        BC6H_SF16,
        BC7
    };

    // Size of a compressed 4x4 block in bytes.
    uint32_t GetBlockSize(Format format);

    // Size of one decoded texel in bytes:
    // - BC1, BC2, BC3 and BC7 decode to RGBA8.
    // - BC4 decodes to R8 (two's complement for SNORM).
    // - BC5 decodes to RG8 (two's complement for SNORM).
    // - BC6H decodes to RGBA16F.
    uint32_t GetDecodedTexelSize(Format format);

    // Size in bytes of the compressed data for a width x height surface.
    size_t GetCompressedSurfaceSize(Format format, uint32_t width, uint32_t height);

    // Decodes a single 4x4 block into 16 texels stored in row-major order.
    void DecodeBlock(Format format, const uint8_t* block, void* texels);

    // Decodes a width x height surface. The source must contain GetCompressedSurfaceSize(format, width, height)
    // bytes and each destination row must be at least width * GetDecodedTexelSize(format) bytes long.
    void DecodeSurface(Format format, const uint8_t* src, uint32_t width, uint32_t height, uint8_t* dst, size_t dstRowPitch);
}
