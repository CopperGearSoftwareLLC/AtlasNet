#pragma once
#include <vector>
#include "../lib_include.h"
#include <string>
namespace ImguiBase
{
    struct Vertex
    {
        vec3 Position;
        vec3 Normal;
        vec2 TexCoord;
        vec4 Color;
    };
    class Mesh
    {
    protected:
        std::vector<Vertex> verticies;
        std::vector<GLuint> indicies;

    public:
        Mesh() {}
        const decltype(verticies) &GetVerticies() const { return verticies; }
        virtual void SetVerticies(const decltype(verticies) &_v) { verticies = _v; }
        const decltype(indicies) &GetIndicies() const { return indicies; }
        virtual void SetIndicies(const decltype(indicies) &_i) { indicies = _i; }

        void Centerize();
        void FitToBounds(const float size);
        void CalculateNormals();

        static Mesh FromOBJ(const std::string &filepath,bool& colorsFound);
    };

    class GL_Mesh : public Mesh
    {
        GLuint vao = -1, vbo = -1, ebo = -1;
        const GLuint pos_binding = 0, normal_binding = 1, texcoord_binding = 2, color_binding = 3;

        void Init();

    public:
        GL_Mesh();
        GL_Mesh(const Mesh &mesh);
        GL_Mesh &operator=(const Mesh &mesh);
        ~GL_Mesh();
        void Use() const { glBindVertexArray(vao); }
        void UnUse() const { glBindVertexArray(0); }
        void SetVerticies(const decltype(verticies) &_v) override;
        void SetIndicies(const decltype(indicies) &_i) override;

        void Draw() const;
    };

};