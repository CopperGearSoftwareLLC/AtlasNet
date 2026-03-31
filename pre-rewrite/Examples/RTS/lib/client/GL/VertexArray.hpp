#pragma once
#include <GL/glew.h>

class VertexArray
{
public:
    GLuint id = 0;

    VertexArray()
    {
        glGenVertexArrays(1, &id);
    }

    ~VertexArray()
    {
        if (id)
            glDeleteVertexArrays(1, &id);
    }

    void Bind() const
    {
        glBindVertexArray(id);
    }

    static void Unbind()
    {
        glBindVertexArray(0);
    }

    void EnableAttribute(GLuint index) const
    {
        glEnableVertexAttribArray(index);
    }

    void SetAttribute(GLuint index, GLint size, GLenum type, GLboolean normalized,
                      GLsizei stride, const void* pointer) const
    {
        glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    }
};