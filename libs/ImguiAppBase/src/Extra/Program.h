#pragma once
#include "../lib_include.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

enum Shader_Stage : GLenum
{
    eVertex = GL_VERTEX_SHADER,
    eGeometry = GL_GEOMETRY_SHADER,
    eFragment = GL_FRAGMENT_SHADER,
    eCompute = GL_COMPUTE_SHADER
};
class Program
{
    GLuint gl_program = -1;

    std::vector<std::pair<Shader_Stage, std::string>> files;

public:
    Program(const decltype(files) &_files, bool is_this_source = false);
    template <typename T>
    void PushUniform(const std::string &, const T &);
    void Use() const { glUseProgram(gl_program); }
    void Unuse() const { glUseProgram(0); }

private:
    GLuint FetchUniformLocation(const std::string &n)
    {
        return glGetUniformLocation(gl_program, n.c_str());
    }
};
