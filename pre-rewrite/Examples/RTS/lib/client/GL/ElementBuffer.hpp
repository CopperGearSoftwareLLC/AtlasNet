#pragma once

#include <vector>
#include "GL/VertexArray.hpp"
class ElementBuffer
{
public:
    GLuint id = 0;
    GLsizei count = 0;

    ElementBuffer(const std::vector<unsigned int>& indices)
    {
        count = static_cast<GLsizei>(indices.size());
        glGenBuffers(1, &id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int),
                     indices.data(),
                     GL_STATIC_DRAW);
    }

    void Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
    }
};