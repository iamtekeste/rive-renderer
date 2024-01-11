/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_render_context_helper_impl.hpp"

#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::pls
{
rcp<PLSTexture> PLSRenderContextHelperImpl::decodeImageTexture(Span<const uint8_t> encodedBytes)
{
#ifdef RIVE_DECODERS
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap)
    {
        // For now, PLSRenderContextImpl::makeImageTexture() only accepts RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
        {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }
        uint32_t width = bitmap->width();
        uint32_t height = bitmap->height();
        uint32_t mipLevelCount = math::msb(height | width);
        return makeImageTexture(width, height, mipLevelCount, bitmap->bytes());
    }
#endif
    return nullptr;
}

void PLSRenderContextHelperImpl::resizePathBuffer(size_t sizeInBytes, size_t elementSizeInBytes)
{
    m_pathBuffer = makeStorageBufferRing(sizeInBytes, elementSizeInBytes);
}

void PLSRenderContextHelperImpl::resizeContourBuffer(size_t sizeInBytes, size_t elementSizeInBytes)
{
    m_contourBuffer = makeStorageBufferRing(sizeInBytes, elementSizeInBytes);
}

void PLSRenderContextHelperImpl::resizeSimpleColorRampsBuffer(size_t sizeInBytes)
{
    m_simpleColorRampsBuffer = makeTextureTransferBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeGradSpanBuffer(size_t sizeInBytes)
{
    m_gradSpanBuffer = makeVertexBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeTessVertexSpanBuffer(size_t sizeInBytes)
{
    m_tessSpanBuffer = makeVertexBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeTriangleVertexBuffer(size_t sizeInBytes)
{
    m_triangleBuffer = makeVertexBufferRing(sizeInBytes);
}

void PLSRenderContextHelperImpl::resizeImageDrawUniformBuffer(size_t sizeInBytes)
{
    m_imageDrawUniformBuffer = makeUniformBufferRing(sizeInBytes);
}

void* PLSRenderContextHelperImpl::mapPathBuffer(size_t mapSizeInBytes)
{
    return m_pathBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapContourBuffer(size_t mapSizeInBytes)
{
    return m_contourBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapSimpleColorRampsBuffer(size_t mapSizeInBytes)
{
    return m_simpleColorRampsBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapGradSpanBuffer(size_t mapSizeInBytes)
{
    return m_gradSpanBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapTessVertexSpanBuffer(size_t mapSizeInBytes)
{
    return m_tessSpanBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapTriangleVertexBuffer(size_t mapSizeInBytes)
{
    return m_triangleBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapImageDrawUniformBuffer(size_t mapSizeInBytes)
{
    return m_imageDrawUniformBuffer->mapBuffer(mapSizeInBytes);
}

void* PLSRenderContextHelperImpl::mapFlushUniformBuffer(size_t mapSizeInBytes)
{
    if (m_flushUniformBuffer == nullptr)
    {
        // Allocate the flushUniformBuffer lazily size it doesn't have a corresponding 'resize()'
        // call where we can allocate it.
        m_flushUniformBuffer = makeUniformBufferRing(sizeof(pls::FlushUniforms));
    }
    return m_flushUniformBuffer->mapBuffer(sizeof(pls::FlushUniforms));
}

void PLSRenderContextHelperImpl::unmapPathBuffer() { m_pathBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapContourBuffer() { m_contourBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapSimpleColorRampsBuffer()
{
    m_simpleColorRampsBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapGradSpanBuffer() { m_gradSpanBuffer->unmapAndSubmitBuffer(); }

void PLSRenderContextHelperImpl::unmapTessVertexSpanBuffer()
{
    m_tessSpanBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapTriangleVertexBuffer()
{
    m_triangleBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapImageDrawUniformBuffer()
{
    m_imageDrawUniformBuffer->unmapAndSubmitBuffer();
}

void PLSRenderContextHelperImpl::unmapFlushUniformBuffer()
{
    m_flushUniformBuffer->unmapAndSubmitBuffer();
}
} // namespace rive::pls
