#include "Core/Core.h"

namespace Core::Natives::FourFurniture
{
	std::vector<RE::TESObjectREFR*> FindFurniture(std::monostate, std::int32_t a_actorCount, RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor)
	{
		std::vector<RE::TESObjectREFR*> out;
		for (const auto& f : FurnitureAround(a_center, a_radius, a_sameFloor)) {
			if (!f.ref) {
				continue;  // walls are spots, not objects
			}
			const bool any = a_actorCount <= 0 || std::ranges::any_of(SceneRegistry::List(static_cast<std::size_t>(a_actorCount)),
				[&](const SceneRegistry::SceneSummary& s) { return !s.furniture.empty() && FurnitureFits(s.furniture, f.type); });
			if (any) {
				out.push_back(RE::TESForm::GetFormByID<RE::TESObjectREFR>(f.ref));
			}
		}
		return out;
	}
	RE::TESObjectREFR* FindFurnitureOfType(std::monostate, std::string a_type, RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor)
	{
		const auto type = LowerCopy(a_type);
		for (const auto& f : FurnitureAround(a_center, a_radius, a_sameFloor)) {
			if (f.ref && Furniture::IsA(f.type, type)) {
				return RE::TESForm::GetFormByID<RE::TESObjectREFR>(f.ref);
			}
		}
		return nullptr;
	}
	// OFurniture.GetOffset: where a scene goes on the piece, in its own
	// frame, as OStim's [x, y, z, rotation, scale] (its marker offset plus
	// the type's offsets; here 4Stim's spot for the type: markers, else its
	// bounds, plus the type's offset). Rotation in radians, as OStim's.
	// Empty for something that isn't furniture a scene type knows.
	std::vector<float> GetOffset(std::monostate, RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			return {};
		}
		const auto type = Furniture::Classify(a_ref);
		if (type.empty()) {
			return {};
		}
		const auto spot = Furniture::SpotFor(a_ref, type, a_ref->data.location);
		const auto d = spot.position - a_ref->data.location;
		const float yaw = a_ref->data.angle.z;
		// Into the piece's frame (the game's heading turns clockwise from north).
		const float x = d.x * std::cos(yaw) - d.y * std::sin(yaw);
		const float y = d.x * std::sin(yaw) + d.y * std::cos(yaw);
		return { x, y, d.z, spot.heading - yaw, Furniture::MultiplyScale(type) };
	}
	// OFurniture.GetSceneID: the thread using the piece, -1 if none.
	std::int32_t GetSceneID(std::monostate, RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			return -1;
		}
		for (const auto& active : g_activeScenes) {
			if (active.furnitureRef == a_ref->GetFormID()) {
				return active.threadID;
			}
		}
		return -1;
	}
}
