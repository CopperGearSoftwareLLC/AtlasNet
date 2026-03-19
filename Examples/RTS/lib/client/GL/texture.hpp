#include <GL/glew.h>
#include <filesystem>
class Texture
{
 public:
    unsigned int textureId;
    int width, height, depth;
    unsigned char* image;
    Texture();
    Texture(const std::filesystem::path &filename);

    void BindTexture(const int unit, const int programId, const std::string& name);
    void UnbindTexture(const int unit);
    Texture(
        int width,
        int height,
        GLenum internalFormat,
        GLenum format,
        GLenum type,
        const void* data = nullptr
    );
    // NEW: explicit API
    void Create2D(
        int width,
        int height,
        GLenum internalFormat,
        GLenum format,
        GLenum type,
        const void* data = nullptr
    );

    void Upload2D(
        GLenum format,
        GLenum type,
        const void* data
    );

    GLuint ID() const { return textureId; }
};
