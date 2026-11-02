#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

// YAML scene files (docs/SCENES.md, "YAML"): 4Stim's addition, OStim reads
// JSON only. A .yaml / .yml file is read into the same JSON value a .json
// file with the same fields gives, so everything after reading is shared.
//
// Plain (unquoted) values are typed as JSON would type them: true / false,
// null / ~ / nothing, whole numbers written in decimal, numbers with a point
// or an exponent; anything else is text. Hex numbers (0x000833) and numbers
// with leading zeros stay text, so form IDs keep their spelling. Quoted
// values are always text.
namespace YamlJSON
{
	// The file's first document, or nullopt with a_error ("line 4, column 7:
	// ...") when it isn't valid YAML.
	std::optional<nlohmann::json> Read(const std::filesystem::path& a_path, std::string& a_error);

	// Whether a_path is a .yaml / .yml file.
	bool IsYAML(const std::filesystem::path& a_path);
}
