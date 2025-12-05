#pragma once
#include "pch.hpp"

enum PixelFormat
{
    eR = GL_RED,
    eRG = GL_RG,
    eRGB = GL_RGB,
    eRGBA = GL_RGBA,
    eInvalid = -1,
    eNonApplicable = -2,
};
enum TextureFilter
{
    Nearest = GL_NEAREST,
    Linear = GL_LINEAR,
};
class Texture
{
    GLuint Handle;
    TextureFilter filter = Linear;
    PixelFormat pixel_format;
    GLenum InternalFormat;
    uvec2 Dimensions;

public:
    Texture()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &Handle);
    }
    ~Texture()
    {
        glDeleteTextures(1, &Handle);
    }
    GLuint GetHandle() const { return Handle; }
    void Bind()
    {
        glBindTexture(GL_TEXTURE_2D, Handle);
    }
    static void Unbind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    void Alloc2D(uvec2 _Dimensions, PixelFormat pixelFormat = eRGB, GLenum Format = GL_RGBA8_SNORM, const void *data = nullptr)
    {
        Dimensions = _Dimensions;
        pixel_format = pixelFormat;
        InternalFormat = Format;
        Bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexImage2D(GL_TEXTURE_2D, 0, Format, Dimensions.x, Dimensions.y, 0, pixelFormat, GL_UNSIGNED_BYTE, data);
        Unbind();
    }
    void Alloc2DStorage(uvec2 _Dimensions, GLenum Format = GL_RGBA8_SNORM)
    {
        pixel_format = eNonApplicable;
        Dimensions = _Dimensions;
        InternalFormat = Format;
        Bind();
        glTextureStorage2D(Handle, 1, Format, Dimensions.x, Dimensions.y);
        glTextureParameteri(Handle, GL_TEXTURE_MIN_FILTER, filter);
        glTextureParameteri(Handle, GL_TEXTURE_MAG_FILTER, filter);
        glTextureParameteri(Handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(Handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        Unbind();
    }
    void GetData(void *dest, size_t size)
    {
        glGetTextureImage(Handle, 0, pixel_format, GL_UNSIGNED_BYTE, size, dest);
    }
    void GenerateMipmaps()
    {
        Bind();
        glGenerateMipmap(GL_TEXTURE_2D);
        Unbind();
    }
    void SetFilter(TextureFilter Newfilter)
    {
        Bind();
        filter = Newfilter;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        Unbind();
    };

    std::shared_ptr<Texture> BlitToNew(uvec2 newSize, TextureFilter filterMode = Linear) const
    {
        // Create destination texture
        auto dst = std::make_shared<Texture>();
        dst->SetFilter(filterMode);
        dst->Alloc2D(newSize, pixel_format, InternalFormat);

        // Create temporary framebuffers
        GLuint srcFBO = 0, dstFBO = 0;
        glGenFramebuffers(1, &srcFBO);
        glGenFramebuffers(1, &dstFBO);

        // Bind source texture to read FBO
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Handle, 0);

        // Bind destination texture to draw FBO
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst->GetHandle(), 0);

        // Perform blit (scaled or unscaled copy)
        glBlitFramebuffer(
            0, 0, Dimensions.x, Dimensions.y,
            0, 0, newSize.x, newSize.y,
            GL_COLOR_BUFFER_BIT,
            filterMode == Linear ? GL_LINEAR : GL_NEAREST);

        // Cleanup
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &srcFBO);
        glDeleteFramebuffers(1, &dstFBO);

        return dst;
    }
    TextureFilter GetFilter() const { return filter; }
    PixelFormat GetPixelFormat() const { return pixel_format; }
    GLenum GetInternalFormat() const { return InternalFormat; }
    uvec2 GetDimensions() const { return Dimensions; }
};