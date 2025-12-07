#pragma once

#include <memory>
#include "Extra/Program.h"
#include "Extra/Texture.h"
#include "Extra/SSBO.hpp"
#include <vector>
#include <optional>
class ComputeOp
{
    std::optional<Program> computeShader;
    std::vector<std::shared_ptr<Texture>> InputTextures, OutputTextures;
    std::vector<std::shared_ptr<SSBO>> InputSSBOs, OutputSSBOs;

    void Prepare();
public:
    ComputeOp(std::string ComputeSrcPath);
    void SetInputTextures(const std::vector<std::shared_ptr<Texture>> &_InputTextures) { InputTextures = _InputTextures; }
    void SetOutputTextures(const std::vector<std::shared_ptr<Texture>> &_OutputTextures) { OutputTextures = _OutputTextures; }
    void SetInputSSBOs(const std::vector<std::shared_ptr<SSBO>> &_InputSSBOs) { InputSSBOs = _InputSSBOs; }     // new
    void SetOutputSSBOs(const std::vector<std::shared_ptr<SSBO>> &_OutputSSBOs) { OutputSSBOs = _OutputSSBOs; } // new
    void Dispatch(uvec3 WorkGroupCount);
    void Dispatch16_16_1(uvec3 WorkGroupCount);
    Program &GetProgram() { return *computeShader; }
};