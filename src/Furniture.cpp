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
			bool                           useMarker = true;  // place at a furniture marker
			int                            markerIndex = 0;   // which one
			// "edge": on an edge of its bounding box (the side nearest the
			// player), facing out from it or in toward it.
			bool                           edge = false;
			// "center": in the middle of its bounding box, turned along its
			// long axis (for lying on a mattress or a sleeping bag).
			bool                           center = false;
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
					type.markerIndex = entry.value("marker", 0);
					if (const auto anchor = Lower(entry.value("anchor", std::string{ "marker" })); anchor == "edge") {
						type.edge = true;
					} else if (anchor == "center") {
						type.center = true;
					} else if (anchor == "origin") {
						type.useMarker = false;
					} else if (anchor != "marker") {
						REX::WARN("Furniture: {}: type \"{}\": unknown \"anchor\" \"{}\" (marker, origin, edge or center)", file, type.id, anchor);
					}
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
			RE::bhkPickData pick;
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

	Spot SpotFor(RE::TESObjectREFR* a_ref, std::string_view a_type, const RE::NiPoint3& a_near)
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
		if (type.edge && bound) {
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
				auto roomOutside = [&](bool a_plus) {
					const float sign = a_plus ? 1.0F : -1.0F;
					const RE::NiPoint3 dir = sidesOnY ? RE::NiPoint3{ 0.0F, sign, 0.0F } : RE::NiPoint3{ sign, 0.0F, 0.0F };
					const float height = static_cast<float>(b.boundMax.z) + 30.0F;
					const RE::NiPoint3 start = RE::NiPoint3{ midX, midY, height } * scale;
					const float toEdge = (sidesOnY ? (a_plus ? maxY - midY : midY - minY) : (a_plus ? maxX - midX : midX - minX)) * scale;
					const float length = toEdge + WALL_CHECK_DISTANCE;
					const float free = FreeFraction(a_ref, ToWorld(a_ref, start), ToWorld(a_ref, start + dir * length)) * length;
					return std::clamp(free - toEdge, 0.0F, WALL_CHECK_DISTANCE);
				};
				const float nearRoom = roomOutside(plus);
				const float farRoom = roomOutside(!plus);
				REX::INFO("Furniture: {:08X}: room beside the near side {:.0f}, the far side {:.0f} (of {:.0f})", a_ref->GetFormID(), nearRoom, farRoom,
					WALL_CHECK_DISTANCE);
				if (nearRoom < WALL_CHECK_DISTANCE && farRoom > nearRoom) {
					plus = !plus;
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
		local = local + RE::NiPoint3{ type.offset.x, type.offset.y, zFromMarker ? 0.0F : type.offset.z };
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
}
