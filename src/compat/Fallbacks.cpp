#include "compat/Fallbacks.h"

#include "compat/Addresses.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string>
#include <intrin.h>

namespace Compat
{
	namespace
	{
		// Whether an address is there, looked up once (the Runtime Database
		// can take a while on a miss).
		template <std::uint64_t ID>
		bool Have()
		{
			static const bool have = HasAddress(ID);
			return have;
		}
	}

	RE::SEX GetSex(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return RE::SEX::kNone;
		}
		if (Have<2229674>()) {  // Actor::GetSex
			return a_actor->GetSex();
		}
		// What Papyrus's ActorBase.GetSex gives: the base NPC's (a leveled
		// actor's base is the NPC made for it).
		const auto base = a_actor->GetObjectReference();
		const auto npc = base && base->GetFormType() == RE::ENUM_FORM_ID::kNPC_ ? static_cast<RE::TESNPC*>(base) : nullptr;
		return npc ? npc->GetSex() : RE::SEX::kNone;
	}

	void ForEachReferenceInRange(const RE::NiPoint3& a_origin, float a_radius, std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR*)> a_callback)
	{
		if (a_radius <= 0.0F) {
			return;
		}
		if (Have<2698044>() && Have<2194566>()) {  // TES::Singleton, GridCellArray::Get
			if (const auto tes = RE::TES::GetSingleton()) {
				tes->ForEachReferenceInRange(a_origin, a_radius, a_callback);
			}
			return;
		}
		// TES::ForEachReferenceInRange (CommonLibF4's), with the worldspace's
		// cell map for TES's grid of loaded cells: the attached ones only, as
		// there.
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto cell = player ? player->GetParentCell() : nullptr;
		REX::INFO("Compat: references within {:.0f} of the player's cell {:08X} ({})", a_radius, cell ? cell->GetFormID() : 0,
			!cell ? "none" : cell->IsInterior() ? "interior" : "exterior");
		if (!cell) {
			return;
		}
		if (cell->IsInterior()) {
			cell->ForEachReferenceInRange(a_origin, a_radius, a_callback);
			return;
		}
		const auto world = cell->worldSpace;
		if (!world) {
			cell->ForEachReferenceInRange(a_origin, a_radius, a_callback);
			return;
		}
		const float xPlus = a_origin.x + a_radius;
		const float xMinus = a_origin.x - a_radius;
		const float yPlus = a_origin.y + a_radius;
		const float yMinus = a_origin.y - a_radius;
		REX::INFO("Compat: worldspace {:08X}, {} cell(s) in its map", world->GetFormID(), world->cellMap.size());
		for (const auto& entry : world->cellMap) {
			const auto exterior = entry.second;
			if (!exterior || !exterior->IsAttached()) {
				continue;
			}
			const auto coords = exterior->GetCoordinates();
			if (!coords) {
				continue;
			}
			if (coords->worldX < xPlus && coords->worldX + 4096.0F > xMinus && coords->worldY < yPlus && coords->worldY + 4096.0F > yMinus) {
				exterior->ForEachReferenceInRange(a_origin, a_radius, a_callback);
			}
		}
		if (const auto sky = world->GetSkyCell()) {
			sky->ForEachReferenceInRange(a_origin, a_radius, a_callback);
		}
	}

	void ReleasePick(RE::bhkPickData& a_pick)
	{
		if (Have<2195747>()) {  // hkReferencedObject::RemoveReference: the destructor does it
			return;
		}
		const auto world = a_pick.castWorld.get();
		if (!world) {
			return;
		}
		// Havok's hkReferencedObject::removeReference: the count is the low
		// 16 bits of m_memSizeAndRefCount (0 = not counted); the last
		// reference deletes the object.
		const auto object = reinterpret_cast<RE::hkReferencedObject*>(world);
		const auto count = reinterpret_cast<volatile long*>(&object->m_memSizeAndRefCount);
		long       before = *count;
		while ((before & 0xFFFF) != 0) {
			const long seen = _InterlockedCompareExchange(count, before - 1, before);
			if (seen == before) {
				if (((before - 1) & 0xFFFF) == 0) {
					object->DeleteThisReferencedObject();
				}
				break;
			}
			before = seen;
		}
		// So hkRefPtr's destructor finds nothing to release.
		*reinterpret_cast<void**>(&a_pick.castWorld) = nullptr;
	}

	namespace
	{
		RE::BSResource::ID OwnResourceID(const char* a_path)
		{
			std::string path = a_path ? a_path : "";
			std::ranges::transform(path, path.begin(), [](char c) {
				return c == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			});
			while (!path.empty() && path.front() == '\\') {
				path.erase(0, 1);
			}
			if (path.starts_with("data\\")) {
				path.erase(0, 5);
			}
			const auto        slash = path.find_last_of('\\');
			const std::string dir = slash == std::string::npos ? std::string{} : path.substr(0, slash);
			std::string       name = slash == std::string::npos ? path : path.substr(slash + 1);
			std::string       ext;
			if (const auto dot = name.find_last_of('.'); dot != std::string::npos) {
				ext = name.substr(dot + 1);
				name.erase(dot);
			}
			auto crc = [](const std::string& a_text) {
				return RE::detail::GenerateCRC32({ reinterpret_cast<const std::uint8_t*>(a_text.data()), a_text.size() });
			};
			RE::BSResource::ID id;
			id.file = crc(name);
			id.dir = crc(dir);
			id.ext = 0;
			for (std::size_t i = 0; i < ext.size() && i < 4; ++i) {
				id.ext |= static_cast<std::uint32_t>(static_cast<unsigned char>(ext[i])) << (8 * i);
			}
			return id;
		}
	}

	RE::BSResource::ID ResourceID(const char* a_path)
	{
		if (!Have<2269259>()) {  // BSResource::ID::GenerateFromPath
			return OwnResourceID(a_path);
		}
		const RE::BSResource::ID game(a_path);
		// Where the game's is there, the first few are checked against 4Stim's
		// own, so the way 1.10.163 does it is known to be right.
		static std::atomic<int> checks{ 0 };
		if (checks.fetch_add(1) < 3) {
			const auto own = OwnResourceID(a_path);
			if (own == game) {
				REX::INFO("Compat: resource ID of {} worked out the same as the game's", a_path);
			} else {
				REX::WARN("Compat: resource ID of {}: game {:08X} {:08X} {:08X}, 4Stim's {:08X} {:08X} {:08X}", a_path, game.file, game.ext, game.dir,
					own.file, own.ext, own.dir);
			}
		}
		return game;
	}

	void Prime()
	{
		Have<2229674>();
		Have<2698044>();
		Have<2194566>();
		Have<2195747>();
		Have<2231227>();
		Have<2267064>();
		Have<2269259>();
		Have<2295298>();
		Have<2211661>();
		Have<2211735>();
		Have<2277919>();
		Have<2277926>();
	}

	bool CanStopInteractingQuick() { return Have<2231227>(); }
	bool CanSetSoundPosition() { return Have<2267064>(); }
	bool CanAddTranslations() { return Have<2295298>(); }
	bool CanMakePackages() { return Have<2211661>() && Have<2211735>(); }
	bool CanReadCollisionBodies() { return Have<2277919>() && Have<2277926>(); }
}
