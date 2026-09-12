#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "json.hpp"

using json = nlohmann::json;

struct Settings {
	bool ShowMenu = true;
	bool Esp_box = false;
	bool cornered_box = false;
	bool Esp_line = false;
	bool Aimbot = false;
	bool playerTrail = false;
	bool Esp_Distance = false;
	bool fovcircle = false;
	bool square_fov = false;
	bool fovcirclefilled = false;
	bool fillbox = false;
	bool lineheadesp = false;
	bool crosshair = false;
	bool Esp_skeleton = false;
	bool skeletonAim = false;
	bool rainbowMode = false;
	bool rainbowBox = false;
	bool rainbowTrail = false;
	bool rainbowFov = false;
	bool rainbowSnaplines = false;
	bool sidewardsEnabled = false;
	bool shaderLabelOverlay = false;
	bool shaderIconOverlay = true;
	bool depthVisualization = false;
	bool trailFade = true;
	bool espDeathCheck = true;
	bool espTeamCheck = true;
	bool streamerMode = false;
	bool radarEnabled = false;
	bool box3dEnabled = false;
	bool offscreenArrows = false;
	bool closestRing = false;
	bool distanceFade = false;
	bool priorityHighlight = false;
	bool lowHpPriority = false;
	bool showDbno = false;
	bool enemyCountHud = false;
	bool roundToasts = false;
	bool sessionStatsHud = false;
	bool multiBoneAim = false;
	bool stickyAim = false;
	bool nearestVector = false;
	bool corpseEsp = false;

	float ChangerFOV = 80.0f;
	float AimFOV = 150.0f;
	float smooth = 5.0f;
	float skeletonThickness = 1.5f;
	float boxThickness = 1.5f;
	float snaplineThickness = 1.0f;
	float trailThickness = 1.5f;
	float fovCircleThickness = 1.0f;
	float crosshairSize = 8.0f;
	float sidewardsValue = 10.0f;
	float g_weatherIntensity = 1.0f;
	float g_weatherWind = 0.3f;
	float radarSize = 220.0f;
	float radarRange = 60.0f;
	float box3dThickness = 1.4f;
	float box3dHalfWidth = 0.35f;
	float box3dHeight = 1.75f;
	float offscreenRadius = 220.0f;
	float distanceFadeNear = 30.0f;
	float distanceFadeFar = 200.0f;

	int trailLength = 60;
	int snaplineOrigin = 2;
	int trailUpdateMs = 33;
	int g_weatherMode = 3;
	int aimTargetMode = 0;
	int radarCorner = 1;
	int enemyCountCorner = 0;
	int aimCurve = 0;
	int VisDist = 250;
	int hitboxpos = 0;
	int aimKey = 0;

	std::array<float, 4> espBoxColor = { 1.0f, 0.0f, 0.0f, 1.0f };
	std::array<float, 4> espSnaplineColor = { 1.0f, 1.0f, 0.0f, 1.0f };
	std::array<float, 4> espTrailColor = { 0.0f, 1.0f, 1.0f, 0.7f };
	std::array<float, 4> espDistanceColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	std::array<float, 4> fovCircleColor = { 1.0f, 1.0f, 1.0f, 0.7f };
	std::array<float, 4> crosshairColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	std::array<float, 4> aimbotTargetColor = { 1.0f, 0.0f, 1.0f, 1.0f };
	std::array<float, 4> filledBoxColor = { 1.0f, 0.0f, 0.0f, 0.15f };
	std::array<float, 4> espSkeletonColor = { 1.0f, 1.0f, 1.0f, 0.85f };
	std::array<float, 4> radarBgColor = { 0.04f, 0.04f, 0.04f, 0.80f };
	std::array<float, 4> radarEnemyColor = { 0.85f, 0.30f, 0.30f, 1.0f };
	std::array<float, 4> radarLocalColor = { 0.79f, 0.66f, 0.43f, 1.0f };
	std::array<float, 4> box3dColor = { 0.79f, 0.66f, 0.43f, 1.0f };
	std::array<float, 4> offscreenColor = { 0.85f, 0.30f, 0.30f, 0.9f };
	std::array<float, 4> closestRingColor = { 0.79f, 0.66f, 0.43f, 1.0f };
	std::array<float, 4> priorityColor = { 0.99f, 0.42f, 0.42f, 1.0f };
	std::array<float, 4> lowHpColor = { 1.0f, 0.85f, 0.20f, 1.0f };
	std::array<float, 4> dbnoColor = { 0.45f, 0.45f, 0.45f, 0.6f };
	std::array<float, 4> nearestVectorColor = { 0.79f, 0.66f, 0.43f, 0.55f };
	std::array<float, 4> corpseColor = { 0.55f, 0.32f, 0.32f, 0.7f };
};

inline void to_json(json& j, const Settings& s) {
	j = json{
		{ "version", 1 },
		{ "ShowMenu", s.ShowMenu }, { "Esp_box", s.Esp_box }, { "cornered_box", s.cornered_box },
		{ "Esp_line", s.Esp_line }, { "Aimbot", s.Aimbot }, { "playerTrail", s.playerTrail },
		{ "Esp_Distance", s.Esp_Distance }, { "fovcircle", s.fovcircle }, { "square_fov", s.square_fov },
		{ "fovcirclefilled", s.fovcirclefilled }, { "fillbox", s.fillbox }, { "lineheadesp", s.lineheadesp },
		{ "crosshair", s.crosshair }, { "Esp_skeleton", s.Esp_skeleton }, { "skeletonAim", s.skeletonAim },
		{ "rainbowMode", s.rainbowMode }, { "rainbowBox", s.rainbowBox }, { "rainbowTrail", s.rainbowTrail },
		{ "rainbowFov", s.rainbowFov }, { "rainbowSnaplines", s.rainbowSnaplines },
		{ "sidewardsEnabled", s.sidewardsEnabled }, { "shaderLabelOverlay", s.shaderLabelOverlay },
		{ "shaderIconOverlay", s.shaderIconOverlay }, { "depthVisualization", s.depthVisualization },
		{ "trailFade", s.trailFade }, { "espDeathCheck", s.espDeathCheck }, { "espTeamCheck", s.espTeamCheck },
		{ "streamerMode", s.streamerMode }, { "radarEnabled", s.radarEnabled }, { "box3dEnabled", s.box3dEnabled },
		{ "offscreenArrows", s.offscreenArrows }, { "closestRing", s.closestRing }, { "distanceFade", s.distanceFade },
		{ "priorityHighlight", s.priorityHighlight }, { "lowHpPriority", s.lowHpPriority }, { "showDbno", s.showDbno },
		{ "enemyCountHud", s.enemyCountHud }, { "roundToasts", s.roundToasts }, { "sessionStatsHud", s.sessionStatsHud },
		{ "multiBoneAim", s.multiBoneAim }, { "stickyAim", s.stickyAim }, { "nearestVector", s.nearestVector },
		{ "corpseEsp", s.corpseEsp },
		{ "ChangerFOV", s.ChangerFOV }, { "AimFOV", s.AimFOV }, { "smooth", s.smooth },
		{ "skeletonThickness", s.skeletonThickness }, { "boxThickness", s.boxThickness },
		{ "snaplineThickness", s.snaplineThickness }, { "trailThickness", s.trailThickness },
		{ "fovCircleThickness", s.fovCircleThickness }, { "crosshairSize", s.crosshairSize },
		{ "sidewardsValue", s.sidewardsValue }, { "g_weatherIntensity", s.g_weatherIntensity },
		{ "g_weatherWind", s.g_weatherWind }, { "radarSize", s.radarSize }, { "radarRange", s.radarRange },
		{ "box3dThickness", s.box3dThickness }, { "box3dHalfWidth", s.box3dHalfWidth }, { "box3dHeight", s.box3dHeight },
		{ "offscreenRadius", s.offscreenRadius }, { "distanceFadeNear", s.distanceFadeNear }, { "distanceFadeFar", s.distanceFadeFar },
		{ "trailLength", s.trailLength }, { "snaplineOrigin", s.snaplineOrigin }, { "trailUpdateMs", s.trailUpdateMs },
		{ "g_weatherMode", s.g_weatherMode }, { "aimTargetMode", s.aimTargetMode }, { "radarCorner", s.radarCorner },
		{ "enemyCountCorner", s.enemyCountCorner }, { "aimCurve", s.aimCurve }, { "VisDist", s.VisDist },
		{ "hitboxpos", s.hitboxpos }, { "aimKey", s.aimKey },
		{ "espBoxColor", s.espBoxColor }, { "espSnaplineColor", s.espSnaplineColor },
		{ "espTrailColor", s.espTrailColor }, { "espDistanceColor", s.espDistanceColor },
		{ "fovCircleColor", s.fovCircleColor }, { "crosshairColor", s.crosshairColor },
		{ "aimbotTargetColor", s.aimbotTargetColor }, { "filledBoxColor", s.filledBoxColor },
		{ "espSkeletonColor", s.espSkeletonColor }, { "radarBgColor", s.radarBgColor },
		{ "radarEnemyColor", s.radarEnemyColor }, { "radarLocalColor", s.radarLocalColor },
		{ "box3dColor", s.box3dColor }, { "offscreenColor", s.offscreenColor },
		{ "closestRingColor", s.closestRingColor }, { "priorityColor", s.priorityColor },
		{ "lowHpColor", s.lowHpColor }, { "dbnoColor", s.dbnoColor },
		{ "nearestVectorColor", s.nearestVectorColor }, { "corpseColor", s.corpseColor }
	};
}

template <typename T>
inline void ReadSetting(const json& j, const char* key, T& value) {
	const auto it = j.find(key);
	if (it == j.end() || it->is_null()) return;
	try {
		it->get_to(value);
	} catch (...) {
		// Keep the current/default value when a single setting is malformed.
	}
}

inline void from_json(const json& j, Settings& s) {
	if (!j.is_object()) return;
#define READ_SETTING(name) ReadSetting(j, #name, s.name)
	READ_SETTING(ShowMenu); READ_SETTING(Esp_box); READ_SETTING(cornered_box); READ_SETTING(Esp_line); READ_SETTING(Aimbot);
	READ_SETTING(playerTrail); READ_SETTING(Esp_Distance); READ_SETTING(fovcircle); READ_SETTING(square_fov); READ_SETTING(fovcirclefilled);
	READ_SETTING(fillbox); READ_SETTING(lineheadesp); READ_SETTING(crosshair); READ_SETTING(Esp_skeleton); READ_SETTING(skeletonAim);
	READ_SETTING(rainbowMode); READ_SETTING(rainbowBox); READ_SETTING(rainbowTrail); READ_SETTING(rainbowFov); READ_SETTING(rainbowSnaplines);
	READ_SETTING(sidewardsEnabled); READ_SETTING(shaderLabelOverlay); READ_SETTING(shaderIconOverlay); READ_SETTING(depthVisualization);
	READ_SETTING(trailFade); READ_SETTING(espDeathCheck); READ_SETTING(espTeamCheck); READ_SETTING(streamerMode); READ_SETTING(radarEnabled);
	READ_SETTING(box3dEnabled); READ_SETTING(offscreenArrows); READ_SETTING(closestRing); READ_SETTING(distanceFade); READ_SETTING(priorityHighlight);
	READ_SETTING(lowHpPriority); READ_SETTING(showDbno); READ_SETTING(enemyCountHud); READ_SETTING(roundToasts); READ_SETTING(sessionStatsHud);
	READ_SETTING(multiBoneAim); READ_SETTING(stickyAim); READ_SETTING(nearestVector); READ_SETTING(corpseEsp);
	READ_SETTING(ChangerFOV); READ_SETTING(AimFOV); READ_SETTING(smooth); READ_SETTING(skeletonThickness); READ_SETTING(boxThickness);
	READ_SETTING(snaplineThickness); READ_SETTING(trailThickness); READ_SETTING(fovCircleThickness); READ_SETTING(crosshairSize); READ_SETTING(sidewardsValue);
	READ_SETTING(g_weatherIntensity); READ_SETTING(g_weatherWind); READ_SETTING(radarSize); READ_SETTING(radarRange); READ_SETTING(box3dThickness);
	READ_SETTING(box3dHalfWidth); READ_SETTING(box3dHeight); READ_SETTING(offscreenRadius); READ_SETTING(distanceFadeNear); READ_SETTING(distanceFadeFar);
	READ_SETTING(trailLength); READ_SETTING(snaplineOrigin); READ_SETTING(trailUpdateMs); READ_SETTING(g_weatherMode); READ_SETTING(aimTargetMode);
	READ_SETTING(radarCorner); READ_SETTING(enemyCountCorner); READ_SETTING(aimCurve); READ_SETTING(VisDist); READ_SETTING(hitboxpos); READ_SETTING(aimKey);
	READ_SETTING(espBoxColor); READ_SETTING(espSnaplineColor); READ_SETTING(espTrailColor); READ_SETTING(espDistanceColor); READ_SETTING(fovCircleColor);
	READ_SETTING(crosshairColor); READ_SETTING(aimbotTargetColor); READ_SETTING(filledBoxColor); READ_SETTING(espSkeletonColor); READ_SETTING(radarBgColor);
	READ_SETTING(radarEnemyColor); READ_SETTING(radarLocalColor); READ_SETTING(box3dColor); READ_SETTING(offscreenColor); READ_SETTING(closestRingColor);
	READ_SETTING(priorityColor); READ_SETTING(lowHpColor); READ_SETTING(dbnoColor); READ_SETTING(nearestVectorColor); READ_SETTING(corpseColor);
#undef READ_SETTING
}

inline bool SaveSettings(const Settings& settings, const std::filesystem::path& path) {
	try {
		const auto parent = path.parent_path();
		if (!parent.empty()) std::filesystem::create_directories(parent);

		auto temporaryPath = path;
		temporaryPath += ".tmp";
		std::ofstream out(temporaryPath, std::ios::out | std::ios::trunc);
		if (!out.good()) return false;
		out << json(settings).dump(4);
		out.flush();
		if (!out.good()) {
			out.close();
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
			return false;
		}
		out.close();
		if (out.fail()) {
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
			return false;
		}

#ifdef _WIN32
		if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
			return false;
		}
#else
		std::error_code replaceError;
		std::filesystem::rename(temporaryPath, path, replaceError);
		if (replaceError) {
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
			return false;
		}
#endif
		return true;
	} catch (...) {
		return false;
	}
}

inline bool LoadSettings(const std::filesystem::path& path, Settings& settings) {
	try {
		std::ifstream in(path);
		if (!in.good()) return false;
		const json j = json::parse(in, nullptr, false);
		if (j.is_discarded() || !j.is_object()) return false;
		Settings loaded = settings;
		from_json(j, loaded);
		settings = loaded;
		return true;
	} catch (...) {
		return false;
	}
}

inline Settings LoadSettings(const std::filesystem::path& path) {
	Settings settings;
	LoadSettings(path, settings);
	return settings;
}

inline std::filesystem::path ConfigDirectory() {
#ifdef _WIN32
	if (const wchar_t* localAppData = _wgetenv(L"LOCALAPPDATA"); localAppData && *localAppData)
		return std::filesystem::path(localAppData) / "R6 Paster Six external" / "configs";

	wchar_t executablePath[32768] = {};
	const DWORD length = GetModuleFileNameW(nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
	if (length > 0 && length < std::size(executablePath))
		return std::filesystem::path(executablePath).parent_path() / "configs";
#else
	if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME"); xdgConfigHome && *xdgConfigHome)
		return std::filesystem::path(xdgConfigHome) / "r6-paster-six-external" / "configs";
	if (const char* userHome = std::getenv("HOME"); userHome && *userHome)
		return std::filesystem::path(userHome) / ".config" / "r6-paster-six-external" / "configs";
#endif
	return std::filesystem::temp_directory_path() / "r6-paster-six-external" / "configs";
}

inline bool IsValidConfigProfileName(std::string_view name) {
	if (name.empty() || name.size() > 48 || name == "." || name == "..") return false;
	return std::all_of(name.begin(), name.end(), [](unsigned char c) {
		return std::isalnum(c) || c == '_' || c == '-';
	});
}

inline std::filesystem::path ConfigProfilePath(std::string_view name) {
	if (!IsValidConfigProfileName(name)) return {};
	return ConfigDirectory() / (std::string(name) + ".json");
}

inline bool SaveConfigProfile(std::string_view name, const Settings& settings) {
	const auto path = ConfigProfilePath(name);
	return !path.empty() && SaveSettings(settings, path);
}

inline bool LoadConfigProfile(std::string_view name, Settings& settings) {
	const auto path = ConfigProfilePath(name);
	return !path.empty() && LoadSettings(path, settings);
}

inline bool DeleteConfigProfile(std::string_view name) {
	try {
		const auto path = ConfigProfilePath(name);
		return !path.empty() && std::filesystem::remove(path);
	} catch (...) {
		return false;
	}
}

inline std::vector<std::string> ListConfigProfiles() {
	std::vector<std::string> profiles;
	try {
		const auto directory = ConfigDirectory();
		if (!std::filesystem::exists(directory)) return profiles;
		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
			const std::string name = entry.path().stem().string();
			if (IsValidConfigProfileName(name)) profiles.push_back(name);
		}
		std::sort(profiles.begin(), profiles.end());
	} catch (...) {
	}
	return profiles;
}
