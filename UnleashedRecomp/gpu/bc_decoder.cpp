#include "bc_decoder.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
    // Reads the bits of a 128-bit block, least significant bit first.
    struct BitReader
    {
        uint64_t low = 0;
        uint64_t high = 0;

        explicit BitReader(const uint8_t* block)
        {
            for (uint32_t i = 0; i < 8; i++)
            {
                low |= uint64_t(block[i]) << (i * 8);
                high |= uint64_t(block[i + 8]) << (i * 8);
            }
        }

        uint32_t Read(uint32_t count)
        {
            if (count == 0)
                return 0;

            uint32_t value = uint32_t(low & ((uint64_t(1) << count) - 1));
            low = (low >> count) | (high << (64 - count));
            high >>= count;
            return value;
        }

        // Reads a field whose bits are stored from the most significant to the least significant one.
        uint32_t ReadReversed(uint32_t count)
        {
            uint32_t value = 0;
            for (uint32_t i = 0; i < count; i++)
                value |= Read(1) << (count - 1 - i);

            return value;
        }
    };

    // BC1-BC5

    void Expand565(uint32_t color, uint8_t* rgb)
    {
        uint32_t r = (color >> 11) & 0x1F;
        uint32_t g = (color >> 5) & 0x3F;
        uint32_t b = color & 0x1F;

        rgb[0] = uint8_t((r << 3) | (r >> 2));
        rgb[1] = uint8_t((g << 2) | (g >> 4));
        rgb[2] = uint8_t((b << 3) | (b >> 2));
    }

    // BC2 and BC3 always decode their color block in four color mode.
    void DecodeColorBlock(const uint8_t* block, uint8_t* rgba, bool allowThreeColorMode)
    {
        uint32_t c0 = block[0] | (uint32_t(block[1]) << 8);
        uint32_t c1 = block[2] | (uint32_t(block[3]) << 8);

        uint8_t colors[4][4];
        Expand565(c0, colors[0]);
        Expand565(c1, colors[1]);
        colors[0][3] = 255;
        colors[1][3] = 255;

        if (c0 > c1 || !allowThreeColorMode)
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                colors[2][i] = uint8_t((2 * colors[0][i] + colors[1][i] + 1) / 3);
                colors[3][i] = uint8_t((colors[0][i] + 2 * colors[1][i] + 1) / 3);
            }

            colors[2][3] = 255;
            colors[3][3] = 255;
        }
        else
        {
            for (uint32_t i = 0; i < 3; i++)
                colors[2][i] = uint8_t((colors[0][i] + colors[1][i] + 1) / 2);

            colors[2][3] = 255;
            memset(colors[3], 0, sizeof(colors[3]));
        }

        uint32_t indices = block[4] | (uint32_t(block[5]) << 8) | (uint32_t(block[6]) << 16) | (uint32_t(block[7]) << 24);

        for (uint32_t i = 0; i < 16; i++)
            memcpy(rgba + i * 4, colors[(indices >> (i * 2)) & 0x3], 4);
    }

    uint64_t ReadChannelIndices(const uint8_t* block)
    {
        uint64_t indices = 0;
        for (uint32_t i = 0; i < 6; i++)
            indices |= uint64_t(block[2 + i]) << (i * 8);

        return indices;
    }

    void DecodeUnsignedChannel(const uint8_t* block, uint8_t* out, uint32_t stride)
    {
        uint32_t v0 = block[0];
        uint32_t v1 = block[1];

        uint8_t values[8];
        values[0] = uint8_t(v0);
        values[1] = uint8_t(v1);

        if (v0 > v1)
        {
            for (uint32_t i = 1; i < 7; i++)
                values[i + 1] = uint8_t(((7 - i) * v0 + i * v1 + 3) / 7);
        }
        else
        {
            for (uint32_t i = 1; i < 5; i++)
                values[i + 1] = uint8_t(((5 - i) * v0 + i * v1 + 2) / 5);

            values[6] = 0;
            values[7] = 255;
        }

        uint64_t indices = ReadChannelIndices(block);

        for (uint32_t i = 0; i < 16; i++)
            out[i * stride] = values[(indices >> (i * 3)) & 0x7];
    }

    int32_t DivideRounded(int32_t numerator, int32_t denominator)
    {
        return (numerator >= 0) ? (numerator + denominator / 2) / denominator : (numerator - denominator / 2) / denominator;
    }

    void DecodeSignedChannel(const uint8_t* block, uint8_t* out, uint32_t stride)
    {
        int32_t v0 = int8_t(block[0]);
        int32_t v1 = int8_t(block[1]);

        // -128 and -127 both represent -1.0.
        int32_t s0 = std::max(v0, -127);
        int32_t s1 = std::max(v1, -127);

        int32_t values[8];
        values[0] = s0;
        values[1] = s1;

        if (v0 > v1)
        {
            for (int32_t i = 1; i < 7; i++)
                values[i + 1] = DivideRounded((7 - i) * s0 + i * s1, 7);
        }
        else
        {
            for (int32_t i = 1; i < 5; i++)
                values[i + 1] = DivideRounded((5 - i) * s0 + i * s1, 5);

            values[6] = -127;
            values[7] = 127;
        }

        uint64_t indices = ReadChannelIndices(block);

        for (uint32_t i = 0; i < 16; i++)
            out[i * stride] = uint8_t(int8_t(values[(indices >> (i * 3)) & 0x7]));
    }

    void DecodeExplicitAlpha(const uint8_t* block, uint8_t* rgba)
    {
        for (uint32_t i = 0; i < 16; i++)
        {
            uint32_t alpha = (block[i / 2] >> ((i & 1) * 4)) & 0xF;
            rgba[i * 4 + 3] = uint8_t(alpha * 17);
        }
    }

    // Tables shared by BC6H and BC7.

    constexpr uint32_t WEIGHTS_2[] = { 0, 21, 43, 64 };
    constexpr uint32_t WEIGHTS_3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    constexpr uint32_t WEIGHTS_4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    uint32_t GetWeight(uint32_t indexBits, uint32_t index)
    {
        switch (indexBits)
        {
        case 2:
            return WEIGHTS_2[index];
        case 3:
            return WEIGHTS_3[index];
        default:
            return WEIGHTS_4[index];
        }
    }

    // Bit N is set when texel N belongs to the second subset.
    constexpr uint16_t PARTITIONS_2[64] =
    {
        0xCCCC, 0x8888, 0xEEEE, 0xECC8, 0xC880, 0xFEEC, 0xFEC8, 0xEC80,
        0xC800, 0xFFEC, 0xFE80, 0xE800, 0xFFE8, 0xFF00, 0xFFF0, 0xF000,
        0xF710, 0x008E, 0x7100, 0x08CE, 0x008C, 0x7310, 0x3100, 0x8CCE,
        0x088C, 0x3110, 0x6666, 0x366C, 0x17E8, 0x0FF0, 0x718E, 0x399C,
        0xAAAA, 0xF0F0, 0x5A5A, 0x33CC, 0x3C3C, 0x55AA, 0x9696, 0xA55A,
        0x73CE, 0x13C8, 0x324C, 0x3BDC, 0x6996, 0xC33C, 0x9966, 0x0660,
        0x0272, 0x04E4, 0x4E40, 0x2720, 0xC936, 0x936C, 0x39C6, 0x639C,
        0x9336, 0x9CC6, 0x817E, 0xE718, 0xCCF0, 0x0FCC, 0x7744, 0xEE22
    };

    constexpr uint8_t PARTITIONS_3[64][16] =
    {
        { 0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 1, 2, 2, 2, 2 }, { 0, 0, 0, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1 },
        { 0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1 }, { 0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2 }, { 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 2, 2 },
        { 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 }, { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2 }, { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2 },
        { 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2 }, { 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2 },
        { 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2, 1, 2, 2, 2 }, { 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0, 2, 2, 2, 0 },
        { 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2 }, { 0, 1, 1, 1, 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0 },
        { 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2 }, { 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1 },
        { 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2, 0, 2, 2, 2 }, { 0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 1, 2, 2, 2, 1 },
        { 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2 }, { 0, 0, 0, 0, 1, 1, 0, 0, 2, 2, 1, 0, 2, 2, 1, 0 },
        { 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0 }, { 0, 0, 1, 2, 0, 0, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2 },
        { 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1, 0, 1, 1, 0 }, { 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1 },
        { 0, 0, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 0, 0, 2, 2 }, { 0, 1, 1, 0, 0, 1, 1, 0, 2, 0, 0, 2, 2, 2, 2, 2 },
        { 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1 }, { 0, 0, 0, 0, 2, 0, 0, 0, 2, 2, 1, 1, 2, 2, 2, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 2, 2, 2 }, { 0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 2, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 0, 0, 1, 2, 0, 0, 2, 2, 0, 2, 2, 2 }, { 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0 },
        { 0, 1, 2, 0, 2, 0, 1, 2, 1, 2, 0, 1, 0, 1, 2, 0 }, { 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1 }, { 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1 }, { 0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 2, 2 },
        { 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1 }, { 0, 2, 2, 0, 1, 2, 2, 1, 0, 2, 2, 0, 1, 2, 2, 1 },
        { 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 0, 1 }, { 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2 }, { 0, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 2, 0, 1, 1, 1 },
        { 0, 0, 0, 2, 1, 1, 1, 2, 0, 0, 0, 2, 1, 1, 1, 2 }, { 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2 },
        { 0, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2 }, { 0, 0, 0, 2, 1, 1, 1, 2, 1, 1, 1, 2, 0, 0, 0, 2 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2 }, { 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2 },
        { 0, 0, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2 },
        { 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1 }, { 0, 2, 2, 2, 1, 2, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2 },
        { 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 1, 1, 1, 2, 0, 1, 1, 2, 2, 0, 1, 2, 2, 2, 0 }
    };

    // Texels whose index is stored with one bit less. The first subset is always anchored at texel 0.
    constexpr uint8_t ANCHORS_2[64] =
    {
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15,  2,  8,  2,  2,  8,  8, 15,  2,  8,  2,  2,  8,  8,  2,  2,
        15, 15,  6,  8,  2,  8, 15, 15,  2,  8,  2,  2,  2, 15, 15,  6,
         6,  2,  6,  8, 15, 15,  2,  2, 15, 15, 15, 15, 15,  2,  2, 15
    };

    constexpr uint8_t ANCHORS_3_SECOND[64] =
    {
         3,  3, 15, 15,  8,  3, 15, 15,  8,  8,  6,  6,  6,  5,  3,  3,
         3,  3,  8, 15,  3,  3,  6, 10,  5,  8,  8,  6,  8,  5, 15, 15,
         8, 15,  3,  5,  6, 10,  8, 15, 15,  3, 15,  5, 15, 15, 15, 15,
         3, 15,  5,  5,  5,  8,  5, 10,  5, 10,  8, 13, 15, 12,  3,  3
    };

    constexpr uint8_t ANCHORS_3_THIRD[64] =
    {
        15,  8,  8,  3, 15, 15,  3,  8, 15, 15, 15, 15, 15, 15, 15,  8,
        15,  8, 15,  3, 15,  8, 15,  8,  3, 15,  6, 10, 15, 15, 10,  8,
        15,  3, 15, 10, 10,  8,  9, 10,  6, 15,  8, 15,  3,  6,  6,  8,
        15,  3, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,  3, 15, 15,  8
    };

    // BC6H

    enum BC6HField : uint8_t
    {
        RW, GW, BW, // Endpoint 0 of the first region.
        RX, GX, BX, // Endpoint 1 of the first region.
        RY, GY, BY, // Endpoint 0 of the second region.
        RZ, GZ, BZ, // Endpoint 1 of the second region.
        D           // Partition index.
    };

    struct BC6HSegment
    {
        uint8_t field = 0;
        uint8_t shift = 0;
        uint8_t count = 0;
        bool reversed = false;
    };

    // Bit layout of each mode's header after the mode bits, as documented in the Direct3D 11 functional specification.
    constexpr BC6HSegment BC6H_LAYOUTS[14][25] =
    {
    // Mode 0 (00)
    { { GY, 4, 1 }, { BY, 4, 1 }, { BZ, 4, 1 }, { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 },
      { RX, 0, 5 }, { GZ, 4, 1 }, { GY, 0, 4 }, { GX, 0, 5 }, { BZ, 0, 1 }, { GZ, 0, 4 },
      { BX, 0, 5 }, { BZ, 1, 1 }, { BY, 0, 4 }, { RY, 0, 5 }, { BZ, 2, 1 }, { RZ, 0, 5 },
      { BZ, 3, 1 }, { D, 0, 5 } },
    // Mode 1 (01)
    { { GY, 5, 1 }, { GZ, 4, 1 }, { GZ, 5, 1 }, { RW, 0, 7 }, { BZ, 0, 1 }, { BZ, 1, 1 },
      { BY, 4, 1 }, { GW, 0, 7 }, { BY, 5, 1 }, { BZ, 2, 1 }, { GY, 4, 1 }, { BW, 0, 7 },
      { BZ, 3, 1 }, { BZ, 5, 1 }, { BZ, 4, 1 }, { RX, 0, 6 }, { GY, 0, 4 }, { GX, 0, 6 },
      { GZ, 0, 4 }, { BX, 0, 6 }, { BY, 0, 4 }, { RY, 0, 6 }, { RZ, 0, 6 }, { D, 0, 5 } },
    // Mode 2 (00010)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 5 }, { RW, 10, 1 }, { GY, 0, 4 },
      { GX, 0, 4 }, { GW, 10, 1 }, { BZ, 0, 1 }, { GZ, 0, 4 }, { BX, 0, 4 }, { BW, 10, 1 },
      { BZ, 1, 1 }, { BY, 0, 4 }, { RY, 0, 5 }, { BZ, 2, 1 }, { RZ, 0, 5 }, { BZ, 3, 1 },
      { D, 0, 5 } },
    // Mode 3 (00110)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 4 }, { RW, 10, 1 }, { GZ, 4, 1 },
      { GY, 0, 4 }, { GX, 0, 5 }, { GW, 10, 1 }, { GZ, 0, 4 }, { BX, 0, 4 }, { BW, 10, 1 },
      { BZ, 1, 1 }, { BY, 0, 4 }, { RY, 0, 4 }, { BZ, 0, 1 }, { BZ, 2, 1 }, { RZ, 0, 4 },
      { GY, 4, 1 }, { BZ, 3, 1 }, { D, 0, 5 } },
    // Mode 4 (01010)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 4 }, { RW, 10, 1 }, { BY, 4, 1 },
      { GY, 0, 4 }, { GX, 0, 4 }, { GW, 10, 1 }, { BZ, 0, 1 }, { GZ, 0, 4 }, { BX, 0, 5 },
      { BW, 10, 1 }, { BY, 0, 4 }, { RY, 0, 4 }, { BZ, 1, 1 }, { BZ, 2, 1 }, { RZ, 0, 4 },
      { BZ, 4, 1 }, { BZ, 3, 1 }, { D, 0, 5 } },
    // Mode 5 (01110)
    { { RW, 0, 9 }, { BY, 4, 1 }, { GW, 0, 9 }, { GY, 4, 1 }, { BW, 0, 9 }, { BZ, 4, 1 },
      { RX, 0, 5 }, { GZ, 4, 1 }, { GY, 0, 4 }, { GX, 0, 5 }, { BZ, 0, 1 }, { GZ, 0, 4 },
      { BX, 0, 5 }, { BZ, 1, 1 }, { BY, 0, 4 }, { RY, 0, 5 }, { BZ, 2, 1 }, { RZ, 0, 5 },
      { BZ, 3, 1 }, { D, 0, 5 } },
    // Mode 6 (10010)
    { { RW, 0, 8 }, { GZ, 4, 1 }, { BY, 4, 1 }, { GW, 0, 8 }, { BZ, 2, 1 }, { GY, 4, 1 },
      { BW, 0, 8 }, { BZ, 3, 1 }, { BZ, 4, 1 }, { RX, 0, 6 }, { GY, 0, 4 }, { GX, 0, 5 },
      { BZ, 0, 1 }, { GZ, 0, 4 }, { BX, 0, 5 }, { BZ, 1, 1 }, { BY, 0, 4 }, { RY, 0, 6 },
      { RZ, 0, 6 }, { D, 0, 5 } },
    // Mode 7 (10110)
    { { RW, 0, 8 }, { BZ, 0, 1 }, { BY, 4, 1 }, { GW, 0, 8 }, { GY, 5, 1 }, { GY, 4, 1 },
      { BW, 0, 8 }, { GZ, 5, 1 }, { BZ, 4, 1 }, { RX, 0, 5 }, { GZ, 4, 1 }, { GY, 0, 4 },
      { GX, 0, 6 }, { GZ, 0, 4 }, { BX, 0, 5 }, { BZ, 1, 1 }, { BY, 0, 4 }, { RY, 0, 5 },
      { BZ, 2, 1 }, { RZ, 0, 5 }, { BZ, 3, 1 }, { D, 0, 5 } },
    // Mode 8 (11010)
    { { RW, 0, 8 }, { BZ, 1, 1 }, { BY, 4, 1 }, { GW, 0, 8 }, { BY, 5, 1 }, { GY, 4, 1 },
      { BW, 0, 8 }, { BZ, 5, 1 }, { BZ, 4, 1 }, { RX, 0, 5 }, { GZ, 4, 1 }, { GY, 0, 4 },
      { GX, 0, 5 }, { BZ, 0, 1 }, { GZ, 0, 4 }, { BX, 0, 6 }, { BY, 0, 4 }, { RY, 0, 5 },
      { BZ, 2, 1 }, { RZ, 0, 5 }, { BZ, 3, 1 }, { D, 0, 5 } },
    // Mode 9 (11110)
    { { RW, 0, 6 }, { GZ, 4, 1 }, { BZ, 0, 1 }, { BZ, 1, 1 }, { BY, 4, 1 }, { GW, 0, 6 },
      { GY, 5, 1 }, { BY, 5, 1 }, { BZ, 2, 1 }, { GY, 4, 1 }, { BW, 0, 6 }, { GZ, 5, 1 },
      { BZ, 3, 1 }, { BZ, 5, 1 }, { BZ, 4, 1 }, { RX, 0, 6 }, { GY, 0, 4 }, { GX, 0, 6 },
      { GZ, 0, 4 }, { BX, 0, 6 }, { BY, 0, 4 }, { RY, 0, 6 }, { RZ, 0, 6 }, { D, 0, 5 } },
    // Mode 10 (00011)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 10 }, { GX, 0, 10 }, { BX, 0, 10 } },
    // Mode 11 (00111)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 9 }, { RW, 10, 1 }, { GX, 0, 9 },
      { GW, 10, 1 }, { BX, 0, 9 }, { BW, 10, 1 } },
    // Mode 12 (01011)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 8 }, { RW, 10, 2, true }, { GX, 0, 8 },
      { GW, 10, 2, true }, { BX, 0, 8 }, { BW, 10, 2, true } },
    // Mode 13 (01111)
    { { RW, 0, 10 }, { GW, 0, 10 }, { BW, 0, 10 }, { RX, 0, 4 }, { RW, 10, 6, true }, { GX, 0, 4 },
      { GW, 10, 6, true }, { BX, 0, 4 }, { BW, 10, 6, true } },
    };

    struct BC6HMode
    {
        uint8_t endpointBits;
        uint8_t deltaBits[3];
        bool transformed;
        bool twoRegions;
    };

    constexpr BC6HMode BC6H_MODES[14] =
    {
        { 10, { 5, 5, 5 }, true, true },
        { 7, { 6, 6, 6 }, true, true },
        { 11, { 5, 4, 4 }, true, true },
        { 11, { 4, 5, 4 }, true, true },
        { 11, { 4, 4, 5 }, true, true },
        { 9, { 5, 5, 5 }, true, true },
        { 8, { 6, 5, 5 }, true, true },
        { 8, { 5, 6, 5 }, true, true },
        { 8, { 5, 5, 6 }, true, true },
        { 6, { 6, 6, 6 }, false, true },
        { 10, { 10, 10, 10 }, false, false },
        { 11, { 9, 9, 9 }, true, false },
        { 12, { 8, 8, 8 }, true, false },
        { 16, { 4, 4, 4 }, true, false }
    };

    int32_t GetBC6HModeIndex(uint32_t modeBits)
    {
        switch (modeBits)
        {
        case 0x00: return 0;
        case 0x01: return 1;
        case 0x02: return 2;
        case 0x06: return 3;
        case 0x0A: return 4;
        case 0x0E: return 5;
        case 0x12: return 6;
        case 0x16: return 7;
        case 0x1A: return 8;
        case 0x1E: return 9;
        case 0x03: return 10;
        case 0x07: return 11;
        case 0x0B: return 12;
        case 0x0F: return 13;
        default: return -1;
        }
    }

    int32_t SignExtend(int32_t value, uint32_t bits)
    {
        uint32_t shift = 32 - bits;
        return int32_t(uint32_t(value) << shift) >> shift;
    }

    int32_t UnquantizeBC6H(int32_t value, uint32_t bits, bool isSigned)
    {
        if (!isSigned)
        {
            if (bits >= 15 || value == 0)
                return value;

            if (value == ((1 << bits) - 1))
                return 0xFFFF;

            return ((value << 16) + 0x8000) >> bits;
        }

        if (bits >= 16)
            return value;

        bool negative = value < 0;
        if (negative)
            value = -value;

        int32_t result;
        if (value == 0)
            result = 0;
        else if (value >= ((1 << (bits - 1)) - 1))
            result = 0x7FFF;
        else
            result = ((value << 15) + 0x4000) >> (bits - 1);

        return negative ? -result : result;
    }

    uint16_t FinishUnquantizeBC6H(int32_t value, bool isSigned)
    {
        if (!isSigned)
            return uint16_t((value * 31) >> 6);

        value = (value < 0) ? -(((-value) * 31) >> 5) : ((value * 31) >> 5);
        return (value < 0) ? uint16_t(0x8000 | -value) : uint16_t(value);
    }

    void DecodeBC6HBlock(const uint8_t* block, uint16_t* rgba, bool isSigned)
    {
        constexpr uint16_t HALF_ONE = 0x3C00;

        BitReader bits(block);

        uint32_t modeBits = bits.Read(2);
        if (modeBits > 1)
            modeBits |= bits.Read(3) << 2;

        int32_t modeIndex = GetBC6HModeIndex(modeBits);
        if (modeIndex < 0)
        {
            // Reserved modes decode to black.
            for (uint32_t i = 0; i < 16; i++)
            {
                rgba[i * 4 + 0] = 0;
                rgba[i * 4 + 1] = 0;
                rgba[i * 4 + 2] = 0;
                rgba[i * 4 + 3] = HALF_ONE;
            }

            return;
        }

        int32_t fields[13] = {};
        for (const BC6HSegment& segment : BC6H_LAYOUTS[modeIndex])
        {
            if (segment.count == 0)
                break;

            uint32_t value = segment.reversed ? bits.ReadReversed(segment.count) : bits.Read(segment.count);
            fields[segment.field] |= int32_t(value << segment.shift);
        }

        const BC6HMode& mode = BC6H_MODES[modeIndex];
        uint32_t numEndpoints = mode.twoRegions ? 4 : 2;

        int32_t endpoints[4][3] =
        {
            { fields[RW], fields[GW], fields[BW] },
            { fields[RX], fields[GX], fields[BX] },
            { fields[RY], fields[GY], fields[BY] },
            { fields[RZ], fields[GZ], fields[BZ] }
        };

        for (uint32_t channel = 0; channel < 3; channel++)
        {
            if (isSigned)
                endpoints[0][channel] = SignExtend(endpoints[0][channel], mode.endpointBits);

            for (uint32_t i = 1; i < numEndpoints; i++)
            {
                int32_t& endpoint = endpoints[i][channel];

                if (mode.transformed)
                {
                    int32_t delta = SignExtend(endpoint, mode.deltaBits[channel]);
                    endpoint = (endpoints[0][channel] + delta) & ((1 << mode.endpointBits) - 1);
                }

                if (isSigned)
                    endpoint = SignExtend(endpoint, mode.endpointBits);
            }

            for (uint32_t i = 0; i < numEndpoints; i++)
                endpoints[i][channel] = UnquantizeBC6H(endpoints[i][channel], mode.endpointBits, isSigned);
        }

        uint32_t partition = uint32_t(fields[D]);
        uint32_t regionIndexBits = mode.twoRegions ? 3 : 4;

        for (uint32_t i = 0; i < 16; i++)
        {
            uint32_t region = mode.twoRegions ? ((PARTITIONS_2[partition] >> i) & 0x1) : 0;
            bool anchor = (i == 0) || (mode.twoRegions && i == ANCHORS_2[partition]);
            uint32_t weight = GetWeight(regionIndexBits, bits.Read(regionIndexBits - anchor));

            const int32_t* e0 = endpoints[region * 2];
            const int32_t* e1 = endpoints[region * 2 + 1];

            for (uint32_t channel = 0; channel < 3; channel++)
            {
                int32_t value = (e0[channel] * int32_t(64 - weight) + e1[channel] * int32_t(weight) + 32) >> 6;
                rgba[i * 4 + channel] = FinishUnquantizeBC6H(value, isSigned);
            }

            rgba[i * 4 + 3] = HALF_ONE;
        }
    }

    // BC7

    struct BC7Mode
    {
        uint8_t numSubsets;
        uint8_t partitionBits;
        uint8_t rotationBits;
        uint8_t indexSelectionBits;
        uint8_t colorBits;
        uint8_t alphaBits;
        uint8_t endpointPBits; // One P-bit per endpoint.
        uint8_t sharedPBits;   // One P-bit per subset.
        uint8_t indexBits;
        uint8_t secondaryIndexBits;
    };

    constexpr BC7Mode BC7_MODES[8] =
    {
        { 3, 4, 0, 0, 4, 0, 1, 0, 3, 0 },
        { 2, 6, 0, 0, 6, 0, 0, 1, 3, 0 },
        { 3, 6, 0, 0, 5, 0, 0, 0, 2, 0 },
        { 2, 6, 0, 0, 7, 0, 1, 0, 2, 0 },
        { 1, 0, 2, 1, 5, 6, 0, 0, 2, 3 },
        { 1, 0, 2, 0, 7, 8, 0, 0, 2, 2 },
        { 1, 0, 0, 0, 7, 7, 1, 0, 4, 0 },
        { 2, 6, 0, 0, 5, 5, 1, 0, 2, 0 }
    };

    uint32_t ExpandBC7Component(uint32_t value, uint32_t bits)
    {
        value <<= (8 - bits);
        return value | (value >> bits);
    }

    uint8_t InterpolateBC7(uint32_t e0, uint32_t e1, uint32_t weight)
    {
        return uint8_t((e0 * (64 - weight) + e1 * weight + 32) >> 6);
    }

    void DecodeBC7Block(const uint8_t* block, uint8_t* rgba)
    {
        BitReader bits(block);

        uint32_t modeIndex = 0;
        while (modeIndex < 8 && bits.Read(1) == 0)
            modeIndex++;

        if (modeIndex >= 8)
        {
            // Reserved mode, decodes to transparent black.
            memset(rgba, 0, 16 * 4);
            return;
        }

        const BC7Mode& mode = BC7_MODES[modeIndex];
        uint32_t partition = bits.Read(mode.partitionBits);
        uint32_t rotation = bits.Read(mode.rotationBits);
        uint32_t indexSelection = bits.Read(mode.indexSelectionBits);

        uint32_t numEndpoints = mode.numSubsets * 2u;
        uint32_t endpoints[6][4] = {};

        for (uint32_t channel = 0; channel < 3; channel++)
        {
            for (uint32_t i = 0; i < numEndpoints; i++)
                endpoints[i][channel] = bits.Read(mode.colorBits);
        }

        if (mode.alphaBits != 0)
        {
            for (uint32_t i = 0; i < numEndpoints; i++)
                endpoints[i][3] = bits.Read(mode.alphaBits);
        }

        uint32_t colorBits = mode.colorBits;
        uint32_t alphaBits = mode.alphaBits;

        if (mode.endpointPBits != 0 || mode.sharedPBits != 0)
        {
            for (uint32_t i = 0; i < numEndpoints; i++)
            {
                // Endpoints of the same subset share the P-bit in modes with shared P-bits.
                if (mode.endpointPBits != 0 || (i & 1) == 0)
                {
                    uint32_t pBit = bits.Read(1);
                    uint32_t count = (mode.endpointPBits != 0) ? 1 : 2;

                    for (uint32_t j = i; j < i + count; j++)
                    {
                        for (uint32_t channel = 0; channel < 4; channel++)
                            endpoints[j][channel] = (endpoints[j][channel] << 1) | pBit;
                    }
                }
            }

            colorBits++;

            if (alphaBits != 0)
                alphaBits++;
        }

        for (uint32_t i = 0; i < numEndpoints; i++)
        {
            for (uint32_t channel = 0; channel < 3; channel++)
                endpoints[i][channel] = ExpandBC7Component(endpoints[i][channel], colorBits);

            endpoints[i][3] = (alphaBits != 0) ? ExpandBC7Component(endpoints[i][3], alphaBits) : 255;
        }

        uint8_t subsets[16];
        uint32_t anchors[3] = { 0, 0, 0 };

        for (uint32_t i = 0; i < 16; i++)
        {
            if (mode.numSubsets == 2)
                subsets[i] = (PARTITIONS_2[partition] >> i) & 0x1;
            else if (mode.numSubsets == 3)
                subsets[i] = PARTITIONS_3[partition][i];
            else
                subsets[i] = 0;
        }

        if (mode.numSubsets == 2)
        {
            anchors[1] = ANCHORS_2[partition];
        }
        else if (mode.numSubsets == 3)
        {
            anchors[1] = ANCHORS_3_SECOND[partition];
            anchors[2] = ANCHORS_3_THIRD[partition];
        }

        uint8_t indices[16];
        uint8_t secondaryIndices[16] = {};

        for (uint32_t i = 0; i < 16; i++)
            indices[i] = uint8_t(bits.Read(mode.indexBits - (i == anchors[subsets[i]])));

        if (mode.secondaryIndexBits != 0)
        {
            for (uint32_t i = 0; i < 16; i++)
                secondaryIndices[i] = uint8_t(bits.Read(mode.secondaryIndexBits - (i == 0)));
        }

        for (uint32_t i = 0; i < 16; i++)
        {
            const uint32_t* e0 = endpoints[subsets[i] * 2];
            const uint32_t* e1 = endpoints[subsets[i] * 2 + 1];
            uint8_t* texel = rgba + i * 4;

            uint32_t colorWeight = GetWeight(mode.indexBits, indices[i]);
            uint32_t alphaWeight = colorWeight;

            if (mode.secondaryIndexBits != 0)
            {
                alphaWeight = GetWeight(mode.secondaryIndexBits, secondaryIndices[i]);

                if (indexSelection != 0)
                    std::swap(colorWeight, alphaWeight);
            }

            for (uint32_t channel = 0; channel < 3; channel++)
                texel[channel] = InterpolateBC7(e0[channel], e1[channel], colorWeight);

            texel[3] = InterpolateBC7(e0[3], e1[3], alphaWeight);

            if (rotation != 0)
                std::swap(texel[rotation - 1], texel[3]);
        }
    }
}

namespace bc
{
    uint32_t GetBlockSize(Format format)
    {
        switch (format)
        {
        case Format::BC1:
        case Format::BC4_UNORM:
        case Format::BC4_SNORM:
            return 8;
        default:
            return 16;
        }
    }

    uint32_t GetDecodedTexelSize(Format format)
    {
        switch (format)
        {
        case Format::BC4_UNORM:
        case Format::BC4_SNORM:
            return 1;
        case Format::BC5_UNORM:
        case Format::BC5_SNORM:
            return 2;
        case Format::BC6H_UF16:
        case Format::BC6H_SF16:
            return 8;
        default:
            return 4;
        }
    }

    size_t GetCompressedSurfaceSize(Format format, uint32_t width, uint32_t height)
    {
        size_t blocksX = (size_t(width) + 3) / 4;
        size_t blocksY = (size_t(height) + 3) / 4;
        return blocksX * blocksY * GetBlockSize(format);
    }

    void DecodeBlock(Format format, const uint8_t* block, void* texels)
    {
        uint8_t* out = reinterpret_cast<uint8_t*>(texels);

        switch (format)
        {
        case Format::BC1:
            DecodeColorBlock(block, out, true);
            break;
        case Format::BC2:
            DecodeColorBlock(block + 8, out, false);
            DecodeExplicitAlpha(block, out);
            break;
        case Format::BC3:
            DecodeColorBlock(block + 8, out, false);
            DecodeUnsignedChannel(block, out + 3, 4);
            break;
        case Format::BC4_UNORM:
            DecodeUnsignedChannel(block, out, 1);
            break;
        case Format::BC4_SNORM:
            DecodeSignedChannel(block, out, 1);
            break;
        case Format::BC5_UNORM:
            DecodeUnsignedChannel(block, out, 2);
            DecodeUnsignedChannel(block + 8, out + 1, 2);
            break;
        case Format::BC5_SNORM:
            DecodeSignedChannel(block, out, 2);
            DecodeSignedChannel(block + 8, out + 1, 2);
            break;
        case Format::BC6H_UF16:
        case Format::BC6H_SF16:
            DecodeBC6HBlock(block, reinterpret_cast<uint16_t*>(texels), format == Format::BC6H_SF16);
            break;
        case Format::BC7:
            DecodeBC7Block(block, out);
            break;
        }
    }

    void DecodeSurface(Format format, const uint8_t* src, uint32_t width, uint32_t height, uint8_t* dst, size_t dstRowPitch)
    {
        const uint32_t blockSize = GetBlockSize(format);
        const uint32_t texelSize = GetDecodedTexelSize(format);
        const uint32_t blocksX = (width + 3) / 4;
        const uint32_t blocksY = (height + 3) / 4;

        alignas(8) uint8_t texels[16 * 8];

        for (uint32_t blockY = 0; blockY < blocksY; blockY++)
        {
            const uint32_t copyHeight = std::min(4u, height - blockY * 4);

            for (uint32_t blockX = 0; blockX < blocksX; blockX++)
            {
                DecodeBlock(format, src, texels);
                src += blockSize;

                const uint32_t copyWidth = std::min(4u, width - blockX * 4);

                for (uint32_t y = 0; y < copyHeight; y++)
                {
                    size_t dstOffset = (size_t(blockY) * 4 + y) * dstRowPitch + size_t(blockX) * 4 * texelSize;
                    memcpy(dst + dstOffset, texels + y * 4 * texelSize, size_t(copyWidth) * texelSize);
                }
            }
        }
    }
}
