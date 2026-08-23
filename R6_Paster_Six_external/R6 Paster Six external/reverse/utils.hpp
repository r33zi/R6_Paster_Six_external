#pragma once
#include <string>
#include <fstream>
#include "json.hpp"
using json = nlohmann::json;

inline bool file_exists(const std::string& path) {
	std::ifstream f(path);
	return f.good();
}

std::string ReadFromJson(std::string path, std::string section)
{
	if (!file_exists(path))
		return "";
	std::ifstream file(path);
	json data = json::parse(file);
	return data[section];
}

bool CheckIfJsonKeyExists(std::string path, std::string section)
{
	if (!file_exists(path))
		return false;
	std::ifstream file(path);
	json data = json::parse(file);
	return data.contains(section);
}

bool WriteToJson(std::string path, std::string name, std::string value, bool userpass, std::string name2, std::string value2)
{
	json file;
	if (!userpass)
	{
		file[name] = value;
	}
	else
	{
		file[name] = value;
		file[name2] = value2;
	}

	std::ofstream jsonfile(path, std::ios::out);
	jsonfile << file;
	jsonfile.close();
	if (!file_exists(path))
		return false;

	return true;
}
