#include "Core/Core.h"

namespace Core
{
	struct SavedBody
	{
		RE::NiPointer<RE::bhkNPCollisionObject> object;
		std::uint32_t                           filter = 0;
		std::uint32_t                           ref = 0;
	};
	std::mutex                                                g_furnitureFiltersLock;
	std::unordered_map<std::uint32_t, std::vector<SavedBody>> g_furnitureFilters;  // scene furniture -> bodies changed for it

	constexpr float FURNITURE_CLEAR_RADIUS = 120.0F;  // other furniture this close to the scene's joins too

	void CollectBodies(RE::NiAVObject* a_object, std::vector<RE::bhkNPCollisionObject*>& a_out, int a_depth)
	{
		if (!a_object) {
			return;
		}
		if (const auto object = netimmerse_cast<RE::bhkNPCollisionObject*>(a_object->collisionObject.get())) {
			a_out.push_back(object);
		}
		if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
			for (const auto& child : node->children) {
				CollectBodies(child.get(), a_out, a_depth - 1);
			}
		}
	}

	void IgnoreFurnitureCollision(std::uint32_t a_furniture, std::vector<std::uint32_t> a_actors, const std::string& a_type)
	{
		if (!a_furniture || !g_lockScenes || !g_settings.furnitureOutOfWay || !Compat::CanReadCollisionBodies()) {
			return;  // without Getbhk / GetBody (1.10.163 if the Runtime Database lacks them): left solid
		}
		// Beds stay solid: scenes lie on them, and without the bed under him
		// the owner's player sank through it (drawn inside the bed, his head
		// at the rail) while the NPC, held more firmly, lay on top. Solid,
		// both lay on it (1 November).
		if (!a_type.empty() && Furniture::IsA(a_type, "bed")) {
			REX::INFO("Furniture collision: {:08X} is a bed: it stays solid for the scene's actors", a_furniture);
			return;
		}
		// A task after IgnorePairCollision's (queued earlier): the actors
		// already share their group.
		F4SE::GetTaskInterface()->AddTask([a_furniture, a_actors]() {
			const auto furniture = RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_furniture);
			if (!furniture) {
				return;
			}
			// The furniture next to it first: the cell's references, not the
			// physics world (outside its lock).
			std::vector<RE::TESObjectREFR*> pieces{ furniture };
			Compat::ForEachReferenceInRange(furniture->data.location, FURNITURE_CLEAR_RADIUS, [&](RE::TESObjectREFR* a_ref) {
				const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
				if (a_ref != furniture && base && base->GetFormType() == RE::ENUM_FORM_ID::kFURN && !a_ref->IsDisabled() && !a_ref->IsDeleted() && a_ref->Get3D()) {
					pieces.push_back(a_ref);
				}
				return pieces.size() < 8 ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop;
			});
			const PhysicsLock physics(furniture);
			std::uint32_t     group = 0;
			for (const auto id : a_actors) {
				const auto controller = GetCharController(RE::TESForm::GetFormByID<RE::Actor>(id));
				const auto body = controller ? controller->GetBodyImpl() : nullptr;
				if (body) {
					RE::CFilter filter{};
					filter.filter = body->m_collisionFilterInfo;
					group = filter.GetSystemGroup();
					break;
				}
			}
			if (group == 0) {
				REX::INFO("Furniture collision: the actors on {:08X} have no collision group; left as it is", a_furniture);
				return;
			}
			std::scoped_lock lock(g_furnitureFiltersLock);
			auto&            saved = g_furnitureFilters[a_furniture];
			for (const auto piece : pieces) {
				std::vector<RE::bhkNPCollisionObject*> objects;
				CollectBodies(piece->Get3D(), objects, 16);
				std::size_t changed = 0;
				for (const auto object : objects) {
					const auto body = object->GetBody();
					if (!body) {
						continue;
					}
					const auto own = body->m_collisionFilterInfo;
					if (std::ranges::any_of(saved, [&](const SavedBody& a_saved) { return a_saved.object.get() == object; })) {
						continue;  // already done (a scene change on the same furniture)
					}
					RE::CFilter filter{};
					filter.filter = own;
					filter.SetSystemGroup(group);
					if (object->SetCollisionFilterInfo(filter)) {
						saved.push_back({ RE::NiPointer<RE::bhkNPCollisionObject>(object), own, piece->GetFormID() });
						++changed;
					}
				}
				REX::INFO("Furniture collision: {:08X}{} out of the scene actors' way ({} of {} collision bodies, group {:04X})", piece->GetFormID(),
					piece == furniture ? " (the scene's)" : " (next to it)", changed, objects.size(), group);
			}
		});
	}

	void RestoreFurnitureCollision(std::uint32_t a_furniture)
	{
		if (!a_furniture) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([a_furniture]() {
			std::vector<SavedBody> bodies;
			{
				std::scoped_lock lock(g_furnitureFiltersLock);
				const auto       it = g_furnitureFilters.find(a_furniture);
				if (it == g_furnitureFilters.end()) {
					return;
				}
				bodies = std::move(it->second);
				g_furnitureFilters.erase(it);
			}
			const PhysicsLock physics(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_furniture));
			std::size_t       restored = 0;
			for (const auto& body : bodies) {
				RE::CFilter filter{};
				filter.filter = body.filter;
				if (body.object && body.object->SetCollisionFilterInfo(filter)) {
					++restored;
				}
			}
			REX::INFO("Furniture collision: {:08X} and what was next to it collide with the actors again ({} of {} bodies)", a_furniture, restored, bodies.size());
		});
	}

	constexpr float WATCH_RADIUS = 160.0F;  // references this close to the actor are looked through

	// The references around a_actor and their collision objects, looked up
	// again at most every 5 seconds. Outside the physics lock: it walks the
	// cell's references and their 3D, not the physics world.
	void RefreshNearby(RE::Actor* a_actor)
	{
		auto&      nearby = g_nearbyBodies[a_actor->GetFormID()];
		const auto now = std::chrono::steady_clock::now();
		if (!nearby.entries.empty() && now - nearby.built < std::chrono::seconds(5)) {
			return;
		}
		nearby.built = now;
		nearby.entries.clear();
		std::size_t refs = 0;
		Compat::ForEachReferenceInRange(a_actor->data.location, WATCH_RADIUS, [&](RE::TESObjectREFR* a_ref) {
			const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
			if (!base || a_ref->IsDisabled() || a_ref->IsDeleted() || base->GetFormType() == RE::ENUM_FORM_ID::kNPC_) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			std::vector<RE::bhkNPCollisionObject*> objects;
			CollectBodies(a_ref->Get3D(), objects, 16);
			for (const auto object : objects) {
				nearby.entries.push_back({ RE::NiPointer<RE::bhkNPCollisionObject>(object), a_ref->GetFormID(), base->GetFormID(), static_cast<std::uint8_t>(base->GetFormType()) });
			}
			return ++refs < 96 ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop;
		});
	}

	// Which of those owns a_body (under the physics lock).
	const NearbyBodies::Entry* OwnerOf(RE::Actor* a_actor, const RE::hknpBody* a_body)
	{
		for (const auto& entry : g_nearbyBodies[a_actor->GetFormID()].entries) {
			if (entry.object && entry.object->GetBody() == a_body) {
				return &entry;
			}
		}
		return nullptr;
	}

	void WatchCollisions(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		if (!Compat::CanReadCollisionBodies()) {
			return;  // 1.10.163 without Getbhk / GetBody: no watch (compat/Fallbacks.h)
		}
		if (g_lockScenes && g_settings.furnitureOutOfWay) {
			RefreshNearby(a_actor);
		}
		const PhysicsLock physics(a_actor);
		const auto        controller = GetCharController(a_actor);
		const auto        own = controller ? controller->GetBodyImpl() : nullptr;
		if (!own) {
			return;
		}
		// Where the controller and the drawn body are, against where the
		// actor is (the lock's spot): a shake that the lock watchdog never
		// saw (no "Scene lock:" in the owner's log) is one of these. At most
		// once a second, ten times a scene.
		if (g_settings.collisionWatch) {
			struct Seen
			{
				std::chrono::steady_clock::time_point last{};
				int                                   count = 0;
			};
			static std::unordered_map<std::uint32_t, Seen> seen;
			auto&                                          entry = seen[a_actor->GetFormID()];
			if (!g_touchLogged.contains(a_actor->GetFormID())) {
				entry.count = 0;  // a new scene (TrackSceneStart forgets the actor's entry)
				g_touchLogged[a_actor->GetFormID()];
			}
			const auto now = std::chrono::steady_clock::now();
			if (entry.count < 20 && now - entry.last > std::chrono::milliseconds(200)) {
				RE::hkVector4f at{};
				controller->GetPositionImpl(at, false);
				constexpr float HAVOK_TO_GAME = 69.99125F;  // Fallout 4's Havok scale (1 / 0.0142875)
				const RE::NiPoint3 controllerAt{ at[0] * HAVOK_TO_GAME, at[1] * HAVOK_TO_GAME, at[2] * HAVOK_TO_GAME };
				const auto         root = a_actor->Get3D();
				const auto         drawnAt = root ? root->world.translate : a_actor->data.location;
				// The controller's position is its capsule's middle, about half
				// the actor's height up (64-71 units in a tester's log): only
				// across the ground counts.
				const auto         dx = controllerAt.x - a_actor->data.location.x;
				const auto         dy = controllerAt.y - a_actor->data.location.y;
				const float        offController = std::sqrt(dx * dx + dy * dy);
				const float        offDrawn = drawnAt.GetDistance(a_actor->data.location);
				// Every check while it lasts (20 a scene), not only when off: the
				// owner's chair shake (4 November) bounced the player a few
				// pixels up and down with nothing past 4 units here. With what
				// the controller stands on and how much it collides with.
				(void)offController;
				(void)offDrawn;
				{
					entry.last = now;
					++entry.count;
					const auto support = controller->supportBody.get();
					const auto supportNode = support ? support->GetSceneGraphObject() : nullptr;
					const auto supportRef = supportNode ? RE::TESObjectREFR::FindReferenceFor3D(supportNode) : nullptr;
					REX::INFO("Collision watch: {:08X} in \"{}\" at ({:.1f}, {:.1f}, {:.1f}); controller at ({:.1f}, {:.1f}, {:.1f}) ({:.1f} off across the ground), body drawn at ({:.1f}, {:.1f}, {:.1f}) ({:.1f} off); stands on {:08X} (layer {}), {} collision(s), state {}, fall time {:.2f}",
						a_actor->GetFormID(), a_sceneID, a_actor->data.location.x, a_actor->data.location.y, a_actor->data.location.z, controllerAt.x, controllerAt.y,
						controllerAt.z, offController, drawnAt.x, drawnAt.y, drawnAt.z, offDrawn, supportRef ? supportRef->GetFormID() : 0,
						std::to_underlying(controller->supportLayer.get()), controller->numCollisions, std::to_underlying(controller->wantState.get()), controller->fallTime);
				}
			}
		}
		if (!g_lockScenes || !g_settings.furnitureOutOfWay) {
			return;
		}
		RE::CFilter ownFilter{};
		ownFilter.filter = own->m_collisionFilterInfo;
		const auto                            group = ownFilter.GetSystemGroup();
		RE::BSScrapArray<const RE::hknpBody*> bodies;
		if (group == 0 || !controller->GetCurrentCollisionsImpl(bodies)) {
			return;
		}
		const auto id = a_actor->GetFormID();
		const auto support = controller->supportBody.get();
		const auto supportBody = support ? support->GetBody() : nullptr;
		for (const auto body : bodies) {
			if (!body || body == own || body == supportBody) {
				continue;
			}
			RE::CFilter filter{};
			filter.filter = body->m_collisionFilterInfo;
			const auto layer = filter.GetCollisionLayer();
			if (layer == RE::COL_LAYER::kCharController || layer == RE::COL_LAYER::kBiped || layer == RE::COL_LAYER::kTerrain || layer == RE::COL_LAYER::kGround) {
				continue;  // actors (the scene's share a group already), and the ground
			}
			const auto owner = OwnerOf(a_actor, body);
			if (!owner) {
				const bool first = g_touchLogged[id].insert(body).second;
				if (!g_settings.passThroughWalls) {
					if (first) {
						REX::INFO("Collision watch: {:08X} in \"{}\" touches a body no reference near them owns (layer {}, filter {:08X}; a precombined wall?): left as it is",
							id, a_sceneID, std::to_underlying(layer), filter.filter);
					}
					continue;
				}
				// A wall (precombined: no reference, so it can't join the
				// group). Wall scenes' spot is 15 units out from it, inside the
				// actors' capsules, so the wall pushed them about 10 units off
				// it and the lock put them back, every frame: the owner's wall
				// shake (6 November; with bTranslationLock=0 they sat 10-11
				// units off the spot instead). The actor's own controller then
				// collides with nothing for the scene: their gravity is 0 and
				// the lock holds them, so they don't need the floor. Back with
				// their filter (RestoreCollisionNow). Not in OStim (Skyrim's
				// lock leaves the capsule out of it).
				if (!ownFilter.QNoCollision()) {
					g_savedFilters.try_emplace(id, own->m_collisionFilterInfo);
					RE::CFilter none = ownFilter;
					none.SetNoCollision(true);
					const auto ok = controller->SetCollisionFilterInfo(none);
					REX::INFO("Collision watch: {:08X} in \"{}\" touches a body no reference near them owns (layer {}, filter {:08X}; a wall): {:08X} collides with nothing this scene ({})",
						id, a_sceneID, std::to_underlying(layer), filter.filter, id, ok);
					if (ok) {
						return;
					}
				}
				continue;
			}
			const auto object = owner->object.get();
			if (g_touchLogged[id].insert(object).second) {
				REX::INFO("Collision watch: {:08X} in \"{}\" touches {:08X} (base {:08X}, type {}, layer {}, filter {:08X})", id, a_sceneID, owner->ref, owner->base,
					owner->type, std::to_underlying(layer), filter.filter);
			}
			// Furniture already out of the way and still touched: the group
			// isn't enough there; no collision at all (its own filter is the
			// furniture's saved one).
			{
				std::scoped_lock lock(g_furnitureFiltersLock);
				bool             furniture = false;
				for (const auto& [ref, saved] : g_furnitureFilters) {
					if (std::ranges::any_of(saved, [&](const SavedBody& a_saved) { return a_saved.object.get() == object; })) {
						furniture = true;
						if (!filter.QNoCollision()) {
							filter.SetNoCollision(true);
							const auto ok = object->SetCollisionFilterInfo(filter);
							REX::WARN("Collision watch: {:08X} still touches furniture {:08X} in its collision group: no collision for it this scene ({})", id,
								owner->ref, ok);
						}
						break;
					}
				}
				if (furniture) {
					continue;
				}
			}
			auto&      list = g_touched[id];
			const auto it = std::ranges::find_if(list, [&](const TouchedBody& a_touched) { return a_touched.object.get() == object; });
			if (it == list.end()) {
				RE::CFilter changed = filter;
				changed.SetSystemGroup(group);
				if (object->SetCollisionFilterInfo(changed)) {
					list.push_back({ owner->object, filter.filter, false });
					REX::INFO("Collision watch: {:08X} ({:08X}) put in {:08X}'s collision group for the scene", owner->ref, filter.filter, id);
				}
			} else if (!it->noCollision) {
				filter.SetNoCollision(true);
				it->noCollision = true;
				const auto ok = object->SetCollisionFilterInfo(filter);
				REX::WARN("Collision watch: {:08X} still touches {:08X} in its collision group: no collision for it this scene ({})", id, owner->ref, ok);
			}
		}
	}
}
