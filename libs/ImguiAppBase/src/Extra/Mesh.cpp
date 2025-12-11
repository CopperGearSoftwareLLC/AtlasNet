#include "Mesh.h"
#include <fstream>
#include <iostream>
#include <map>
#include <array>
#include <algorithm>
#include <filesystem>
#define OBJ_JUMP_VALUE(x, y) (uint16_t(x) << 8) + y

static std::vector<std::string> split(const std::string &str, char divider, bool AddEmpties = false)
{
    std::vector<std::string> result;
    std::string temp;

    for (char ch : str)
    {
        if (ch == divider)
        {
            if (!temp.empty() || AddEmpties)
            {
                result.push_back(temp);
                temp.clear();
            }
        }
        else
        {
            temp += ch; // Add character to the current substring
        }
    }

    if (!temp.empty())
    {
        result.push_back(temp); // Add the last substring
    }

    return result;
}
namespace ImguiBase
{
    void Mesh::Centerize()
    {
        vec3 pos = {0, 0, 0};
        for (const Vertex &v : verticies)
            pos += v.Position;
        pos /= verticies.size();
        for (auto &v : verticies)
            v.Position -= pos;

        SetVerticies(GetVerticies());
    }

    void Mesh::FitToBounds(const float size)
    {
        vec3 minSize, maxSize;
        for (const auto &v : verticies)
        {
            minSize = min(v.Position, minSize);
            maxSize = max(v.Position, maxSize);
        }
        float largest = -INFINITY;
        for (int i = 0; i < 3; i++)
            largest = max(maxSize[i], largest);
        float smallest = INFINITY;
        for (int i = 0; i < 3; i++)
            smallest = min(minSize[i], smallest);

        const float finalScale = abs(largest) >= abs(smallest) ? abs(largest) : abs(smallest);

        for (auto &v : verticies)
        {
            v.Position /= (finalScale / size);
        }
        SetVerticies(GetVerticies());
    }

    void Mesh::CalculateNormals()
    {
        for (auto &v : verticies)
            v.Normal = {0, 0, 0};

        for (int i = 0; i < indicies.size(); i += 3)
        {
            vec3 verts[] = {
                verticies[indicies[i]].Position,
                verticies[indicies[i + 1]].Position,
                verticies[indicies[i + 2]].Position};

            vec3 edges[3] = {// AC AB BC
                             normalize(verts[0] - verts[2]),
                             normalize(verts[0] - verts[1]),
                             normalize(verts[1] - verts[2])};
            const vec3 normal = normalize(cross(edges[1], edges[0]));
            for (int v = 0; v < 3; v++)
            {
                Vertex &vert = verticies[indicies[i + v]];
                vert.Normal += normal * length(cross(edges[v], edges[(v + 1) % 3]));
            }
        }
        for (auto &v : verticies)
            v.Normal = normalize(v.Normal);

        SetVerticies(GetVerticies());
    }

    Mesh ImguiBase::Mesh::FromOBJ(const std::string &filepath, bool &colorsFound)
    {
        std::vector<vec3> Positions;
        std::vector<vec3> Normals;
        std::vector<vec4> Colors;
        std::vector<vec2> TexCoords;

        std::map<std::array<uint32_t, 3>, size_t> vertexEntries;
        std::vector<Vertex> verticies;

        std::vector<uint32_t> indicies;
        colorsFound = false;
        std::ifstream file(filepath); // Open the file
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open file");
        }

        std::string line;

        while (std::getline(file, line))
        { // Read each line
            if (line.empty())
                continue;

            std::replace(line.begin(), line.end(), '\t', ' ');
            const uint16_t jumpValue = OBJ_JUMP_VALUE(line[0], line[1]);

            switch (jumpValue)
            {
            case OBJ_JUMP_VALUE('v', ' '): // POSITION + COLOR
            {
                const std::vector<std::string> splits = split(line, ' ');
                {
                    vec3 pos;
                    for (int i = 0; i < 3; i++)
                        pos[i] = std::stof(splits[i + 1]);
                    Positions.push_back(pos);
                }
                if (splits.size() <= 4)
                    continue;
                {
                    colorsFound = true;
                    vec4 color;
                    color.w = 1;
                    // w component is optional as specified by the obj file format
                    const int compCount = splits.size() == 7 ? 3 : 4;
                    for (int i = 0; i < compCount; i++)
                        color[i] = std::stof(splits[i + 4]);

                    Colors.push_back(color);
                }
            }
            /* code */
            break;
            case OBJ_JUMP_VALUE('v', 'n'): // NORMALS
            {

                const std::vector<std::string> splits = split(line, ' ');
                vec3 normal;
                for (int i = 0; i < 3; i++)
                    normal[i] = std::stof(splits[i + 1]);
                Normals.push_back(normal);
            }
            /* code */
            break;
            case OBJ_JUMP_VALUE('v', 't'): // TexCoords
            {

                const std::vector<std::string> splits = split(line, ' ');
                vec2 texCoord;
                for (int i = 0; i < 2; i++)
                    texCoord[i] = std::stof(splits[i + 1]);
                TexCoords.push_back(texCoord);
            }
            /* code */
            break;
            case OBJ_JUMP_VALUE('f', ' '): // COMPOSITION
            {

                const std::vector<std::string> splits = split(line, ' ');

                for (int i = 0; i < 3; i++)
                {
                    const std::vector<std::string> comps = split(splits[i + 1], '/', true);
                    uint32_t posInd = comps[0].empty() ? -1 : std::stoi(comps[0]) - 1;
                    uint32_t texInd = comps.size() <= 1 || comps[1].empty() ? -1 : std::stoi(comps[1]) - 1;
                    uint32_t norInd = comps.size() <= 2 || comps[2].empty() ? -1 : std::stoi(comps[2]) - 1;

                    const decltype(vertexEntries)::iterator entry = vertexEntries.find({posInd, texInd, norInd});
                    if (entry != vertexEntries.end())
                    {
                        indicies.push_back((*entry).second);
                        continue;
                    }
                    Vertex v;
                    v.Position = {0, 0, 0};
                    v.Normal = {0, 0, 0};
                    v.Color = {0, 0, 0, 0};
                    v.TexCoord = {0, 0};

                    // vertex_index/texture_index/normal_index
                    if (posInd != -1)
                        v.Position = Positions[posInd];
                    if (texInd != -1)
                        v.TexCoord = TexCoords[texInd];
                    if (norInd != -1)
                        v.Normal = Normals[norInd];

                    uint32_t vertIndex = verticies.size();
                    verticies.push_back(v);
                    vertexEntries[{posInd, texInd, norInd}] = vertIndex;
                    indicies.push_back(vertIndex);
                }
            }
            /* code */
            break;

            default:
                break;
            }
        }

        file.close(); // Close the file

        Mesh mesh;
        mesh.SetVerticies(verticies);
        mesh.SetIndicies(indicies);
        return mesh;
    }
    void GL_Mesh::Init()
    {
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glCreateBuffers(1, &vbo);
        glCreateBuffers(1, &ebo);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        if (!verticies.empty())
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verticies.size(), verticies.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        if (!indicies.empty())
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indicies.size(), indicies.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(pos_binding);
        glVertexAttribPointer(pos_binding, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);

        glEnableVertexAttribArray(normal_binding);
        glVertexAttribPointer(normal_binding, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void *)sizeof(vec3));

        glEnableVertexAttribArray(texcoord_binding);
        glVertexAttribPointer(texcoord_binding, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(sizeof(vec3) * 2));

        glEnableVertexAttribArray(color_binding);
        glVertexAttribPointer(color_binding, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(sizeof(vec3) * 2 + sizeof(vec2)));

        glBindVertexArray(0);
    }
    GL_Mesh::GL_Mesh()
    {
        Init();
    }

    GL_Mesh::GL_Mesh(const Mesh &mesh) : Mesh(mesh)
    {
        Init();
    }

    GL_Mesh &GL_Mesh::operator=(const Mesh &mesh)
    {
        SetVerticies(mesh.GetVerticies());
        SetIndicies(mesh.GetIndicies());
        return *this;
    }

    GL_Mesh::~GL_Mesh()
    {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    }

    void GL_Mesh::SetVerticies(const decltype(verticies) &_v)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        if (verticies.size() != _v.size())
            glBufferData(GL_ARRAY_BUFFER, _v.size() * sizeof(Vertex), _v.data(), GL_STATIC_DRAW);
        else
            glBufferSubData(GL_ARRAY_BUFFER, 0, _v.size() * sizeof(Vertex), _v.data());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        verticies = _v;
    }
    void GL_Mesh::SetIndicies(const decltype(indicies) &_i)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

        if (indicies.size() != _i.size())
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, _i.size() * sizeof(GLuint), _i.data(), GL_STATIC_DRAW);
        else
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, _i.size() * sizeof(GLuint), _i.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        indicies = _i;
    }

    void GL_Mesh::Draw() const
    {
        Use();
        glDrawElements(GL_TRIANGLES, indicies.size(), GL_UNSIGNED_INT, 0);
        UnUse();
    }
}