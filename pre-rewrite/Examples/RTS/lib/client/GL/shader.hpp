///////////////////////////////////////////////////////////////////////
// A slight encapsulation of a shader program. This contains methods
// to build a shader program from multiples files containing vertex
// and pixel shader code, and a method to link the result.  When
// loaded (method "Use"), its vertex shader and pixel shader will be
// invoked for all geometry passing through the graphics pipeline.
// When done, unload it with method "Unuse".
////////////////////////////////////////////////////////////////////////
#pragma once
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "Global/pch.hpp"
class ShaderProgram
{
public:
    int programId;
    
    ShaderProgram();
    void AddShader(const char* source, const GLenum type);
    void LinkProgram();
    void UseShader();
    void UnuseShader();

private:
    GLint GetUniformLocation(const char* name)
    {
        GLint loc = glGetUniformLocation(programId, name);
        ASSERT(loc != -1 ,"Uniform not found!");
        return loc;
    }

public:

    // =========================
    // Uniform setters
    // =========================

    void SetInt(const char* name, int value)
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void SetFloat(const char* name, float value)
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void SetVec3(const char* name, const vec3& v)
    {
        glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(v));
    }

    void SetVec4(const char* name, const vec4& v)
    {
        glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(v));
    }

    void SetMat4(const char* name, const mat4& m)
    {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
    }
};
