#include "Furniture.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>

#include <nlohmann/json.hpp>

namespace Furniture
{
	namespace
	{
		constexpr auto TYPE_FOLDER = "Data/F4SE/Plugins/4Stim/Furniture";
		constexpr float DEG = 3.14159265F / 180.0F;

		struct Type
		{
			std::string                    id;
			std::string                    name;
			std::string                    supertype;  // "" = none
			int                            priority = 0;
			std::vector<RE::ENUM_FORM_ID>  forms;            // base object types it can be
			std::vector<std::string>       models;           // any of these in the model path (lowercase)
			std::vector<std::string>       excludeModels;    // none of these
			std::vector<std::string>       keywords;         // any of these on the base object (editor IDs)
			std::vector<std::string>       excludeKeywords;  // none of these
			int                            minMarkers = -1;  // furniture markers, -1 = any
			int                            maxMarkers = -1;
			bool                           useMarker = true;  // place at the first furniture marker
			bool                           ignoreMarker[3] = { false, false, false };
			RE::NiPoint3                   offset;            // in the furniture's own frame
			float                          rotation = 0.0F;   // radians
		};

		std::mutex                  g_lock;
		std::map<std::string, Type> g_types;  // key: lowercase id
		bool                        g_loaded = false;

		std::string Lower(std::string_view a_str)
		{
			std::string out(a_str);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::vector<std::string> Strings(const nlohmann::json& a_entry, const char* a_key, bool a_lower)
		{
			std::vector<std::string> out;
			const auto it = a_entry.find(a_key);
			if (it == a_entry.end() || !it->is_array()) {
				return out;
			}
			for (const auto& s : *it) {
				if (s.is_string()) {
					out.push_back(a_lower ? Lower(s.get<std::string>()) : s.get<std::string>());
				}
			}
			return out;
		}

		std::optional<RE::ENUM_FORM_ID> FormType(std::string_view a_code)
		{
			const auto code = Lower(a_code);
			if (code == "furn") {
				return RE::ENUM_FORM_ID::kFURN;
			}
			if (code == "stat") {
				return RE::ENUM_FORM_ID::kSTAT;
			}
			if (code == "mstt") {
				return RE::ENUM_FORM_ID::kMSTT;
			}
			if (code == "acti") {
				return RE::ENUM_FORM_ID::kACTI;
			}
			if (code == "cont") {
				return RE::ENUM_FORM_ID::kCONT;
			}
			return std::nullopt;
		}

		void LoadLocked()
		{
			g_types.clear();
			g_loaded = true;
			std::error_code                    ec;
			std::vector<std::filesystem::path> files;
			for (const auto& e : std::filesystem::directory_iterator(TYPE_FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					files.push_back(e.path());
				}
			}
			std::ranges::sort(files);  // later files override earlier types of the same id
			for (const auto& path : files) {
				const auto     file = path.filename().string();
				nlohmann::json root;
				try {
					std::ifstream in(path);
					root = nlohmann::json::parse(in, nullptr, true, true);
				} catch (const std::exception& ex) {
					REX::WARN("Furniture: {}: not valid JSON ({})", file, ex.what());
					continue;
				}
				const auto types = root.find("types");
				if (types == root.end() || !types->is_array()) {
					REX::WARN("Furniture: {}: no \"types\" list", file);
					continue;
				}
				for (const auto& entry : *types) {
					Type type;
					type.id = entry.value("id", std::string{});
					if (type.id.empty()) {
						REX::WARN("Furniture: {}: a type without an \"id\", skipped", file);
						continue;
					}
					type.name = entry.value("name", type.id);
					type.supertype = Lower(entry.value("supertype", std::string{}));
					type.priority = entry.value("priority", 0);
					for (const auto& code : Strings(entry, "forms", false)) {
						if (const auto form = FormType(code)) {
							type.forms.push_back(*form);
						} else {
							REX::WARN("Furniture: {}: type \"{}\": unknown form type \"{}\" (FURN, STAT, MSTT, ACTI or CONT)", file, type.id, code);
						}
					}
					if (type.forms.empty()) {
						type.forms.push_back(RE::ENUM_FORM_ID::kFURN);
					}
					type.models = Strings(entry, "models", true);
					type.excludeModels = Strings(entry, "excludeModels", true);
					type.keywords = Strings(entry, "keywords", false);
					type.excludeKeywords = Strings(entry, "excludeKeywords", false);
					type.minMarkers = entry.value("minMarkers", -1);
					type.maxMarkers = entry.value("maxMarkers", -1);
					type.useMarker = entry.value("useMarker", true);
					if (const auto ignore = Strings(entry, "ignoreMarkerAxes", true); !ignore.empty()) {
						for (const auto& axis : ignore) {
							if (axis == "x" || axis == "y" || axis == "z") {
								type.ignoreMarker[axis[0] - 'x'] = true;
							}
						}
					}
					if (const auto off = entry.find("offset"); off != entry.end() && off->is_array() && off->size() == 3) {
						type.offset = { (*off)[0].get<float>(), (*off)[1].get<float>(), (*off)[2].get<float>() };
					}
					type.rotation = entry.value("rotation", 0.0F) * DEG;
					if (type.models.empty() && type.keywords.empty()) {
						REX::INFO("Furniture: {}: type \"{}\" has no \"models\" or \"keywords\": only a supertype for others", file, type.id);
					}
					g_types[Lower(type.id)] = std::move(type);
				}
			}
			REX::INFO("Furniture: {} type(s) from {} file(s)", g_types.size(), files.size());
		}

		void EnsureLoaded()
		{
			if (!g_loaded) {
				LoadLocked();
			}
		}

		const char* ModelOf(RE::TESBoundObject* a_base)
		{
			if (const auto model = a_base ? a_base->As<RE::TESModel>() : nullptr) {
				return model->GetModel();
			}
			return "";
		}

		int MarkerCount(RE::TESBoundObject* a_base)
		{
			const auto furn = a_base ? a_base->As<RE::TESFurniture>() : nullptr;
			return furn ? static_cast<int>(furn->markersArray.size()) : 0;
		}

		bool HasAnyKeyword(RE::TESBoundObject* a_base, const std::vector<std::string>& a_keywords)
		{
			const auto form = a_base ? a_base->As<RE::BGSKeywordForm>() : nullptr;
			if (!form) {
				return false;
			}
			return std::ranges::any_of(a_keywords, [&](const std::string& kw) { return form->HasKeywordString(kw); });
		}

		bool Matches(const Type& a_type, RE::TESBoundObject* a_base, const std::string& a_model)
		{
			if (a_type.models.empty() && a_type.keywords.empty()) {
				return false;  // nothing to recognize it by
			}
			if (std::ranges::find(a_type.forms, a_base->GetFormType()) == a_type.forms.end()) {
				return false;
			}
			const bool modelHit = std::ranges::any_of(a_type.models, [&](const std::string& m) { return a_model.find(m) != std::string::npos; });
			if (!modelHit && !HasAnyKeyword(a_base, a_type.keywords)) {
				return false;
			}
			if (std::ranges::any_of(a_type.excludeModels, [&](const std::string& m) { return a_model.find(m) != std::string::npos; })) {
				return false;
			}
			if (HasAnyKeyword(a_base, a_type.excludeKeywords)) {
				return false;
			}
			const int markers = MarkerCount(a_base);
			if ((a_type.minMarkers >= 0 && markers < a_type.minMarkers) || (a_type.maxMarkers >= 0 && markers > a_type.maxMarkers)) {
				return false;
			}
			return true;
		}

		// g_lock held.
		std::string ClassifyLocked(RE::TESObjectREFR* a_ref)
		{
			if (!a_ref || a_ref->GetDelete() || (a_ref->formFlags & 0x800) != 0) {  // deleted or disabled
				return {};
			}
			const auto base = a_ref->GetObjectReference();
			if (!base) {
				return {};
			}
			const auto  model = Lower(ModelOf(base));
			const Type* best = nullptr;
			for (const auto& [key, type] : g_types) {
				if (Matches(type, base, model) && (!best || type.priority > best->priority)) {
					best = &type;
				}
			}
			return best ? best->id : std::string{};
		}

		bool IsALocked(std::string_view a_type, std::string_view a_wanted)
		{
			auto       key = Lower(a_type);
			const auto wanted = Lower(a_wanted);
			for (int hop = 0; hop < 8 && !key.empty(); ++hop) {
				if (key == wanted) {
					return true;
				}
				const auto it = g_types.find(key);
				if (it == g_types.end()) {
					break;
				}
				key = it->second.supertype;
			}
			return false;
		}
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}

	std::string Classify(RE::TESObjectREFR* a_ref)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return ClassifyLocked(a_ref);
	}

	bool IsA(std::string_view a_type, std::string_view a_wanted)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return IsALocked(a_type, a_wanted);
	}

	std::string Name(std::string_view a_type)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		const auto it = g_types.find(Lower(a_type));
		return it != g_types.end() ? it->second.name : std::string(a_type);
	}

	std::vector<Found> FindNear(const RE::NiPoint3& a_center, float a_radius, float a_maxHeight, bool a_log)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		std::vector<Found> found;
		const auto         tes = RE::TES::GetSingleton();
		if (!tes) {
			return found;
		}
		tes->ForEachReferenceInRange(a_center, a_radius, [&](RE::TESObjectREFR* a_ref) {
			const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
			if (!base) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto formType = base->GetFormType();
			const bool candidate = formType == RE::ENUM_FORM_ID::kFURN || formType == RE::ENUM_FORM_ID::kSTAT ||
			                       formType == RE::ENUM_FORM_ID::kMSTT || formType == RE::ENUM_FORM_ID::kACTI;
			if (!candidate) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto pos = a_ref->GetPosition();
			if (std::fabs(pos.z - a_center.z) > a_maxHeight) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto type = ClassifyLocked(a_ref);
			const auto distance = a_center.GetDistance(pos);
			if (a_log && (formType == RE::ENUM_FORM_ID::kFURN || !type.empty())) {
				const char* code = formType == RE::ENUM_FORM_ID::kFURN ? "FURN" : formType == RE::ENUM_FORM_ID::kSTAT ? "STAT" :
				                   formType == RE::ENUM_FORM_ID::kMSTT ? "MSTT" :
				                                                         "ACTI";
				REX::INFO("Furniture: {:08X} {} \"{}\", {} marker(s), {:.0f} away -> {}", a_ref->GetFormID(), code,
					ModelOf(base), MarkerCount(base), distance, type.empty() ? "(none)" : type);
			}
			if (type.empty()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto it = std::ranges::find_if(found, [&](const Found& f) { return f.type == type; });
			if (it == found.end()) {
				found.push_back({ type, g_types[Lower(type)].name, a_ref->GetFormID(), distance });
			} else if (distance < it->distance) {
				it->ref = a_ref->GetFormID();
				it->distance = distance;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		std::ranges::sort(found, [](const Found& a, const Found& b) { return a.distance < b.distance; });
		return found;
	}

	Spot SpotFor(RE::TESObjectREFR* a_ref, std::string_view a_type)
	{
		Spot spot;
		if (!a_ref) {
			return spot;
		}
		Type type;
		{
			std::scoped_lock lock(g_lock);
			EnsureLoaded();
			if (const auto it = g_types.find(Lower(a_type)); it != g_types.end()) {
				type = it->second;
			}
		}
		const auto  base = a_ref->GetObjectReference();
		const auto  furn = base ? base->As<RE::TESFurniture>() : nullptr;
		const float scale = a_ref->refScale > 0 ? a_ref->refScale / 100.0F : 1.0F;
		const float yaw = a_ref->data.angle.z;

		// In the furniture's own frame: x right, y forward, z up.
		RE::NiPoint3 local;
		float        localHeading = 0.0F;
		if (type.useMarker && furn && !furn->markersArray.empty()) {
			const auto& marker = furn->markersArray[0];
			local = marker.position;
			localHeading = marker.heading;
			if (type.ignoreMarker[0]) {
				local.x = 0.0F;
			}
			if (type.ignoreMarker[1]) {
				local.y = 0.0F;
			}
			if (type.ignoreMarker[2]) {
				local.z = 0.0F;
			}
		}
		local = local + type.offset;
		local = local * scale;

		const float c = std::cos(yaw);
		const float s = std::sin(yaw);
		const auto  origin = a_ref->GetPosition();
		spot.position = { origin.x + local.x * c + local.y * s, origin.y - local.x * s + local.y * c, origin.z + local.z };
		spot.heading = yaw + localHeading + type.rotation;
		REX::INFO("Furniture: spot on {:08X} ({}): ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg (marker {}: local ({:.1f}, {:.1f}, {:.1f}), {:.1f} deg)",
			a_ref->GetFormID(), a_type, spot.position.x, spot.position.y, spot.position.z, spot.heading / DEG,
			furn && !furn->markersArray.empty() ? "0" : "none", local.x, local.y, local.z, localHeading / DEG);
		return spot;
	}
}
