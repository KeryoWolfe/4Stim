#include "Core/Core.h"

namespace Core::Natives::FourThreadBuilder
{
	// FourThreadBuilder (OThreadBuilder)
	std::int32_t Create(std::monostate, std::vector<RE::Actor*> a_actors) { return BuilderCreate(std::move(a_actors)); }
	void SetDominantActors(std::monostate, std::int32_t a_id, std::vector<RE::Actor*> a_actors)
	{
		WithBuilder(a_id, [&](ThreadBuilder& b) {
			b.dominant.clear();
			for (const auto actor : a_actors) {
				if (actor) {
					b.dominant.push_back(actor->GetFormID());
				}
			}
		});
	}
	void SetFurniture(std::monostate, std::int32_t a_id, RE::TESObjectREFR* a_ref)
	{
		WithBuilder(a_id, [&](ThreadBuilder& b) { b.furniture = a_ref ? a_ref->GetFormID() : 0; });
	}
	void SetDuration(std::monostate, std::int32_t a_id, float a_seconds)
	{
		WithBuilder(a_id, [&](ThreadBuilder& b) { b.duration = std::max(a_seconds, 0.0F); });
	}
	// As OStim's ThreadStartParams: an unknown scene is left out, with a warning.
	bool KnownStartingScene(const std::string& a_scene, std::string_view a_what)
	{
		if (SceneRegistry::Find(a_scene)) {
			return true;
		}
		REX::WARN("ThreadBuilder.{}: no scene \"{}\", left out", a_what, a_scene);
		return false;
	}
	void SetStartingAnimation(std::monostate, std::int32_t a_id, std::string a_scene)
	{
		if (!KnownStartingScene(a_scene, "SetStartingAnimation")) {
			return;
		}
		WithBuilder(a_id, [&](ThreadBuilder& b) {
			b.starting = { SceneRegistry::SequenceEntry{ a_scene, 0.0F, 0 } };
			b.navigateTo = { false };
		});
	}
	void AddStartingAnimation(std::monostate, std::int32_t a_id, std::string a_scene, float a_duration, bool a_navigateTo)
	{
		if (!KnownStartingScene(a_scene, "AddStartingAnimation")) {
			return;
		}
		WithBuilder(a_id, [&](ThreadBuilder& b) {
			b.navigateTo.resize(b.starting.size(), false);
			b.starting.push_back(SceneRegistry::SequenceEntry{ a_scene, a_duration, 0 });
			b.navigateTo.push_back(a_navigateTo);
		});
	}
	void SetStartingSequence(std::monostate, std::int32_t a_id, std::string a_sequence)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequence);
		if (!sequence) {
			REX::WARN("ThreadBuilder.SetStartingSequence: no sequence \"{}\"", a_sequence);
			return;
		}
		WithBuilder(a_id, [&](ThreadBuilder& b) {
			b.starting = sequence->entries;
			b.navigateTo.assign(b.starting.size(), false);
		});
	}
	void ConcatStartingSequence(std::monostate, std::int32_t a_id, std::string a_sequence, bool a_navigateTo)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequence);
		if (!sequence) {
			REX::WARN("ThreadBuilder.ConcatStartingSequence: no sequence \"{}\"", a_sequence);
			return;
		}
		WithBuilder(a_id, [&](ThreadBuilder& b) {
			b.navigateTo.resize(b.starting.size(), false);
			b.starting.insert(b.starting.end(), sequence->entries.begin(), sequence->entries.end());
			b.navigateTo.push_back(a_navigateTo);  // the way to its first scene
			b.navigateTo.resize(b.starting.size(), false);
		});
	}
	void EndAfterSequence(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.endAfterSequence = true; }); }
	void UndressActors(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.undress = true; }); }
	void NoAutoMode(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noAutoMode = true; }); }
	void NoPlayerControl(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noPlayerControl = true; }); }
	void NoUndressing(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noUndress = true; }); }
	void NoPostDialogue(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noPostDialogue = true; }); }
	void NoFurniture(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noFurniture = true; }); }
	void SetMetadata(std::monostate, std::int32_t a_id, std::vector<std::string> a_metadata)
	{
		WithBuilder(a_id, [&](ThreadBuilder& b) {
			b.metadata.clear();
			for (auto& m : a_metadata) {
				b.metadata.push_back(LowerCopy(m));
			}
		});
	}
	void SetMetadataCSV(std::monostate, std::int32_t a_id, std::string a_csv)
	{
		SetMetadata({}, a_id, SplitCSV(a_csv));
	}
	std::int32_t Start(std::monostate, std::int32_t a_id)
	{
		ThreadBuilder builder;
		{
			std::scoped_lock lock(g_buildersLock);
			const auto it = g_builders.find(a_id);
			if (it == g_builders.end()) {
				REX::WARN("ThreadBuilder.Start: no builder {}", a_id);
				return -1;
			}
			builder = std::move(it->second);
			g_builders.erase(it);
		}
		return StartThread(std::move(builder));
	}
	void Cancel(std::monostate, std::int32_t a_id)
	{
		std::scoped_lock lock(g_buildersLock);
		g_builders.erase(a_id);
	}
}

namespace Core
{
	void StartSoloScene()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto id = player ? BuilderCreate({ player }) : -1;
		if (id < 0 || Natives::FourThreadBuilder::Start({}, id) < 0) {
			Notify(TR("$4stim_solo_failed", "4Stim: couldn't start a scene with you alone (see 4Stim.log)"));
		}
	}
}
