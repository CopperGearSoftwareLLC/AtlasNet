// Framebuffer.h
#include <GL/glew.h>
#include <iostream>
#include <vector>
#pragma once

class Framebuffer
{
public:
    Framebuffer()
    {
        glCreateFramebuffers(1, &m_id);
    }

    ~Framebuffer()
    {
        glDeleteFramebuffers(1, &m_id);
    }

    void AttachColor(GLuint attachmentIndex, GLuint textureID, GLint level = 0)
    {
        glNamedFramebufferTexture(
            m_id,
            GL_COLOR_ATTACHMENT0 + attachmentIndex,
            textureID,
            level
        );

        if (attachmentIndex >= m_colorAttachments.size())
            m_colorAttachments.resize(attachmentIndex + 1);

        m_colorAttachments[attachmentIndex] =
            GL_COLOR_ATTACHMENT0 + attachmentIndex;
    }

    void AttachDepth(GLuint textureID, GLint level = 0)
    {
        glNamedFramebufferTexture(
            m_id,
            GL_DEPTH_ATTACHMENT,
            textureID,
            level
        );
    }

    void AttachDepthStencil(GLuint textureID, GLint level = 0)
    {
        glNamedFramebufferTexture(
            m_id,
            GL_DEPTH_STENCIL_ATTACHMENT,
            textureID,
            level
        );
    }

    void Finalize()
    {
        if (!m_colorAttachments.empty())
        {
            glNamedFramebufferDrawBuffers(
                m_id,
                static_cast<GLsizei>(m_colorAttachments.size()),
                m_colorAttachments.data()
            );
        }
        else
        {
            glNamedFramebufferDrawBuffer(m_id, GL_NONE);
            glNamedFramebufferReadBuffer(m_id, GL_NONE);
        }

        GLenum status = glCheckNamedFramebufferStatus(m_id, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            // You probably want better error handling
            std::cerr << "INVALID FRAMEBUFFER\n";
            throw std::runtime_error("");

        }
    }

    void Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_id);
    }

    static void BindDefault()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLuint ID() const { return m_id; }

private:
    GLuint m_id = 0;
    std::vector<GLenum> m_colorAttachments;
};
