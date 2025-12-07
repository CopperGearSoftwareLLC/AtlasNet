#include "Program.h"

Program::Program(const decltype(files) &_files, bool is_this_source) : files(_files)
{
    gl_program = glCreateProgram();
    std::vector<GLuint> shader_modules(_files.size());
    for (int i = 0; i < files.size(); i++)
    {
        const Shader_Stage stage = files[i].first;
        const std::string &file_name = files[i].second;
        std::string file_content;
        if (is_this_source)
        {
            file_content = file_name;
        }
        else
        {
            std::ifstream file(file_name);
            if (!file.is_open())
            {
                throw std::runtime_error(file_name + " does not exist");
            }
            file_content = (std::stringstream() << file.rdbuf()).str();
            file.close();
        }

        const char *file_content_str = file_content.c_str();

        GLuint module = glCreateShader(stage);
        glShaderSource(module, 1, &file_content_str, 0);
        std::cerr << "Compiling " << file_name << std::endl;
        glCompileShader(module);
        GLint info_log_length = 0;
        glGetShaderiv(module, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0)
        {
            std::string log(info_log_length + 1, 0);
            glGetShaderInfoLog(module, log.size(), 0, log.data());
            std::cout << "Shader Module Log [" + std::to_string(stage) + "]: " << log << std::endl;
        }
        glAttachShader(gl_program, module);
        shader_modules[i] = module;
    }
    for (int i = 0; i < 2; i++)
    {
        if (i == 0)
            glLinkProgram(gl_program);
        else
            glValidateProgram(gl_program);

        GLint success;
        glGetProgramiv(gl_program, GL_LINK_STATUS, &success);
        if (!success)
            std::cerr << "Shader Program failed to link\n";
        GLint info_log_length = 0;
        glGetProgramiv(gl_program, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0)
        {
            std::string log(info_log_length + 1, 0);
            glGetProgramInfoLog(gl_program, log.size(), 0, log.data());
            std::cout << "Shader Program Log: " << log << std::endl;
        }
    }
    for (const auto &module : shader_modules)
    {
        glDetachShader(gl_program, module);
        glDeleteShader(module);
    }

    std::cout << "Shader Program compiled\n";
}
template <>
void Program::PushUniform<mat4>(const std::string &name, const mat4 &m)
{
    Use();
    glUniformMatrix4fv(FetchUniformLocation(name), 1, GL_FALSE, &m[0][0]);
    Unuse();
}

template <>
void Program::PushUniform<vec3>(const std::string &name, const vec3 &v)
{
    Use();
    glUniform3fv(FetchUniformLocation(name), 1, &v.x);
    Unuse();
}
template <>
void Program::PushUniform<vec4>(const std::string &name, const vec4 &v)
{
    Use();
    glUniform4fv(FetchUniformLocation(name), 1, &v.x);
    Unuse();
}

template <>
void Program::PushUniform<float>(const std::string &name, const float &v)
{
    Use();
    glUniform1fv(FetchUniformLocation(name), 1, &v);
    Unuse();
}
template <>
void Program::PushUniform<int>(const std::string &name, const int &v)
{
    Use();
    glUniform1iv(FetchUniformLocation(name), 1, &v);
    Unuse();
}
template <>
void Program::PushUniform<uvec2>(const std::string &name, const uvec2 &v)
{
    Use();
    glUniform2ui(FetchUniformLocation(name), v.x, v.y);
    Unuse();
}
template <>
void Program::PushUniform<ivec2>(const std::string &name, const ivec2 &v)
{
    Use();
    glUniform2i(FetchUniformLocation(name), v.x, v.y);
    Unuse();
}
template <>
void Program::PushUniform<bool>(const std::string &name, const bool &v)
{
    Use();
    glUniform1i(FetchUniformLocation(name), static_cast<GLint>(v));
    Unuse();
}
template <>
void Program::PushUniform<uint32>(const std::string &name, const uint32 &v)
{
    Use();
    glUniform1ui(FetchUniformLocation(name), v);
    Unuse();
}