#include "ComputeOp.hpp"

ComputeOp::ComputeOp(std::string ComputeSrcPath)
{
    std::vector<std::pair<Shader_Stage, std::string>> files = {{eCompute, ComputeSrcPath}};
    computeShader.emplace(files, false);
}

void ComputeOp::Prepare()
{
    computeShader->Use();

    // Bind input textures (read-only)
    for (size_t i = 0; i < InputTextures.size(); ++i)
    {
        GLuint unit = i;
        glBindImageTexture(unit, InputTextures[i]->GetHandle(), 0, GL_FALSE, 0, GL_READ_ONLY, InputTextures[i]->GetInternalFormat());
    }

    // Bind output textures (write-only)
    for (size_t i = 0; i < OutputTextures.size(); ++i)
    {
        GLuint unit = static_cast<GLuint>(i + InputTextures.size());
        glBindImageTexture(unit, OutputTextures[i]->GetHandle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, OutputTextures[i]->GetInternalFormat());
    }

    // Bind SSBOs
    const GLuint ssboBase = InputTextures.size() + OutputTextures.size();
    for (size_t i = 0; i < InputSSBOs.size(); ++i)
        InputSSBOs[i]->BindBase(ssboBase + i);

    for (size_t i = 0; i < OutputSSBOs.size(); ++i)
        OutputSSBOs[i]->BindBase(ssboBase + static_cast<GLuint>(InputSSBOs.size() + i));

    computeShader->Unuse();
}
void ComputeOp::Dispatch(uvec3 WorkGroupCount)
{
    Prepare();
    computeShader->Use();

    // Dispatch compute

    glDispatchCompute(WorkGroupCount.x, WorkGroupCount.y, WorkGroupCount.z);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    computeShader->Unuse();
}

void ComputeOp::Dispatch16_16_1(uvec3 WorkGroupCount)
{
    Prepare();
    computeShader->Use();

    // Dispatch compute
    WorkGroupCount.x += 15u;
    WorkGroupCount.y += 15u;
    glDispatchCompute(WorkGroupCount.x / 16, WorkGroupCount.y / 16, WorkGroupCount.z / 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    computeShader->Unuse();
}
