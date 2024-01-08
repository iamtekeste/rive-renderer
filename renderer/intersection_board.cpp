/*
 * Copyright 2024 Rive
 */

#include "intersection_board.hpp"

#include "rive/math/math_types.hpp"

namespace rive::pls
{
void IntersectionTile::reset(int left, int top, uint16_t baselineGroupIndex)
{
    m_topLeft = {left, top, left, top};
    m_baselineGroupIndex = baselineGroupIndex;
    m_maxGroupIndex = baselineGroupIndex;
    m_edges.clear();
    m_groupIndices.clear();
    m_rectangleCount = 0;
}

void IntersectionTile::addRectangle(int4 ltrb, uint16_t groupIndex)
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.
    assert(groupIndex > m_baselineGroupIndex);

    // Transform and clamp ltrb to our tile.
    ltrb -= m_topLeft;
    assert(simd::all(ltrb.xy < 255)); // Ensure ltrb isn't completely outside the tile.
    assert(simd::all(ltrb.zw > 0));   // Ensure ltrb isn't completely outside the tile.
    ltrb = simd::clamp(ltrb, int4(0), int4(255));

    if (simd::all(ltrb == int4{0, 0, 255, 255}))
    {
        // The entire tile is covered -- reset to a new baseline.
        assert(groupIndex > m_maxGroupIndex);
        reset(m_topLeft.x, m_topLeft.y, groupIndex);
        return;
    }

    uint32_t subIdx = m_rectangleCount % kChunkSize;
    if (subIdx == 0)
    {
        // Push back maximally negative rectangles so they always fail intersection tests.
        assert(m_edges.size() * kChunkSize == m_rectangleCount);
        m_edges.push_back(uint8x32(255));

        // Uninitialized since the corresponding rectangles never pass an intersection test.
        assert(m_groupIndices.size() * kChunkSize == m_rectangleCount);
        m_groupIndices.emplace_back();
    }

    // m_edges is a list of 8 rectangles encoded as [L, T, 255 - R, 255 - B], relative to m_topLeft.
    // The data is also transposed: [L0..L7, T0..T7, -R0..R7, -B0..B7].
    m_edges.back()[subIdx] = ltrb.x;
    m_edges.back()[subIdx + 8] = ltrb.y;
    m_edges.back()[subIdx + 16] = 255 - ltrb.z;
    m_edges.back()[subIdx + 24] = 255 - ltrb.w;

    m_groupIndices.back()[subIdx] = groupIndex;

    m_maxGroupIndex = std::max(groupIndex, m_maxGroupIndex);
    ++m_rectangleCount;
}

uint16x8 IntersectionTile::findMaxIntersectingGroupIndex(int4 ltrb, uint16x8 groupIndices) const
{
    assert(simd::all(ltrb.xy < ltrb.zw)); // Ensure ltrb isn't zero or negative.

    // Transform and clamp ltrb to our tile.
    ltrb -= m_topLeft;
    assert(simd::all(ltrb.xy < 255)); // Ensure ltrb isn't completely outside the tile.
    assert(simd::all(ltrb.zw > 0));   // Ensure ltrb isn't completely outside the tile.
    ltrb = simd::clamp(ltrb, int4(0), int4(255));

    if (simd::all(ltrb == int4{0, 0, 255, 255}))
    {
        // The entire tile is covered -- we know it intersects with every rectangle.
        groupIndices[0] = std::max(groupIndices[0], m_maxGroupIndex);
        return groupIndices;
    }

    // Intersection test: l0 < r1 &&
    //                    t0 < b1 &&
    //                    r0 > l1 &&
    //                    b0 > t1
    //
    // Or, to make them all use the same operator: +l0 < +r1 &&
    //                                             +t0 < +b1 &&
    //                                             -r0 < -l1 &&
    //                                             -b0 < -t1
    //
    // m_edges are already encoded like the left column, so encode "ltrb" like the right.
    uint8x8 r = ltrb.z;
    uint8x8 b = ltrb.w;
    uint8x8 _l = 255 - ltrb.x;
    uint8x8 _t = 255 - ltrb.y;
    uint8x32 complement = simd::join(r, b, _l, _t);

    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        // Test 32 edges!
        auto edgeMasks = m_edges[i] < complement;
        // Each byte of isectMasks8 is 0xff if we intersect with the corresponding rectangle,
        // otherwise 0.
        int64_t isectMasks8 = simd::reduce_and(math::bit_cast<int64x4>(edgeMasks));
        // Sign-extend isectMasks8 to 16 bits per mask, where each element of isectMasks16 is 0xffff
        // if we intersect with the rectangle, otherwise 0. Encode these as signed integers so the
        // arithmetic shift copies the sign bit.
        int16x4 lo = math::bit_cast<int16x4>(isectMasks8) << 8;
        int16x4 hi = math::bit_cast<int16x4>(isectMasks8);
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) || defined(_WIN32)
        int16x8 isectMasks16 = simd::zip(lo, hi) >> 8; // Arithmetic shift right copies sign bit.
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        int16x8 isectMasks16 = simd::zip(hi, lo) >> 8; // Arithmetic shift right copies sign bit.
#endif
        // maskedGroupIndices[j] is m_groupIndices[i][j] if we intersect with the rect, otherwise 0.
        uint16x8 maskedGroupIndices = m_groupIndices[i] & isectMasks16;
        groupIndices = simd::max(maskedGroupIndices, groupIndices);
    }

    // Ensure we never drop below our baseline index.
    groupIndices[0] = std::max(groupIndices[0], m_baselineGroupIndex);
    return groupIndices;
}

void IntersectionBoard::resizeAndReset(uint32_t viewportWidth, uint32_t viewportHeight)
{
    m_viewportSize = int2{static_cast<int>(viewportWidth), static_cast<int>(viewportHeight)};

    // Divide the board into 255x255 tiles.
    int2 dims = (m_viewportSize + 254) / 255;
    m_cols = dims.x;
    m_rows = dims.y;
    m_tiles.resize(m_rows * m_cols);
    auto tileIter = m_tiles.begin();
    for (int y = 0; y < m_rows; ++y)
    {
        for (int x = 0; x < m_cols; ++x)
        {
            tileIter->reset(x * 255, y * 255);
            ++tileIter;
        }
    }
}

uint16_t IntersectionBoard::addRectangle(int4 ltrb)
{
    // Discard empty, negative, or offscreen rectangles.
    if (simd::any(ltrb.xy >= m_viewportSize || ltrb.zw <= 0 || ltrb.xy >= ltrb.zw))
    {
        return 0;
    }

    // Find the tiled row and column that each corner of the rectangle falls on.
    int4 span = (ltrb - int4{0, 0, 1, 1}) / 255;
    span = simd::clamp(span, int4(0), int4{m_cols, m_rows, m_cols, m_rows} - 1);
    assert(simd::all(span.xy <= span.zw));

    // Accumulate the max groupIndex from each tile the rectangle touches.
    uint16x8 maxGroupIndices = 0;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            maxGroupIndices = tileIter->findMaxIntersectingGroupIndex(ltrb, maxGroupIndices);
            ++tileIter;
        }
    }

    // Add the rectangle and its newly-found groupIndex to each tile it touches.
    uint16_t groupIndex = simd::reduce_max(maxGroupIndices) + 1;
    for (int y = span.y; y <= span.w; ++y)
    {
        auto tileIter = m_tiles.begin() + y * m_cols + span.x;
        for (int x = span.x; x <= span.z; ++x)
        {
            tileIter->addRectangle(ltrb, groupIndex);
            ++tileIter;
        }
    }

    return groupIndex;
}
} // namespace rive::pls
