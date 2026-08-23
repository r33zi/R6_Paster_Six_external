#pragma once
#include <filesystem>
#include <fstream>
#include <array>
#include "json.hpp"

using json = nlohmann::json;

struct Settings {
	bool ShowMenu = true;
	bool Esp_box = false;
	bool cornered_box = false;
	bool Aimbot = false;
	bool playerTrail = false;
	bool Esp_skeleton = false;
	bool fovcircle = false;
	bool square_fov = false;
	bool fillbox = false;
	float ChangerFOV = 80.0f;
	float crosshairSize = 8.0f;
	std::array<float,4> espBoxColor = {1.0f, 0.0f, 0.0f, 1.0f};
	std::array<float,4> filledBoxColor = {1.0f, 0.0f, 0.0f, 0.15f};
	std::array<float,4> crosshairColor = {0.0f, 1.0f, 0.0f, 1.0f};
};

inline void to_json(json& j, const Settings& s) {
	j = json{
		{"ShowMenu", s.ShowMenu},
		{"Esp_box", s.Esp_box},
		{"cornered_box", s.cornered_box},
		{"Aimbot", s.Aimbot},
		{"playerTrail", s.playerTrail},
		{"Esp_skeleton", s.Esp_skeleton},
		{"fovcircle", s.fovcircle},
		{"square_fov", s.square_fov},
		{"fillbox", s.fillbox},
		{"ChangerFOV", s.ChangerFOV},
		{"crosshairSize", s.crosshairSize},
		{"espBoxColor", s.espBoxColor},
		{"filledBoxColor", s.filledBoxColor},
		{"crosshairColor", s.crosshairColor}
	};
}

inline void from_json(const json& j, Settings& s) {
	j.value("ShowMenu", s.ShowMenu);
	j.value("Esp_box", s.Esp_box);
	j.value("cornered_box", s.cornered_box);
	j.value("Aimbot", s.Aimbot);
	j.value("playerTrail", s.playerTrail);
	j.value("Esp_skeleton", s.Esp_skeleton);
	j.value("fovcircle", s.fovcircle);
	j.value("square_fov", s.square_fov);
	j.value("fillbox", s.fillbox);
	j.value("ChangerFOV", s.ChangerFOV);
	j.value("crosshairSize", s.crosshairSize);
	if (j.contains("espBoxColor") && j["espBoxColor"].is_array()) j["espBoxColor"].get_to(s.espBoxColor);
	if (j.contains("filledBoxColor") && j["filledBoxColor"].is_array()) j["filledBoxColor"].get_to(s.filledBoxColor);
	if (j.contains("crosshairColor") && j["crosshairColor"].is_array()) j["crosshairColor"].get_to(s.crosshairColor);
}

inline bool SaveSettings(const Settings& s, const std::string& path = "config.json") {
	try {
		json j = s;
		std::ofstream out(path, std::ios::out | std::ios::trunc);
		if (!out.good()) return false;
		out << j.dump(4);
		out.close();
		return true;
	} catch (...) {
		return false;
	}
}

inline Settings LoadSettings(const std::string& path = "config.json") {
	Settings s;
	try {
		inline_auto: ;
		{
			std::ifstream _f(path);
			if (!_f.good()) return s;
		}
		std::ifstream in(path);
		if (!in.good()) return s;
		json j = json::parse(in, nullptr, false);
		if (j.is_discarded()) return s;
		s = j.get<Settings>();
	} catch (...) {}
	return s;
}
