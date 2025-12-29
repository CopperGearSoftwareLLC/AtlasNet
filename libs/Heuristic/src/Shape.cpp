#include "Shape.hpp"
#include <iostream>
Shape Shape::FromString(const std::string &data)
{
    Shape shape;

    // Parse metadata first
    size_t shapeIdStart = data.find("shape_id:");
    if (shapeIdStart != std::string::npos)
    {
        size_t shapeIdEnd = data.find(";", shapeIdStart);
        if (shapeIdEnd != std::string::npos)
        {
            std::string shapeId = data.substr(shapeIdStart + 9, shapeIdEnd - (shapeIdStart + 9));
            
        }
    }

    // Find the triangles section
    size_t trianglesStart = data.find("triangles:");
    if (trianglesStart == std::string::npos)
    {
        std::cerr << "Invalid shape data format - no triangles section" << std::endl;
        throw "Invalid shape";
    }

    // Parse the triangle vertices
    size_t pos = trianglesStart + 9; // Skip "triangles:"
    while (pos < data.length())
    {
        // Find next triangle marker
        pos = data.find("triangle:", pos);
        if (pos == std::string::npos)
            break;
        pos += 9; // Skip "triangle:"

        Triangle triangle;
        bool validTriangle = true;

        // Parse 3 vertices for this triangle
        for (int i = 0; i < 3; i++)
        {
            size_t vStart = data.find("v(", pos);
            if (vStart == std::string::npos)
            {
                validTriangle = false;
                break;
            }

            size_t comma = data.find(",", vStart);
            size_t end = data.find(")", vStart);
            if (comma == std::string::npos || end == std::string::npos)
            {
                validTriangle = false;
                break;
            }

            try
            {
                float x = std::stof(data.substr(vStart + 2, comma - (vStart + 2)));
                float y = std::stof(data.substr(comma + 1, end - (comma + 1)));
                triangle[i] = vec2(x, y);
                //logger->DebugFormatted("Parsed vertex {}: ({}, {})", i, x, y);
                pos = end + 1;
            }
            catch (const std::exception &e)
            {
                //logger->ErrorFormatted("Error parsing vertex: {}", e.what());
                validTriangle = false;
                break;
            }
        }

        if (validTriangle)
        {
            shape.triangles.push_back(triangle);
            //logger->Debug("Added triangle to shape");
        }
        else
        {
            //logger->Error("Failed to parse triangle vertices");
        }
    }

    // Assign the shape to our member variable

    
    return shape;
}
