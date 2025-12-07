#include "Texture.h"
Texture::Texture()
{
glCreateTextures(GL_TEXTURE_2D,1, &Handle);
}

Texture::~Texture()
{
    glDeleteTextures(1,&Handle);
}
