
#include "texture.hpp"

#include <filesystem>

#include "stb_image.h"

Texture::Texture() : textureId(0) {}

Texture::Texture(const std::filesystem::path& path) : textureId(0)
{
	stbi_set_flip_vertically_on_load(true);
	image = stbi_load(path.c_str(), &width, &height, &depth, 4);
	printf("%d %d %d %s\n", depth, width, height, path.c_str());
	if (!image)
	{
		printf("\nRead error on file %s:\n  %s\n\n", path.c_str(), stbi_failure_reason());
		exit(-1);
	}

	glGenTextures(1, &textureId);  // Get an integer id for this texture from OpenGL
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
				 image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 10);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR_MIPMAP_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(image);
}

Texture::Texture(int w, int h, GLenum internalFormat, GLenum format, GLenum type, const void* data)
	: textureId(0), width(w), height(h), depth(1), image(nullptr)
{
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalFormat, width, height, 0, format, type, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int)GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int)GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
}
void Texture::Create2D(int w, int h, GLenum internalFormat, GLenum format, GLenum type,
					   const void* data)
{
	width = w;
	height = h;
	depth = 1;

	if (textureId == 0)
		glGenTextures(1, &textureId);

	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalFormat, width, height, 0, format, type, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int)GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int)GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
}
void Texture::Upload2D(GLenum format, GLenum type, const void* data)
{
	if (!textureId || !data)
		return;

	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, data);

	glBindTexture(GL_TEXTURE_2D, 0);
}
// Make a texture availabe to a shader program.  The unit parameter is
// a small integer specifying which texture unit should load the
// texture.  The name parameter is the sampler2d in the shader program
// which will provide access to the texture.
void Texture::BindTexture(const int unit, const int programId, const std::string& name)
{
	glActiveTexture((GLenum)((int)GL_TEXTURE0 + unit));
	glBindTexture(GL_TEXTURE_2D, textureId);
	int loc = glGetUniformLocation(programId, name.c_str());
	glUniform1i(loc, unit);
}

// Unbind a texture from a texture unit whne no longer needed.
void Texture::UnbindTexture(const int unit)
{
	glActiveTexture((GLenum)((int)GL_TEXTURE0 + unit));
	glBindTexture(GL_TEXTURE_2D, 0);
}
