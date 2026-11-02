#include "YamlJSON.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <fstream>
#include <regex>

#include <yaml-cpp/yaml.h>

namespace YamlJSON
{
	namespace
	{
		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		// A plain scalar, typed as JSON would read it (YAML 1.2's JSON schema,
		// with YAML's own spellings of true / false / null).
		nlohmann::json Plain(const std::string& a_text)
		{
			static const std::regex integer(R"([-+]?(0|[1-9][0-9]*))");
			static const std::regex number(R"([-+]?(\.[0-9]+|(0|[1-9][0-9]*)(\.[0-9]*)?)([eE][-+]?[0-9]+)?)");
			const auto lower = Lower(a_text);
			if (a_text.empty() || a_text == "~" || lower == "null") {
				return nullptr;
			}
			if (lower == "true") {
				return true;
			}
			if (lower == "false") {
				return false;
			}
			if (std::regex_match(a_text, integer)) {
				std::int64_t value = 0;
				const auto  first = a_text.data() + (a_text[0] == '+' ? 1 : 0);
				if (const auto [end, error] = std::from_chars(first, a_text.data() + a_text.size(), value); error == std::errc{}) {
					return value;
				}
			}
			if (std::regex_match(a_text, number)) {
				try {
					return std::stod(a_text);
				} catch (const std::exception&) {
				}
			}
			return a_text;
		}

		nlohmann::json Convert(const YAML::Node& a_node)
		{
			switch (a_node.Type()) {
			case YAML::NodeType::Map:
				{
					auto object = nlohmann::json::object();
					for (const auto& entry : a_node) {
						object[entry.first.as<std::string>()] = Convert(entry.second);
					}
					return object;
				}
			case YAML::NodeType::Sequence:
				{
					auto array = nlohmann::json::array();
					for (const auto& entry : a_node) {
						array.push_back(Convert(entry));
					}
					return array;
				}
			case YAML::NodeType::Scalar:
				// "?" is a plain scalar; "!" a quoted one; "!!str" asks for text.
				if (a_node.Tag() == "?") {
					return Plain(a_node.Scalar());
				}
				return a_node.Scalar();
			default:  // Null, Undefined
				return nullptr;
			}
		}
	}

	bool IsYAML(const std::filesystem::path& a_path)
	{
		const auto extension = Lower(a_path.extension().string());
		return extension == ".yaml" || extension == ".yml";
	}

	std::optional<nlohmann::json> Read(const std::filesystem::path& a_path, std::string& a_error)
	{
		try {
			std::ifstream in(a_path, std::ios::binary);
			if (!in) {
				a_error = "can't be opened";
				return std::nullopt;
			}
			return Convert(YAML::Load(in));
		} catch (const YAML::Exception& e) {
			a_error = e.mark.is_null() ? e.msg : std::format("line {}, column {}: {}", e.mark.line + 1, e.mark.column + 1, e.msg);
		} catch (const std::exception& e) {
			a_error = e.what();
		}
		return std::nullopt;
	}
}
