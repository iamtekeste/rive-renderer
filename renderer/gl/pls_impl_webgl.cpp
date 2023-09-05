/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include <stdio.h>

#ifdef RIVE_WEBGL
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include "../out/obj/generated/glsl.exports.h"

namespace rive::pls
{
class PLSRenderContextGLImpl::PLSImplWebGL : public PLSRenderContextGLImpl::PLSImpl
{
    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID,
                                              size_t width,
                                              size_t height,
                                              const PlatformFeatures&) override
    {
        // WEBGL_shader_pixel_local_storage can't load or store to framebuffers.
        return nullptr;
    }

    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(
        size_t width,
        size_t height,
        const PlatformFeatures& platformFeatures) override
    {
        auto renderTarget = rcp(new PLSRenderTargetGL(width, height, platformFeatures));
        renderTarget->allocateCoverageBackingTextures();
        glFramebufferTexturePixelLocalStorageWEBGL(kFramebufferPlaneIdx,
                                                   renderTarget->m_offscreenTextureID,
                                                   0,
                                                   0);
        glFramebufferTexturePixelLocalStorageWEBGL(kCoveragePlaneIdx,
                                                   renderTarget->m_coverageTextureID,
                                                   0,
                                                   0);
        glFramebufferTexturePixelLocalStorageWEBGL(kOriginalDstColorPlaneIdx,
                                                   renderTarget->m_originalDstColorTextureID,
                                                   0,
                                                   0);
        glFramebufferTexturePixelLocalStorageWEBGL(kClipPlaneIdx,
                                                   renderTarget->m_clipTextureID,
                                                   0,
                                                   0);
        renderTarget->createSideFramebuffer();
        return renderTarget;
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl*,
                                   const PLSRenderContext::FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<const PLSRenderTargetGL*>(desc.renderTarget);
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->drawFramebufferID());

        if (desc.loadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            glFramebufferPixelLocalClearValuefvWEBGL(kFramebufferPlaneIdx, clearColor4f);
        }

        GLenum loadOps[4] = {(GLenum)(desc.loadAction == LoadAction::clear ? GL_LOAD_OP_CLEAR_WEBGL
                                                                           : GL_LOAD_OP_LOAD_WEBGL),
                             GL_LOAD_OP_ZERO_WEBGL,
                             GL_DONT_CARE,
                             (GLenum)(desc.needsClipBuffer ? GL_LOAD_OP_ZERO_WEBGL : GL_DONT_CARE)};

        glBeginPixelLocalStorageWEBGL(4, loadOps);
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*) override
    {
        constexpr static GLenum kStoreOps[4] = {GL_STORE_OP_STORE_WEBGL,
                                                GL_DONT_CARE,
                                                GL_DONT_CARE,
                                                GL_DONT_CARE};
        glEndPixelLocalStorageWEBGL(4, kStoreOps);
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_WEBGL; }
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplWebGL()
{
    return std::make_unique<PLSImplWebGL>();
}
} // namespace rive::pls
