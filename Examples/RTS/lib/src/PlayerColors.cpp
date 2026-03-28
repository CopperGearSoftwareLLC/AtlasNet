#include "PlayerColors.hpp"

vec3 PlayerTeamsToColor(PlayerTeams color)
{
	switch (color)
	{
		case PlayerTeams::Red:
			return vec3(1.0f, 0.0f, 0.0f);
		case PlayerTeams::Green:
			return vec3(0.0f, 1.0f, 0.0f);
		case PlayerTeams::Blue:
			return vec3(0.0f, 0.0f, 1.0f);
		case PlayerTeams::White:
			return vec3(1.0f, 1.0f, 1.0f);
		case PlayerTeams::Black:
			return vec3(0.0f, 0.0f, 0.0f);
		case PlayerTeams::Yellow:
			return vec3(1.0f, 1.0f, 0.0f);
		case PlayerTeams::Cyan:
			return vec3(0.0f, 1.0f, 1.0f);
		case PlayerTeams::Magenta:
			return vec3(1.0f, 0.0f, 1.0f);
		case PlayerTeams::Orange:
			return vec3(1.0f, 0.5f, 0.0f);
		case PlayerTeams::Purple:
			return vec3(0.5f, 0.0f, 0.5f);
		case PlayerTeams::Pink:
			return vec3(1.0f, 0.75f, 0.8f);
		case PlayerTeams::Gray:
			return vec3(0.5f, 0.5f, 0.5f);
		default:
			throw std::invalid_argument("Unknown Color");
	}
}
std::string PlayerTeamToString(PlayerTeams team)
{
	switch (team)
	{
		case PlayerTeams::Red:
			return "Red";
		case PlayerTeams::Green:
			return "Green";
		case PlayerTeams::Blue:
			return "Blue";
		case PlayerTeams::White:
			return "White";
		case PlayerTeams::Black:
			return "Black";
		case PlayerTeams::Yellow:
			return "Yellow";
		case PlayerTeams::Cyan:
			return "Cyan";
		case PlayerTeams::Magenta:
			return "Magenta";
		case PlayerTeams::Orange:
			return "Orange";
		case PlayerTeams::Purple:
			return "Purple";
		case PlayerTeams::Pink:
			return "Pink";
		case PlayerTeams::Gray:
			return "Gray";
		default:
			throw std::invalid_argument("Unknown Team");
	}
}
PlayerTeams PlayerTeamFromString(const std::string_view str)
{
	if (str == "Red")
		return PlayerTeams::Red;
	if (str == "Green")
		return PlayerTeams::Green;
	if (str == "Blue")
		return PlayerTeams::Blue;
	if (str == "White")
		return PlayerTeams::White;
	if (str == "Black")
		return PlayerTeams::Black;
	if (str == "Yellow")
		return PlayerTeams::Yellow;
	if (str == "Cyan")
		return PlayerTeams::Cyan;
	if (str == "Magenta")
		return PlayerTeams::Magenta;
	if (str == "Orange")
		return PlayerTeams::Orange;
	if (str == "Purple")
		return PlayerTeams::Purple;
	if (str == "Pink")
		return PlayerTeams::Pink;
	if (str == "Gray")
		return PlayerTeams::Gray;
	throw std::invalid_argument("Unknown Team");
}