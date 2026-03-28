
#include <fstream>


#include "shader.hpp"


// Creates an empty shader program.
ShaderProgram::ShaderProgram()
{ 
    programId = glCreateProgram();
}

// Use a shader program
void ShaderProgram::UseShader()
{
    glUseProgram(programId);
}

// Done using a shader program
void ShaderProgram::UnuseShader()
{
    glUseProgram(0);
}

// Read, send to OpenGL, and compile a single file into a shader
// program.  In case of an error, retrieve and print the error log
// string.
void ShaderProgram::AddShader(const char* source, GLenum type)
{
    // Read the source from the named file
    const char* psrc[1] = {source};

    // Create a shader and attach, hand it the source, and compile it.
    int shader = glCreateShader(type);
    glAttachShader(programId, shader);
    glShaderSource(shader, 1, psrc, NULL);
    glCompileShader(shader);

    // Get the compilation status
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    
    // If compilation status is not OK, get and print the log message.
    if (status != GL_TRUE) {
        int length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char* buffer = new char[length];
        glGetShaderInfoLog(shader, length, NULL, buffer);
        printf("Compile log:\n%s\n", buffer);
        delete buffer;
    }
}

// Link a shader program after all the shader files have been added
// with the AddShader method.  In case of an error, retrieve and print
// the error log string.
void ShaderProgram::LinkProgram()
{
    // Link program and check the status
    glLinkProgram(programId);
    int status;
    glGetProgramiv(programId, GL_LINK_STATUS, &status);
    
    // If link failed, get and print log
    if (status != 1) {
        int length;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &length);
        char* buffer = new char[length];
        glGetProgramInfoLog(programId, length, NULL, buffer);
        printf("Link log:\n%s\n", buffer);
        delete buffer;
    }
}
