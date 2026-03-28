#pragma once

#include <stdexcept>

#include "Global/pch.hpp"
enum class PlayerTeams
{
	Red = 0,
	Green = 1,
	Blue = 2,
	White = 3,
	Black = 4,
	Yellow = 5,
	Cyan = 6,
	Magenta = 7,
	Orange = 8,
	Purple = 9,
	Pink = 10,
	Gray = 11
};
constexpr static const std::array<PlayerTeams, 12> AllPlayerColors = {
	PlayerTeams::Red,	 PlayerTeams::Green,  PlayerTeams::Blue, PlayerTeams::White,
	PlayerTeams::Black,	 PlayerTeams::Yellow, PlayerTeams::Cyan, PlayerTeams::Magenta,
	PlayerTeams::Orange, PlayerTeams::Purple, PlayerTeams::Pink, PlayerTeams::Gray};
vec3 PlayerTeamsToColor(PlayerTeams color);
std::string PlayerTeamToString(PlayerTeams team);
PlayerTeams PlayerTeamFromString(const std::string_view str);