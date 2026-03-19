#pragma once
#include <GL/glew.h>

class VertexBuffer
{
   public:
	GLuint id = 0;
	GLenum usage = GL_STATIC_DRAW;

	// Constructor with initial data
	VertexBuffer(GLsizeiptr size, GLenum usage_ = GL_STATIC_DRAW, const void* data = nullptr)
		: usage(usage_)
	{
		glGenBuffers(1, &id);
		glBindBuffer(GL_ARRAY_BUFFER, id);
		glBufferData(GL_ARRAY_BUFFER, size, data, usage);
	}

	~VertexBuffer()
	{
		if (id)
			glDeleteBuffers(1, &id);
	}

	void Bind() const { glBindBuffer(GL_ARRAY_BUFFER, id); }

	static void Unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }

	// Resize or set data for the buffer
	void SetData(const void* data, GLsizeiptr size)
	{
		glBindBuffer(GL_ARRAY_BUFFER, id);
		glBufferData(GL_ARRAY_BUFFER, size, data, usage);
	}
};