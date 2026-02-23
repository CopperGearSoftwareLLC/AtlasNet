#pragma once
#include "Debug/Log.hpp"
#include "Heuristic/IBounds.hpp"
#include <vector>
#include <array>
#include <string>
/*
SWIG cannot use glm stuff or any complex containers
*/
struct IBoundsDrawShape
{
	enum class Type
	{
		eInvalid = -1,
		eCircle = 0,
		eRectangle = 1,
		eLine = 2,
		ePolygon = 3,
		eRectImage = 4
	};
	IBounds::BoundsID id;
	std::string owner_id;			 // Claimed shard identity (if claimed)
	Type type = Type::eInvalid;	 // "circle", "rectangle", "line", "polygon", etc.
	float pos_x,pos_y;				 // Position
	std::string color;				 // Stroke color override (optional)

	/*Type specific */


	float radius;  // Circle

	float size_x,size_y;					  // rectangle,RectImage
	std::vector<std::pair<float,float>> verticies;  // Polygon
};

class HeuristicDraw
{
	Log logger = Log("HeuristicDraw");
   public:
	HeuristicDraw() = default;
	void DrawCurrentHeuristic(std::vector<IBoundsDrawShape>& shapes);
};