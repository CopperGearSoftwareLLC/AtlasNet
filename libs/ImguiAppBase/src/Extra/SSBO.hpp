#pragma once

#include "lib_include.h"
#include <array>

class SSBO 
{
GLuint Handle;
size_t bufferSize;

public:
SSBO(size_t size,void* data = nullptr)
{
    bufferSize = size;
    glCreateBuffers(1,&Handle);
    glNamedBufferData(Handle,size,data,GL_DYNAMIC_DRAW);
}
~SSBO()
{
    glDeleteBuffers(1,&Handle);
}
void BindBase(uint32 Index)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,Index,Handle);
}
auto GetHandle() {return Handle;}
void Insert(void* data, size_t size,size_t offset = 0)
{
    glNamedBufferSubData(Handle,offset,size,data);
}
void GetData(size_t offset, size_t size, void* dest)
{
    glGetNamedBufferSubData(Handle,offset,size,dest);
}
void FillWithZeros()
{
    std::array<GLuint,256> zeros{};
    glNamedBufferSubData(Handle,0,bufferSize,zeros.data());
}
};