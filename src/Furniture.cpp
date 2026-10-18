#include "Furniture.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>

#include <nlohmann/json.hpp>

#include "Bridge.h"
#include "compat/Fallbacks.h"

namespace Furniture
{
	namespace
	{
		constexpr auto TYPE_FOLDER = "Data/F4SE/Plugins/4Stim/Furniture";
		// OStim's layout: one type per file, id = file name, in subfolders too.
		constexpr auto OSTIM_TYPE_FOLDER = "Data/F4SE/Plugins/4Stim/Furniture Types";
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
			bool                           useMarker = true;  // place at a furniture marker
			int                            markerIndex = 0;   // which one
			// "edge": on an edge of its bounding box (the side nearest the
			// player), facing out from it or in toward it.
			bool                           edge = false;
			// "center": in the middle of its bounding box, turned along its
			// long axis (for lying on a mattress or a sleeping bag).
			bool                           center = false;
			// "wall": no object, the nearest wall found by ray casts around
			// the search center (OStim's wall type is Skyrim's wall-lean
			// markers, which Fallout 4 hardly has), within wallDistance.
			bool                           wall = false;
			float                          wallDistance = 300.0F;
			// Scenes without furniture of their own played on this one (its
			// type chain reaches "none"): at least one actor must have one of
			// needTags, and none of excludeTags (scene actor tags).
			std::vector<std::string>       floorNeedTags;
			std::vector<std::string>       floorExcludeTags;
			bool                           edgeLong = true;      // a long side (else a short one)
			bool                           edgeFacingOut = true;
			float                          edgeInset = 0.0F;     // toward the middle; negative = outside
			bool                           checkWalls = true;    // edge: avoid a side with a wall right outside
			bool                           markerHeight = false; // edge: at the marker's height, if it has one
			bool                           onFloor = false;      // edge: at the bottom of its bounds (where it stands)
			bool                           ignoreMarker[3] = { false, false, false };
			RE::NiPoint3                   offset;            // in the furniture's own frame
			float                          rotation = 0.0F;   // radians
			bool                           listIndividually = false;  // OStim's: list every piece of it, not just the nearest
			float                          multiplyScale = 1.0F;      // OStim's: actors on it are scaled by this
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

		// A keyword given as OStim does ({"mod": "Fallout4.esm", "formid":
		// "0x..."}) or by editor ID: its editor ID, or "".
		std::string KeywordEditorID(const nlohmann::json& a_value)
		{
			if (a_value.is_string()) {
				return a_value.get<std::string>();
			}
			if (!a_value.is_object()) {
				return {};
			}
			const auto mod = a_value.value("mod", std::string{});
			const auto formID = a_value.value("formid", std::string{});
			const auto handler = RE::TESDataHandler::GetSingleton();
			if (mod.empty() || formID.empty() || !handler) {
				return {};
			}
			try {
				const auto id = static_cast<std::uint32_t>(std::stoul(formID, nullptr, 0));
				auto       keyword = handler->LookupForm<RE::BGSKeyword>(id & 0x00FFFFFF, mod);
				if (!keyword) {
					keyword = handler->LookupForm<RE::BGSKeyword>(id & 0x00000FFF, mod);
				}
				return keyword ? std::string(keyword->formEditorID.c_str()) : std::string{};
			} catch (...) {
				return {};
			}
		}

		// One type from its JSON object, with 4Stim's field names and OStim's
		// (offsetX / offsetY / offsetZ, ignoreMarkerOffsetX..., conditions
		// "anykeyword" / "keywordblacklist", listIndividually, multiplyScale).
		Type ParseType(const nlohmann::json& entry, const std::string& id, const std::string& file)
		{
			Type type;
			type.id = id;
			type.name = entry.value("name", type.id);
			if (type.name.starts_with("$")) {
				type.name = type.id;  // an OStim translation key: no translations here
			}
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
			if (const auto conditions = entry.find("conditions"); conditions != entry.end() && conditions->is_array()) {
				for (const auto& condition : *conditions) {
					const auto kind = condition.is_object() ? Lower(condition.value("type", std::string{})) : std::string{};
					const auto keywords = condition.is_object() ? condition.find("keywords") : condition.end();
					if ((kind == "anykeyword" || kind == "keywordblacklist") && keywords != condition.end() && keywords->is_array()) {
						for (const auto& keyword : *keywords) {
							if (auto editorID = KeywordEditorID(keyword); !editorID.empty()) {
								(kind == "anykeyword" ? type.keywords : type.excludeKeywords).push_back(std::move(editorID));
							}
						}
					} else {
						REX::WARN("Furniture: {}: type \"{}\": condition \"{}\" isn't supported in Fallout 4 (only anykeyword and keywordblacklist), ignored", file, type.id, kind);
					}
				}
			}
			type.minMarkers = entry.value("minMarkers", -1);
			type.maxMarkers = entry.value("maxMarkers", -1);
			type.useMarker = entry.value("useMarker", true);
			type.markerIndex = entry.value("marker", 0);
			if (const auto anchor = Lower(entry.value("anchor", std::string{ "marker" })); anchor == "edge") {
				type.edge = true;
			} else if (anchor == "center") {
				type.center = true;
			} else if (anchor == "origin") {
				type.useMarker = false;
			} else if (anchor == "wall") {
				type.wall = true;
			} else if (anchor != "marker") {
				REX::WARN("Furniture: {}: type \"{}\": unknown \"anchor\" \"{}\" (marker, origin, edge, center or wall)", file, type.id, anchor);
			}
			type.wallDistance = entry.value("wallDistance", type.wallDistance);
			if (const auto floor = entry.find("floorScenes"); floor != entry.end() && floor->is_object()) {
				type.floorNeedTags = Strings(*floor, "needTags", true);
				type.floorExcludeTags = Strings(*floor, "excludeTags", true);
			}
			type.edgeLong = Lower(entry.value("edgeSide", std::string{ "long" })) != "short";
			type.edgeFacingOut = Lower(entry.value("facing", std::string{ "out" })) != "in";
			type.edgeInset = entry.value("edgeInset", 0.0F);
			type.checkWalls = entry.value("checkWalls", true);
			type.markerHeight = entry.value("markerHeight", false);
			type.onFloor = entry.value("onFloor", false);
			for (const auto& axis : Strings(entry, "ignoreMarkerAxes", true)) {
				if (axis == "x" || axis == "y" || axis == "z") {
					type.ignoreMarker[axis[0] - 'x'] = true;
				}
			}
			type.ignoreMarker[0] = type.ignoreMarker[0] || entry.value("ignoreMarkerOffsetX", false);
			type.ignoreMarker[1] = type.ignoreMarker[1] || entry.value("ignoreMarkerOffsetY", false);
			type.ignoreMarker[2] = type.ignoreMarker[2] || entry.value("ignoreMarkerOffsetZ", false);
			if (const auto off = entry.find("offset"); off != entry.end() && off->is_array() && off->size() == 3) {
				type.offset = { (*off)[0].get<float>(), (*off)[1].get<float>(), (*off)[2].get<float>() };
			}
			type.offset.x = entry.value("offsetX", type.offset.x);
			type.offset.y = entry.value("offsetY", type.offset.y);
			type.offset.z = entry.value("offsetZ", type.offset.z);
			type.rotation = entry.value("rotation", 0.0F) * DEG;
			type.listIndividually = entry.value("listIndividually", false);
			type.multiplyScale = entry.value("multiplyScale", 1.0F);
			if (type.models.empty() && type.keywords.empty() && !type.wall) {
				REX::INFO("Furniture: {}: type \"{}\" has no \"models\" or \"keywords\": only a supertype for others", file, type.id);
			}
			return type;
		}

		std::optional<nlohmann::json> ReadJSON(const std::filesystem::path& a_path)
		{
			try {
				std::ifstream in(a_path);
				return nlohmann::json::parse(in, nullptr, true, true);
			} catch (const std::exception& ex) {
				REX::WARN("Furniture: {}: not valid JSON ({})", a_path.filename().string(), ex.what());
				return std::nullopt;
			}
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
				const auto file = path.filename().string();
				const auto root = ReadJSON(path);
				if (!root) {
					continue;
				}
				const auto types = root->find("types");
				if (types == root->end() || !types->is_array()) {
					REX::WARN("Furniture: {}: no \"types\" list", file);
					continue;
				}
				for (const auto& entry : *types) {
					const auto id = entry.value("id", std::string{});
					if (id.empty()) {
						REX::WARN("Furniture: {}: a type without an \"id\", skipped", file);
						continue;
					}
					g_types[Lower(id)] = ParseType(entry, id, file);
				}
			}
			// OStim's layout: Furniture Types\**\<id>.json, after 4Stim's (so
			// they override a type of the same id).
			std::vector<std::filesystem::path> ostimFiles;
			for (const auto& e : std::filesystem::recursive_directory_iterator(OSTIM_TYPE_FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					ostimFiles.push_back(e.path());
				}
			}
			std::ranges::sort(ostimFiles);
			for (const auto& path : ostimFiles) {
				if (const auto root = ReadJSON(path); root && root->is_object()) {
					const auto id = Lower(path.stem().string());
					g_types[id] = ParseType(*root, id, path.filename().string());
				}
			}
			REX::INFO("Furniture: {} type(s) from {} file(s)", g_types.size(), files.size() + ostimFiles.size());
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

		// The furniture markers in an object's loaded model (the "FRN" extra
		// data of its root node), in the object's own frame. Fallout 4 keeps
		// the marker positions there; the form only has per-marker settings.
		struct MarkerNode : RE::NiExtraData
		{
			RE::BSTArray<RE::BSFurnitureMarker> markers;  // 18
		};

		MarkerNode* FindMarkerNode(RE::NiAVObject* a_object, int a_depth)
		{
			if (!a_object) {
				return nullptr;
			}
			if (const auto node = static_cast<MarkerNode*>(a_object->GetExtraData("FRN"))) {
				return node;
			}
			const auto asNode = a_depth > 0 ? a_object->IsNode() : nullptr;
			if (!asNode) {
				return nullptr;
			}
			for (const auto& child : asNode->children) {
				if (const auto found = FindMarkerNode(child.get(), a_depth - 1)) {
					return found;
				}
			}
			return nullptr;
		}

		std::vector<RE::BSFurnitureMarker> MarkersOf(RE::TESObjectREFR* a_ref)
		{
			std::vector<RE::BSFurnitureMarker> out;
			const auto root = a_ref ? a_ref->Get3D() : nullptr;
			if (!root) {
				REX::INFO("Furniture: {:08X}: no loaded model", a_ref ? a_ref->GetFormID() : 0);
				return out;
			}
			const auto node = FindMarkerNode(root, 2);
			if (!node) {
				REX::INFO("Furniture: {:08X}: no furniture markers in its model (root \"{}\")", a_ref->GetFormID(), root->GetName().c_str());
				return out;
			}
			const auto count = node->markers.size();
			if (count == 0 || count > 64) {
				REX::WARN("Furniture: {:08X}: {} markers in its model? Not used", a_ref->GetFormID(), count);
				return out;
			}
			for (const auto& marker : node->markers) {
				out.push_back(marker);
			}
			return out;
		}

		// A line-of-sight ray from a_from to a_to in a_cell: how far along it
		// (0-1) it hit something other than an actor, or nothing (1).
		struct RayHit
		{
			bool               hit = false;
			float              fraction = 1.0F;
			RE::TESObjectREFR* ref = nullptr;  // what it hit, if an object
			bool               actor = false;  // it hit an actor
		};

		RayHit CastRay(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
		{
			RayHit out;
			if (!a_cell) {
				return out;
			}
			RE::bhkPickData        pick;
			const Compat::PickGuard guard{ pick };
			pick.SetStartEnd(a_from, a_to);
			pick.castQuery.m_filterData.m_collisionFilterInfo = static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
			const auto object = a_cell->Pick(pick);
			if (!pick.HasHit()) {
				return out;
			}
			out.ref = object ? RE::TESObjectREFR::FindReferenceFor3D(object) : nullptr;
			out.actor = out.ref && out.ref->As<RE::Actor>();
			out.hit = true;
			out.fraction = std::clamp(pick.GetHitFraction(), 0.0F, 1.0F);
			return out;
		}

		// How far a_from -> a_to is clear, looking past actors and a_self (the
		// furniture being measured around: a ray starting over a bed goes on
		// through the bed): the distance to the first other thing, or the
		// whole length.
		float FreeDistance(RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_self, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
		{
			const auto  full = a_to - a_from;
			const float length = std::sqrt(full.x * full.x + full.y * full.y + full.z * full.z);
			if (length < 0.01F) {
				return 0.0F;
			}
			const auto dir = full * (1.0F / length);
			float      start = 0.0F;
			for (int i = 0; i < 6 && start < length; ++i) {
				const auto hit = CastRay(a_cell, a_from + dir * start, a_to);
				if (!hit.hit) {
					return length;
				}
				const float at = start + hit.fraction * (length - start);
				if (!hit.actor && (!a_self || hit.ref != a_self)) {
					return at;
				}
				start = at + 2.0F;  // on past it
			}
			return length;
		}

		// Whether a_point is within a_ref's bounding box grown by a_margin
		// (sideways; up to its top, from a little below it).
		bool InBounds(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_point, float a_margin)
		{
			const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
			if (!base) {
				return false;
			}
			const auto& b = base->boundData;
			const float scale = a_ref->refScale > 0 ? a_ref->refScale / 100.0F : 1.0F;
			const float yaw = a_ref->data.angle.z;
			const auto  origin = a_ref->GetPosition();
			const float dx = a_point.x - origin.x, dy = a_point.y - origin.y;
			const float c = std::cos(yaw), s = std::sin(yaw);
			const float x = (dx * c - dy * s) / scale, y = (dx * s + dy * c) / scale, z = (a_point.z - origin.z) / scale;
			const float m = a_margin / scale;
			return x >= b.boundMin.x - m && x <= b.boundMax.x + m && y >= b.boundMin.y - m && y <= b.boundMax.y + m && z >= b.boundMin.z - 50.0F &&
			       z <= b.boundMax.z;
		}

		// The nearest wall around a_center (feet height), within a_distance:
		// rays out in 24 directions at two heights must both hit the same
		// upright surface (not a door, not an actor). Where it is (on the
		// floor, at the foot of the wall) and which way is out from it.
		// A wall with something in front of it doesn't count: rays at five
		// heights, straight and a little to each side (an actor's width), must
		// all reach it (an open shelf lets the first two through to the wall
		// behind), and the spot can't be in or right beside any of a_furniture.
		struct WallHit
		{
			RE::NiPoint3 point;
			RE::NiPoint3 normal;  // horizontal, out from the wall toward a_center
			float        distance = 0.0F;
		};

		std::optional<WallHit> FindWall(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_center, float a_distance,
			const std::vector<RE::TESObjectREFR*>& a_furniture, float a_standOff)
		{
			constexpr float LOW = 50.0F, HIGH = 110.0F, MID = 80.0F;  // above the floor
			constexpr float SAME_SURFACE = 12.0F;                    // the two heights' hits may differ by this much
			constexpr float MIN_DISTANCE = 15.0F;
			constexpr float CLEAR_HEIGHTS[] = { 15.0F, 40.0F, 80.0F, 130.0F, 170.0F };
			constexpr float CLEAR_SIDE = 20.0F;    // either side of the straight ray
			constexpr float CLEAR_SLACK = 20.0F;   // a clearance ray may stop this much short of the wall
			constexpr float FURNITURE_MARGIN = 30.0F;
			auto dirOf = [](float a_angle) { return RE::NiPoint3{ std::sin(a_angle), std::cos(a_angle), 0.0F }; };
			auto distanceAt = [&](float a_angle, float a_height) -> std::optional<float> {
				const RE::NiPoint3 from{ a_center.x, a_center.y, a_center.z + a_height };
				const auto         hit = CastRay(a_cell, from, from + dirOf(a_angle) * a_distance);
				if (!hit.hit || hit.actor ||
					(hit.ref && hit.ref->GetObjectReference() && hit.ref->GetObjectReference()->GetFormType() == RE::ENUM_FORM_ID::kDOOR)) {
					return std::nullopt;
				}
				return hit.fraction * a_distance;
			};
			// Every direction where both heights hit one upright surface.
			struct Candidate
			{
				float angle;
				float distance;
			};
			std::vector<Candidate> candidates;
			for (int i = 0; i < 24; ++i) {
				const float angle = static_cast<float>(i) * 15.0F * DEG;
				const auto  low = distanceAt(angle, LOW);
				const auto  high = low ? distanceAt(angle, HIGH) : std::nullopt;
				if (!low || !high || std::fabs(*low - *high) > SAME_SURFACE) {
					continue;
				}
				const float d = (*low + *high) * 0.5F;
				if (d >= MIN_DISTANCE) {
					candidates.push_back({ angle, d });
				}
			}
			std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
			for (const auto& candidate : candidates) {
				const auto dir = dirOf(candidate.angle);
				const RE::NiPoint3 side{ dir.y, -dir.x, 0.0F };
				// Clear all the way to the wall: nothing in front of it.
				bool clear = true;
				for (const float height : CLEAR_HEIGHTS) {
					for (const float offset : { -CLEAR_SIDE, 0.0F, CLEAR_SIDE }) {
						const RE::NiPoint3 from = RE::NiPoint3{ a_center.x, a_center.y, a_center.z + height } + side * offset;
						const float        free = FreeDistance(a_cell, nullptr, from, from + dir * (candidate.distance + 10.0F));
						if (free < candidate.distance - CLEAR_SLACK) {
							clear = false;
							break;
						}
					}
					if (!clear) {
						break;
					}
				}
				if (!clear) {
					continue;
				}
				WallHit wall{ a_center + dir * candidate.distance, dir * -1.0F, candidate.distance };
				// The wall's own direction from two rays either side: out from it
				// is square to it, toward the center (else straight back).
				const auto left = distanceAt(candidate.angle - 6.0F * DEG, MID);
				const auto right = distanceAt(candidate.angle + 6.0F * DEG, MID);
				if (left && right && std::fabs(*left - *right) < 60.0F) {
					const auto   a = a_center + dirOf(candidate.angle - 6.0F * DEG) * *left;
					const auto   b = a_center + dirOf(candidate.angle + 6.0F * DEG) * *right;
					RE::NiPoint3 normal{ b.y - a.y, -(b.x - a.x), 0.0F };
					const float  length = std::sqrt(normal.x * normal.x + normal.y * normal.y);
					if (length > 0.01F) {
						normal = normal * (1.0F / length);
						const auto toCenter = a_center - wall.point;
						if (normal.x * toCenter.x + normal.y * toCenter.y < 0.0F) {
							normal = normal * -1.0F;
						}
						wall.normal = normal;
					}
				}
				wall.point.z = a_center.z;
				// Not where furniture stands: the spot out from the wall, and the
				// wall itself (a wall behind a shelf).
				const auto spot = wall.point + wall.normal * a_standOff;
				const auto blocked = std::ranges::find_if(a_furniture, [&](RE::TESObjectREFR* a_ref) {
					return InBounds(a_ref, spot, FURNITURE_MARGIN) || InBounds(a_ref, wall.point, FURNITURE_MARGIN);
				});
				if (blocked != a_furniture.end()) {
					REX::INFO("Furniture: a wall {:.0f} away, but {:08X} stands there", candidate.distance, (*blocked)->GetFormID());
					continue;
				}
				return wall;
			}
			return std::nullopt;
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

	std::vector<Found> FindNear(const RE::NiPoint3& a_center, float a_radius, float a_maxHeight, bool a_log, RE::TESObjectREFR* a_from)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		std::vector<Found> found;
		// Furniture near enough to stand in front of a wall: no wall spot there.
		std::vector<RE::TESObjectREFR*> obstacles;
		Compat::ForEachReferenceInRange(a_center, a_radius, [&](RE::TESObjectREFR* a_ref) {
			const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
			if (!base) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto formType = base->GetFormType();
			const bool candidate = formType == RE::ENUM_FORM_ID::kFURN || formType == RE::ENUM_FORM_ID::kSTAT ||
			                       formType == RE::ENUM_FORM_ID::kMSTT || formType == RE::ENUM_FORM_ID::kACTI ||
			                       formType == RE::ENUM_FORM_ID::kCONT;
			if (!candidate) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto pos = a_ref->GetPosition();
			if (std::fabs(pos.z - a_center.z) > a_maxHeight) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto type = ClassifyLocked(a_ref);
			const auto distance = a_center.GetDistance(pos);
			if ((!type.empty() || formType == RE::ENUM_FORM_ID::kFURN || formType == RE::ENUM_FORM_ID::kCONT) && distance < 600.0F) {
				obstacles.push_back(a_ref);
			}
			if (a_log && (formType == RE::ENUM_FORM_ID::kFURN || !type.empty())) {
				const char* code = formType == RE::ENUM_FORM_ID::kFURN ? "FURN" : formType == RE::ENUM_FORM_ID::kSTAT ? "STAT" :
				                   formType == RE::ENUM_FORM_ID::kMSTT ? "MSTT" :
				                   formType == RE::ENUM_FORM_ID::kCONT ? "CONT" :
				                                                         "ACTI";
				REX::INFO("Furniture: {:08X} {} \"{}\", {} marker(s), {:.0f} away -> {}", a_ref->GetFormID(), code,
					ModelOf(base), MarkerCount(base), distance, type.empty() ? "(none)" : type);
			}
			if (type.empty()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			// One entry per type (the nearest piece), or every piece of a type
			// OStim lists individually.
			const bool individually = g_types[Lower(type)].listIndividually;
			const auto it = individually ? found.end() : std::ranges::find_if(found, [&](const Found& f) { return f.type == type; });
			if (it == found.end()) {
				found.push_back({ type, g_types[Lower(type)].name, a_ref->GetFormID(), distance });
			} else if (distance < it->distance) {
				it->ref = a_ref->GetFormID();
				it->distance = distance;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		// Walls: the nearest one, for each type anchored on a wall.
		if (const auto cell = a_from ? a_from->GetParentCell() : nullptr) {
			for (const auto& [key, type] : g_types) {
				if (!type.wall) {
					continue;
				}
				const auto wall = FindWall(cell, a_center, std::min(type.wallDistance, a_radius), obstacles, std::max(type.offset.y, 15.0F));
				if (a_log) {
					REX::INFO("Furniture: wall for \"{}\": {}", type.id,
						wall ? std::format("{:.0f} away at ({:.1f}, {:.1f}, {:.1f}), out ({:.2f}, {:.2f})", wall->distance, wall->point.x, wall->point.y,
								   wall->point.z, wall->normal.x, wall->normal.y) :
							   std::string("none"));
				}
				if (wall) {
					// Facing out from the wall: forward (sin h, cos h) is its normal.
					found.push_back({ type.id, type.name, 0, wall->distance, Spot{ wall->point, std::atan2(wall->normal.x, wall->normal.y) } });
				}
			}
		}
		std::ranges::sort(found, [](const Found& a, const Found& b) { return a.distance < b.distance; });
		return found;
	}

	namespace
	{
		// How far out from an edge there must be room (an actor beside a bed).
		constexpr float WALL_CHECK_DISTANCE = 70.0F;

		// A point in a_ref's own frame (x right, y forward, z up), in the world.
		RE::NiPoint3 ToWorld(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_local)
		{
			const float yaw = a_ref->data.angle.z;
			const float c = std::cos(yaw), s = std::sin(yaw);
			const auto  origin = a_ref->GetPosition();
			return { origin.x + a_local.x * c + a_local.y * s, origin.y - a_local.x * s + a_local.y * c, origin.z + a_local.z };
		}

		// How much of the line a_from -> a_to is free (1 = all of it), by a
		// line-of-sight ray: walls, other furniture and clutter stop it;
		// actors and a_self don't count.
		float FreeFraction(RE::TESObjectREFR* a_self, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
		{
			const auto cell = a_self->GetParentCell();
			if (!cell) {
				return 1.0F;
			}
			RE::bhkPickData        pick;
			const Compat::PickGuard guard{ pick };
			pick.SetStartEnd(a_from, a_to);
			pick.castQuery.m_filterData.m_collisionFilterInfo = static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
			const auto hit = cell->Pick(pick);
			if (!pick.HasHit()) {
				return 1.0F;
			}
			if (hit) {
				const auto ref = RE::TESObjectREFR::FindReferenceFor3D(hit);
				if (ref == a_self || (ref && ref->As<RE::Actor>())) {
					return 1.0F;
				}
			}
			return std::clamp(pick.GetHitFraction(), 0.0F, 1.0F);
		}
	}

	float MultiplyScale(std::string_view a_type)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		const auto it = g_types.find(Lower(a_type));
		return it != g_types.end() && it->second.multiplyScale > 0.0F ? it->second.multiplyScale : 1.0F;
	}

	bool IsEdgeAnchored(std::string_view a_type)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		const auto it = g_types.find(Lower(a_type));
		return it != g_types.end() && it->second.edge;
	}

	Spot SpotFor(RE::TESObjectREFR* a_ref, std::string_view a_type, const RE::NiPoint3& a_near, bool a_floorScene)
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
		const float scale = a_ref->refScale > 0 ? a_ref->refScale / 100.0F : 1.0F;
		const float yaw = a_ref->data.angle.z;

		// In the furniture's own frame: x right, y forward, z up.
		RE::NiPoint3 local;
		float        localHeading = 0.0F;
		const auto   markers = MarkersOf(a_ref);
		for (std::size_t i = 0; i < markers.size(); ++i) {
			REX::INFO("Furniture: {:08X} marker {}: ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg, animations 0x{:X}", a_ref->GetFormID(), i,
				markers[i].position.x, markers[i].position.y, markers[i].position.z, markers[i].heading / DEG, markers[i].allowedAnimations);
		}
		int        used = -1;
		bool       zFromMarker = false;
		const auto bound = a_ref->GetObjectReference();  // already the base object's bounds holder
		if (type.edge && a_floorScene && bound) {
			// A scene without furniture of its own played on edge furniture (a
			// floor scene on a bed): lying along it, in the middle, at the
			// height the edge spot has (the mattress), heading toward the end
			// farther from the player. The edge spot is for scenes made for
			// the bed (sitting on its side...); a lying floor scene there
			// would stick out over the floor.
			const auto& b = bound->boundData;
			const float midX = (b.boundMin.x + b.boundMax.x) * 0.5F, midY = (b.boundMin.y + b.boundMax.y) * 0.5F;
			const bool  alongX = (b.boundMax.x - b.boundMin.x) >= (b.boundMax.y - b.boundMin.y);
			const auto  origin = a_ref->GetPosition();
			const float dx = a_near.x - origin.x, dy = a_near.y - origin.y;
			const float c0 = std::cos(yaw), s0 = std::sin(yaw);
			const float nearX = dx * c0 - dy * s0;
			const float nearY = dx * s0 + dy * c0;
			local = { midX, midY, type.onFloor ? static_cast<float>(b.boundMin.z) : 0.0F };
			if (alongX) {
				localHeading = nearX >= midX * scale ? -3.14159265F * 0.5F : 3.14159265F * 0.5F;
			} else {
				localHeading = nearY >= midY * scale ? 3.14159265F : 0.0F;
			}
			if (type.markerHeight && !markers.empty()) {
				local.z = markers[std::clamp(type.markerIndex, 0, static_cast<int>(markers.size()) - 1)].position.z;
				zFromMarker = true;
			}
			REX::INFO("Furniture: {:08X} bounds ({}, {}, {}) to ({}, {}, {}); a floor scene: center, along {}", a_ref->GetFormID(),
				b.boundMin.x, b.boundMin.y, b.boundMin.z, b.boundMax.x, b.boundMax.y, b.boundMax.z, alongX ? "x" : "y");
		} else if (type.edge && bound) {
			// The bounding box, in the object's frame.
			const auto& b = bound->boundData;
			const float minX = b.boundMin.x, maxX = b.boundMax.x, minY = b.boundMin.y, maxY = b.boundMax.y;
			const float midX = (minX + maxX) * 0.5F, midY = (minY + maxY) * 0.5F;
			const bool  alongX = (maxX - minX) >= (maxY - minY);  // long axis is x
			// The sides to choose from: long sides lie along the long axis.
			const bool sidesOnY = type.edgeLong ? alongX : !alongX;
			// Which of the two is nearer the player, in the object's frame.
			const auto  origin = a_ref->GetPosition();
			const float dx = a_near.x - origin.x, dy = a_near.y - origin.y;
			const float c0 = std::cos(yaw), s0 = std::sin(yaw);
			const float nearX = dx * c0 - dy * s0;  // inverse of the rotation below
			const float nearY = dx * s0 + dy * c0;
			// The side nearer the player, unless a wall (or other furniture)
			// stands right outside it and the other side is free.
			bool plus = sidesOnY ? nearY >= midY * scale : nearX >= midX * scale;
			if (type.checkWalls) {
				// How far out from that side there's room, up to
				// WALL_CHECK_DISTANCE. The ray starts over the middle of the bed,
				// not at its edge: a bed pushed against a wall has its edge in
				// or touching the wall, and a ray starting inside a wall
				// doesn't see it.
				// At knee, chest and head height above the floor it stands on,
				// and just over its top, looking past the furniture itself: the
				// least room of those (a low wall, a gap above a short one, a
				// crate beside a bed all count).
				auto roomOutside = [&](bool a_plus) {
					const float sign = a_plus ? 1.0F : -1.0F;
					const RE::NiPoint3 dir = sidesOnY ? RE::NiPoint3{ 0.0F, sign, 0.0F } : RE::NiPoint3{ sign, 0.0F, 0.0F };
					const float toEdge = (sidesOnY ? (a_plus ? maxY - midY : midY - minY) : (a_plus ? maxX - midX : midX - minX)) * scale;
					const float length = toEdge + WALL_CHECK_DISTANCE;
					const float floor = static_cast<float>(b.boundMin.z);
					float       room = WALL_CHECK_DISTANCE;
					for (const float height : { floor + 40.0F, floor + 100.0F, floor + 150.0F, static_cast<float>(b.boundMax.z) + 30.0F }) {
						const RE::NiPoint3 start = RE::NiPoint3{ midX, midY, height } * scale;
						const float free = FreeDistance(a_ref->GetParentCell(), a_ref, ToWorld(a_ref, start), ToWorld(a_ref, start + dir * length));
						room = std::min(room, std::clamp(free - toEdge, 0.0F, WALL_CHECK_DISTANCE));
					}
					return room;
				};
				const float nearRoom = roomOutside(plus);
				const float farRoom = roomOutside(!plus);
				REX::INFO("Furniture: {:08X}: room beside the near side {:.0f}, the far side {:.0f} (of {:.0f})", a_ref->GetFormID(), nearRoom, farRoom,
					WALL_CHECK_DISTANCE);
				if (nearRoom < WALL_CHECK_DISTANCE && farRoom > nearRoom) {
					plus = !plus;
					REX::INFO("Furniture: {:08X}: the side away from the wall", a_ref->GetFormID());
				}
			}
			if (sidesOnY) {
				const float edgeY = plus ? maxY : minY;
				local = { midX, edgeY + (plus ? -type.edgeInset : type.edgeInset), 0.0F };
				localHeading = plus ? 0.0F : 3.14159265F;  // facing out along y
			} else {
				const float edgeX = plus ? maxX : minX;
				local = { edgeX + (plus ? -type.edgeInset : type.edgeInset), midY, 0.0F };
				localHeading = plus ? 3.14159265F * 0.5F : -3.14159265F * 0.5F;  // facing out along x
			}
			if (!type.edgeFacingOut) {
				localHeading += 3.14159265F;
			}
			if (type.onFloor) {
				local.z = b.boundMin.z;
			}
			if (type.markerHeight && !markers.empty()) {
				local.z = markers[std::clamp(type.markerIndex, 0, static_cast<int>(markers.size()) - 1)].position.z;
				zFromMarker = true;  // instead of the offset's height
			}
			REX::INFO("Furniture: {:08X} bounds ({}, {}, {}) to ({}, {}, {}); edge {} side, facing {}", a_ref->GetFormID(),
				b.boundMin.x, b.boundMin.y, b.boundMin.z, b.boundMax.x, b.boundMax.y, b.boundMax.z, type.edgeLong ? "long" : "short", type.edgeFacingOut ? "out" : "in");
		} else if (type.center && bound) {
			// The middle of the bounding box, lengthwise, heading toward the
			// end farther from the player (so the player is at the near end).
			const auto& b = bound->boundData;
			const float midX = (b.boundMin.x + b.boundMax.x) * 0.5F, midY = (b.boundMin.y + b.boundMax.y) * 0.5F;
			const bool  alongX = (b.boundMax.x - b.boundMin.x) >= (b.boundMax.y - b.boundMin.y);
			const auto  origin = a_ref->GetPosition();
			const float dx = a_near.x - origin.x, dy = a_near.y - origin.y;
			const float c0 = std::cos(yaw), s0 = std::sin(yaw);
			const float nearX = dx * c0 - dy * s0;
			const float nearY = dx * s0 + dy * c0;
			local = { midX, midY, static_cast<float>(type.onFloor ? b.boundMin.z : b.boundMax.z) };
			if (alongX) {
				localHeading = nearX >= midX * scale ? -3.14159265F * 0.5F : 3.14159265F * 0.5F;
			} else {
				localHeading = nearY >= midY * scale ? 3.14159265F : 0.0F;
			}
			if (type.markerHeight && !markers.empty()) {
				local.z = markers[std::clamp(type.markerIndex, 0, static_cast<int>(markers.size()) - 1)].position.z;
				zFromMarker = true;
			}
			REX::INFO("Furniture: {:08X} bounds ({}, {}, {}) to ({}, {}, {}); center, along {}", a_ref->GetFormID(),
				b.boundMin.x, b.boundMin.y, b.boundMin.z, b.boundMax.x, b.boundMax.y, b.boundMax.z, alongX ? "x" : "y");
		} else if (type.edge || type.center) {
			REX::WARN("Furniture: {:08X}: no bounds to find an edge or center on", a_ref->GetFormID());
		} else if (type.useMarker && !markers.empty()) {
			used = std::clamp(type.markerIndex, 0, static_cast<int>(markers.size()) - 1);
			const auto& marker = markers[used];
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
		// The type's offset is in the edge spot's terms; a floor scene on it
		// only takes its height.
		const bool floorOnEdge = type.edge && a_floorScene;
		local = local + RE::NiPoint3{ floorOnEdge ? 0.0F : type.offset.x, floorOnEdge ? 0.0F : type.offset.y, zFromMarker ? 0.0F : type.offset.z };
		local = local * scale;

		const float c = std::cos(yaw);
		const float s = std::sin(yaw);
		const auto  origin = a_ref->GetPosition();
		spot.position = { origin.x + local.x * c + local.y * s, origin.y - local.x * s + local.y * c, origin.z + local.z };
		spot.heading = yaw + localHeading + type.rotation;
		REX::INFO("Furniture: spot on {:08X} ({}): ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg (marker {}: local ({:.1f}, {:.1f}, {:.1f}), {:.1f} deg)",
			a_ref->GetFormID(), a_type, spot.position.x, spot.position.y, spot.position.z, spot.heading / DEG,
			used >= 0 ? std::to_string(used) : std::string("none"), local.x, local.y, local.z, localHeading / DEG);
		return spot;
	}

	bool AllowsFloorScene(std::string_view a_type, const SceneRegistry::Scene& a_scene)
	{
		Type type;
		{
			std::scoped_lock lock(g_lock);
			EnsureLoaded();
			const auto it = g_types.find(Lower(a_type));
			if (it == g_types.end()) {
				return true;
			}
			type = it->second;
		}
		auto anyActorHas = [&](const std::vector<std::string>& a_tags) {
			return std::ranges::any_of(a_scene.actors, [&](const SceneRegistry::SceneActor& a_actor) {
				return std::ranges::any_of(a_tags, [&](const std::string& a_tag) { return a_actor.HasTag(a_tag); });
			});
		};
		if (!type.floorNeedTags.empty() && !anyActorHas(type.floorNeedTags)) {
			return false;
		}
		return type.floorExcludeTags.empty() || !anyActorHas(type.floorExcludeTags);
	}

	float ClearFraction(RE::TESObjectREFR* a_ignore, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
	{
		return a_ignore ? FreeFraction(a_ignore, a_from, a_to) : 1.0F;
	}

	Spot SpotOf(const Found& a_found, const RE::NiPoint3& a_near, bool a_floorScene)
	{
		if (!a_found.spot) {
			return SpotFor(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_found.ref), a_found.type, a_near, a_floorScene);
		}
		// A wall: the type's offset in the spot's frame (x right, y forward
		// out from the wall, z up) and its rotation.
		RE::NiPoint3 offset;
		float        rotation = 0.0F;
		{
			std::scoped_lock lock(g_lock);
			EnsureLoaded();
			if (const auto it = g_types.find(Lower(a_found.type)); it != g_types.end()) {
				offset = it->second.offset;
				rotation = it->second.rotation;
			}
		}
		Spot        spot = *a_found.spot;
		const float c = std::cos(spot.heading), s = std::sin(spot.heading);
		spot.position = { spot.position.x + offset.x * c + offset.y * s, spot.position.y - offset.x * s + offset.y * c, spot.position.z + offset.z };
		spot.heading += rotation;
		REX::INFO("Furniture: spot at the wall ({}): ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg", a_found.type, spot.position.x, spot.position.y,
			spot.position.z, spot.heading / DEG);
		return spot;
	}

	namespace
	{
		// OStim's clutter forms (FurnitureTable::clutterForms), as Fallout 4 has them.
		bool IsClutter(RE::ENUM_FORM_ID a_type)
		{
			switch (a_type) {
			case RE::ENUM_FORM_ID::kAMMO:
			case RE::ENUM_FORM_ID::kARMO:
			case RE::ENUM_FORM_ID::kBOOK:
			case RE::ENUM_FORM_ID::kINGR:
			case RE::ENUM_FORM_ID::kKEYM:
			case RE::ENUM_FORM_ID::kMISC:
			case RE::ENUM_FORM_ID::kMSTT:
			case RE::ENUM_FORM_ID::kALCH:
			case RE::ENUM_FORM_ID::kWEAP:
			case RE::ENUM_FORM_ID::kNOTE:
				return true;
			default:
				return false;
			}
		}

		constexpr std::size_t MAX_CLUTTER = 512;
	}

	std::vector<ClutterItem> SnapshotClutter(const RE::NiPoint3& a_center, float a_radius)
	{
		std::vector<ClutterItem> items;
		if (a_radius <= 0.0F) {
			return items;
		}
		Compat::ForEachReferenceInRange(a_center, a_radius, [&](RE::TESObjectREFR* a_ref) {
			const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
			if (!base || !IsClutter(base->GetFormType()) || a_ref->IsDisabled() || a_ref->IsDeleted() || !a_ref->Get3D()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			items.push_back({ a_ref->GetFormID(), a_ref->data.location, a_ref->data.angle });
			return items.size() < MAX_CLUTTER ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop;
		});
		REX::INFO("Clutter: {} loose item(s) within {:.0f} noted", items.size(), a_radius);
		return items;
	}

	std::size_t RestoreClutter(const std::vector<ClutterItem>& a_items)
	{
		const auto vm = FourStim::GetVM();
		if (!vm) {
			return 0;
		}
		constexpr float MOVED = 2.0F;              // units
		constexpr float TURNED = 3.0F * DEG;
		std::size_t     count = 0;
		for (const auto& item : a_items) {
			const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(item.ref);
			if (!ref || ref->IsDisabled() || ref->IsDeleted() || !ref->GetParentCell()) {
				continue;  // picked up or gone
			}
			const auto& at = ref->data.location;
			const auto& angle = ref->data.angle;
			auto        turned = [](float a, float b) { return std::fabs(std::remainder(a - b, 2.0F * 3.14159265F)) > TURNED; };
			if (at.GetDistance(item.position) < MOVED && !turned(angle.x, item.angle.x) && !turned(angle.y, item.angle.y) && !turned(angle.z, item.angle.z)) {
				continue;
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("FourStimFurniture"sv, "PutBack"sv, callback, static_cast<std::int32_t>(item.ref), item.position.x, item.position.y,
				item.position.z, item.angle.x / DEG, item.angle.y / DEG, item.angle.z / DEG);
			++count;
		}
		REX::INFO("Clutter: {} of {} item(s) put back", count, a_items.size());
		return count;
	}
}
