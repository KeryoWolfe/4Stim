#include "Actions.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace Actions
{
	namespace
	{
		constexpr auto ACTION_FOLDER = "Data/F4SE/Plugins/4Stim/Actions";
		constexpr auto TAG_FOLDER = "Data/F4SE/Plugins/4Stim/Action Tags";  // OStim: action tags

		// OStim's action tags: forms every action with the tag gets, per side
		// ("-tag": the same with actor and target swapped). Read before the
		// actions.
		struct TagForms
		{
			ApiFormRefs actor, target, performer;
		};
		std::unordered_map<std::string, TagForms> g_tagForms;
		int                                       g_missingForms = 0;  // references to forms not loaded (no 4Stim.esl?)

		std::mutex g_lock;
		bool       g_loaded = false;
		// key: lowercase id or alias -> the type (aliases share the pointer)
		std::unordered_map<std::string, std::shared_ptr<const Type>> g_types;
		std::vector<std::string>                                     g_ids;
		// Types made by FindOrDefault for ids no file defines: a copy of
		// "default" under the id asked for. Cleared on reload.
		std::unordered_map<std::string, std::shared_ptr<const Type>> g_unknown;

		std::string Lower(std::string_view a_str)
		{
			std::string out(a_str);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::vector<std::string> LowerList(const nlohmann::json& a_obj, const char* a_key)
		{
			std::vector<std::string> out;
			if (const auto it = a_obj.find(a_key); it != a_obj.end() && it->is_array()) {
				for (const auto& v : *it) {
					if (v.is_string()) {
						out.push_back(Lower(v.get<std::string>()));
					}
				}
			} else if (it != a_obj.end() && it->is_string()) {
				out.push_back(Lower(it->get<std::string>()));  // one, as a plain string
			}
			return out;
		}

		// OStim's strippingSlots are Skyrim's biped slots; these are the
		// Fallout 4 slots each one covers (docs/UNDRESS.md). Slots Skyrim
		// uses for things Fallout 4 doesn't have (shield, tail, back...)
		// map to nothing.
		std::vector<int> FalloutSlotsFor(int a_skyrimSlot)
		{
			switch (a_skyrimSlot) {
			case 30:  // head
			case 31:  // hair
			case 41:  // long hair
			case 42:  // circlet
				return { 46 };
			case 32:  // body
				return { 33, 36, 39, 40, 41, 44, 45 };
			case 33:  // hands
				return { 34, 35 };
			case 34:  // forearms
			case 57:  // shoulder
			case 58:  // arm (secondary)
			case 59:  // arm (primary)
				return { 37, 38, 42, 43 };
			case 35:  // amulet
			case 45:  // neck
				return { 50 };
			case 36:  // ring
				return { 51 };
			case 37:  // feet: Fallout 4 has no feet slot, shoes are part of the legs
			case 38:  // calves
			case 53:  // leg (primary)
			case 54:  // leg (secondary)
				return { 39, 40, 44, 45 };
			case 44:  // face / mouth
			case 55:  // face (alternate)
				return { 49 };
			case 46:  // chest (primary)
			case 56:  // chest (secondary)
				return { 33, 36, 41 };
			case 49:  // pelvis (primary)
			case 52:  // pelvis (secondary)
				return { 33, 36, 39, 40, 44, 45 };
			default:
				return {};
			}
		}

		ApiFormRefs ParseForms(const nlohmann::json& a_side)
		{
			ApiFormRefs forms;
			const auto read = [&](const char* a_key, std::vector<std::uint32_t>& a_out) {
				if (const auto v = a_side.find(a_key); v != a_side.end()) {
					a_out = ReadForms(*v);
				}
			};
			read("faction", forms.factions);
			read("statFaction", forms.statFactions);
			read("playerStatFaction", forms.playerStatFactions);
			read("climaxStatFaction", forms.climaxStatFactions);
			read("partnerClimaxStatFaction", forms.partnerClimaxStatFactions);
			read("playerClimaxStatFaction", forms.playerClimaxStatFactions);
			read("playerPartnerClimaxStatFaction", forms.playerPartnerClimaxStatFactions);
			read("playerStatList", forms.playerStatLists);
			read("playerClimaxStatList", forms.playerClimaxStatLists);
			read("playerPartnerClimaxStatList", forms.playerPartnerClimaxStatLists);
			return forms;
		}

		void LoadTags()
		{
			g_tagForms.clear();
			std::error_code ec;
			if (!std::filesystem::is_directory(TAG_FOLDER, ec)) {
				return;
			}
			for (const auto& e : std::filesystem::directory_iterator(TAG_FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					if (!root.is_object()) {
						continue;
					}
					TagForms tag;
					const auto side = [&](const char* a_key) {
						const auto it = root.find(a_key);
						return it != root.end() && it->is_object() ? ParseForms(*it) : ApiFormRefs{};
					};
					tag.actor = side("actor");
					tag.target = side("target");
					tag.performer = side("performer");
					const auto name = Lower(e.path().stem().string());
					TagForms inverted{ tag.target, tag.actor, tag.performer };  // OStim's "-tag"
					g_tagForms[name] = std::move(tag);
					g_tagForms["-" + name] = std::move(inverted);
				} catch (const std::exception& ex) {
					REX::WARN("Actions: action tag {}: not valid JSON ({})", e.path().filename().string(), ex.what());
				}
			}
			REX::INFO("Actions: {} action tag(s)", g_tagForms.size() / 2);
		}

		// OStim's custom role data (GraphTableActions.cpp): keys lowercase,
		// string values too; entries of the wrong type are skipped.
		void ParseCustomData(const nlohmann::json& a_side, Side& a_out)
		{
			const auto each = [&](const char* a_key, auto&& a_read) {
				const auto it = a_side.find(a_key);
				if (it == a_side.end() || !it->is_object()) {
					return;
				}
				for (const auto& [key, value] : it->items()) {
					a_read(Lower(key), value);
				}
			};
			each("ints", [&](const std::string& k, const nlohmann::json& v) {
				if (v.is_number()) {
					a_out.ints[k] = v.get<int>();
				}
			});
			each("floats", [&](const std::string& k, const nlohmann::json& v) {
				if (v.is_number()) {
					a_out.floats[k] = v.get<float>();
				}
			});
			each("strings", [&](const std::string& k, const nlohmann::json& v) {
				if (v.is_string()) {
					a_out.strings[k] = Lower(v.get<std::string>());
				}
			});
			each("intLists", [&](const std::string& k, const nlohmann::json& v) {
				if (v.is_array()) {
					auto& list = a_out.intLists[k];
					for (const auto& e : v) {
						if (e.is_number()) {
							list.push_back(e.get<int>());
						}
					}
				}
			});
			each("floatLists", [&](const std::string& k, const nlohmann::json& v) {
				if (v.is_array()) {
					auto& list = a_out.floatLists[k];
					for (const auto& e : v) {
						if (e.is_number()) {
							list.push_back(e.get<float>());
						}
					}
				}
			});
			each("stringLists", [&](const std::string& k, const nlohmann::json& v) {
				if (v.is_array()) {
					auto& list = a_out.stringLists[k];
					for (const auto& e : v) {
						if (e.is_string()) {
							list.push_back(Lower(e.get<std::string>()));
						}
					}
				}
			});
		}

		Side ParseSide(const nlohmann::json& a_action, const char* a_key)
		{
			Side side;
			const auto it = a_action.find(a_key);
			if (it == a_action.end() || !it->is_object()) {
				return side;
			}
			if (const auto v = it->find("stimulation"); v != it->end() && v->is_number()) {
				side.stimulation = v->get<float>();
			}
			if (const auto v = it->find("maxStimulation"); v != it->end() && v->is_number()) {
				side.maxStimulation = v->get<float>();
			}
			side.requirements = LowerList(*it, "requirements");
			side.fullStrip = it->value("fullStrip", false);
			side.moan = it->value("moan", false);
			side.talk = it->value("talk", false);
			side.muffled = it->value("muffled", false);
			side.expressionOverride = Lower(it->value("expressionOverride", std::string{}));
			side.equipObjects = LowerList(*it, "equipObject");  // OStim's field name (a string or a list)
			side.toySlots = LowerList(*it, "toySlot");          // OStim's, a string or a list
			side.forms = ParseForms(*it);
			ParseCustomData(*it, side);
			if (const auto v = it->find("undressSlots"); v != it->end() && v->is_array()) {
				for (const auto& slot : *v) {
					if (slot.is_number_integer() && slot.get<int>() >= 30 && slot.get<int>() <= 61) {
						side.undressSlots.push_back(slot.get<int>());
					}
				}
			} else if (const auto skyrim = it->find("strippingSlots"); skyrim != it->end() && skyrim->is_array()) {
				// OStim's field: Skyrim slots, mapped to Fallout 4's.
				for (const auto& slot : *skyrim) {
					if (!slot.is_number_integer()) {
						continue;
					}
					for (const int fo4 : FalloutSlotsFor(slot.get<int>())) {
						if (std::ranges::find(side.undressSlots, fo4) == side.undressSlots.end()) {
							side.undressSlots.push_back(fo4);
						}
					}
				}
			}
			return side;
		}

		// A bone name or a list of them.
		std::vector<std::string> Bones(const nlohmann::json& a_object, const char* a_key)
		{
			std::vector<std::string> out;
			const auto it = a_object.find(a_key);
			if (it == a_object.end()) {
				return out;
			}
			if (it->is_string()) {
				out.push_back(it->get<std::string>());
			} else if (it->is_array()) {
				for (const auto& b : *it) {
					if (b.is_string()) {
						out.push_back(b.get<std::string>());
					}
				}
			}
			return out;
		}

		float Milliseconds(const nlohmann::json& a_object, const char* a_key, float a_default)
		{
			const auto it = a_object.find(a_key);
			return it != a_object.end() && it->is_number() ? it->get<float>() / 1000.0F : a_default;
		}

		// OStim's "peak": {"type": "loop", "interval": ms} or {"type":
		// "bonedistance", "actorBone", "targetBone", "inverse"}.
		Peak ParsePeak(const nlohmann::json& a_action, const std::string& a_id, const std::string& a_file)
		{
			Peak peak;
			const auto it = a_action.find("peak");
			if (it == a_action.end()) {
				return peak;
			}
			if (!it->is_object() || !it->contains("type")) {
				REX::WARN("Actions: {}: \"peak\" of action \"{}\" is malformed", a_file, a_id);
				return peak;
			}
			const auto type = Lower(it->value("type", std::string{}));
			if (type == "loop") {
				peak.kind = Peak::Kind::kLoop;
				peak.interval = std::max(Milliseconds(*it, "interval", 1.0F), 0.05F);
			} else if (type == "bonedistance") {
				peak.kind = Peak::Kind::kBoneDistance;
				peak.inverse = it->value("inverse", false);
				peak.minRange = it->value("minRange", 3.0F);
				peak.actorBones = Bones(*it, "actorBone");
				peak.targetBones = Bones(*it, "targetBone");
				if (peak.actorBones.empty() || peak.targetBones.empty()) {
					REX::WARN("Actions: {}: \"peak\" of action \"{}\" needs \"actorBone\" and \"targetBone\"", a_file, a_id);
					peak.kind = Peak::Kind::kNone;
				}
			} else {
				REX::WARN("Actions: {}: action \"{}\" has unknown peak type \"{}\"", a_file, a_id, type);
			}
			return peak;
		}

		// OStim's "sounds": [{"type": "peak" | "bonedistance" | "loop",
		// "sound": form (4Stim: or "file"), ...}].
		std::vector<ActionSound> ParseSounds(const nlohmann::json& a_action, const std::string& a_id, const std::string& a_file)
		{
			std::vector<ActionSound> out;
			const auto list = a_action.find("sounds");
			if (list == a_action.end() || !list->is_array()) {
				return out;
			}
			for (const auto& e : *list) {
				if (!e.is_object()) {
					continue;
				}
				ActionSound sound;
				sound.sound = Sounds::ReadSound(e);
				if (sound.sound.Empty()) {
					REX::WARN("Actions: {}: a sound of action \"{}\" has no usable \"sound\" or \"file\"", a_file, a_id);
					continue;
				}
				sound.muteWithActor = e.value("muteWithActor", false);
				sound.muteWithTarget = e.value("muteWithTarget", false);
				sound.playerThreadOnly = e.value("playerThreadOnly", false);
				const auto type = Lower(e.value("type", std::string{}));
				if (type == "peak") {
					sound.kind = ActionSound::Kind::kPeak;
				} else if (type == "bonedistance") {
					sound.kind = ActionSound::Kind::kBoneDistance;
					sound.inverse = e.value("inverse", false);
					sound.minRange = e.value("minRange", 3.0F);
					sound.minInterval = Milliseconds(e, "minInterval", 0.05F);
					sound.maxInterval = Milliseconds(e, "maxInterval", 0.0F);
					sound.actorBones = Bones(e, "actorBone");
					// OStim reads "actorBone" for the target's bones too (a slip);
					// 4Stim reads "targetBone".
					sound.targetBones = Bones(e, "targetBone");
					if (sound.actorBones.empty() || sound.targetBones.empty()) {
						REX::WARN("Actions: {}: a bonedistance sound of action \"{}\" needs \"actorBone\" and \"targetBone\"", a_file, a_id);
						continue;
					}
				} else if (type == "loop") {
					sound.kind = ActionSound::Kind::kLoop;
					sound.delay = Milliseconds(e, "delay", 0.0F);
				} else {
					REX::WARN("Actions: {}: action \"{}\" has a sound of unknown type \"{}\"", a_file, a_id, type);
					continue;
				}
				out.push_back(std::move(sound));
			}
			return out;
		}

		// "vaginalsex" -> "Vaginalsex"; files should give a "name".
		std::string DefaultName(const std::string& a_id)
		{
			auto name = a_id;
			if (!name.empty()) {
				name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
			}
			return name;
		}

		void AddType(const nlohmann::json& entry, const std::string& id, const std::string& file,
			std::unordered_map<std::string, std::shared_ptr<const Type>>& a_out, std::vector<std::string>& a_ids, int& a_loaded);

		// A file under Actions\: 4Stim's layout (an "actions" list) or
		// OStim's (one action per file, its id the file name).
		void LoadFile(const std::filesystem::path& a_path, std::unordered_map<std::string, std::shared_ptr<const Type>>& a_out,
			std::vector<std::string>& a_ids, int& a_loaded)
		{
			const auto     file = a_path.filename().string();
			nlohmann::json root;
			try {
				std::ifstream in(a_path);
				root = nlohmann::json::parse(in, nullptr, true, true);
			} catch (const std::exception& e) {
				REX::WARN("Actions: {}: not valid JSON: {}", file, e.what());
				return;
			}
			if (!root.is_object()) {
				REX::WARN("Actions: {}: not a JSON object", file);
				return;
			}
			const auto list = root.find("actions");
			if (list == root.end() || !list->is_array()) {
				// OStim's layout: this file is one action, its id the file name.
				AddType(root, Lower(a_path.stem().string()), file, a_out, a_ids, a_loaded);
				return;
			}
			for (const auto& entry : *list) {
				const auto id = entry.is_object() ? Lower(entry.value("id", std::string{})) : std::string{};
				if (id.empty()) {
					REX::WARN("Actions: {}: an action has no \"id\", skipped", file);
					continue;
				}
				AddType(entry, id, file, a_out, a_ids, a_loaded);
			}
		}

		void AddType(const nlohmann::json& entry, const std::string& id, const std::string& file,
			std::unordered_map<std::string, std::shared_ptr<const Type>>& a_out, std::vector<std::string>& a_ids, int& a_loaded)
		{
			{
				Type type;
				type.id = id;
				type.name = entry.value("name", DefaultName(id));
				type.tags = LowerList(entry, "tags");
				type.actor = ParseSide(entry, "actor");
				type.target = ParseSide(entry, "target");
				type.performer = ParseSide(entry, "performer");
				// As OStim's mergeTags: each tag's forms on the matching side.
				for (const auto& tag : type.tags) {
					if (const auto it = g_tagForms.find(tag); it != g_tagForms.end()) {
						type.actor.forms.Merge(it->second.actor);
						type.target.forms.Merge(it->second.target);
						type.performer.forms.Merge(it->second.performer);
					}
				}
				type.peak = ParsePeak(entry, id, file);
				type.sounds = ParseSounds(entry, id, file);
				type.sourceFile = file;

				if (const auto existing = a_out.find(id); existing != a_out.end()) {
					if (existing->second->id == id) {
						REX::WARN("Actions: {}: action \"{}\" replaces the one from {}", file, id, existing->second->sourceFile);
					}
				} else {
					a_ids.push_back(id);
				}
				const auto ptr = std::make_shared<const Type>(std::move(type));
				a_out[id] = ptr;
				for (const auto& alias : LowerList(entry, "aliases")) {
					if (alias == id) {
						continue;
					}
					const auto taken = a_out.find(alias);
					if (taken != a_out.end() && taken->second->id == alias) {
						REX::WARN("Actions: {}: alias \"{}\" of \"{}\" is another action's id, ignored", file, alias, id);
						continue;
					}
					a_out[alias] = ptr;
				}
				++a_loaded;
			}
		}

		void LoadLocked()
		{
			g_loaded = true;
			std::unordered_map<std::string, std::shared_ptr<const Type>> types;
			std::vector<std::string>                                     ids;

			g_missingForms = 0;
			LoadTags();
			std::error_code ec;
			if (!std::filesystem::is_directory(ACTION_FOLDER, ec)) {
				REX::INFO("Actions: no {} folder, no actions loaded", ACTION_FOLDER);
			} else {
				std::vector<std::filesystem::path> files;
				for (const auto& e : std::filesystem::directory_iterator(ACTION_FOLDER, ec)) {
					if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
						files.push_back(e.path());
					}
				}
				std::ranges::sort(files);  // later files win
				int loaded = 0;
				for (const auto& f : files) {
					LoadFile(f, types, ids, loaded);
				}
				REX::INFO("Actions: loaded {} action type(s) from {} file(s)", ids.size(), files.size());
				if (g_missingForms > 0) {
					REX::WARN("Actions: {} faction / form list reference(s) to forms that aren't loaded (is 4Stim.esl enabled?): no API factions for those", g_missingForms);
				}
			}
			std::ranges::sort(ids);
			g_types = std::move(types);
			g_ids = std::move(ids);
			g_unknown.clear();
		}
	}

	void ApiFormRefs::Merge(const ApiFormRefs& a_other)
	{
		const auto add = [](std::vector<std::uint32_t>& a_to, const std::vector<std::uint32_t>& a_from) {
			for (const auto id : a_from) {
				if (std::ranges::find(a_to, id) == a_to.end()) {
					a_to.push_back(id);
				}
			}
		};
		add(factions, a_other.factions);
		add(statFactions, a_other.statFactions);
		add(playerStatFactions, a_other.playerStatFactions);
		add(climaxStatFactions, a_other.climaxStatFactions);
		add(partnerClimaxStatFactions, a_other.partnerClimaxStatFactions);
		add(playerClimaxStatFactions, a_other.playerClimaxStatFactions);
		add(playerPartnerClimaxStatFactions, a_other.playerPartnerClimaxStatFactions);
		add(playerStatLists, a_other.playerStatLists);
		add(playerClimaxStatLists, a_other.playerClimaxStatLists);
		add(playerPartnerClimaxStatLists, a_other.playerPartnerClimaxStatLists);
	}

	bool ApiFormRefs::Empty() const
	{
		return factions.empty() && statFactions.empty() && playerStatFactions.empty() && climaxStatFactions.empty() && partnerClimaxStatFactions.empty() &&
		       playerClimaxStatFactions.empty() && playerPartnerClimaxStatFactions.empty() && playerStatLists.empty() && playerClimaxStatLists.empty() &&
		       playerPartnerClimaxStatLists.empty();
	}

	std::vector<std::uint32_t> ReadForms(const nlohmann::json& a_value)
	{
		std::vector<std::uint32_t> out;
		const auto one = [&](const nlohmann::json& a_ref) {
			if (!a_ref.is_object()) {
				return;
			}
			auto ref = a_ref;
			if (_stricmp(ref.value("mod", std::string{}).c_str(), "OStim.esp") == 0) {
				ref["mod"] = "4Stim.esl";
			}
			if (const auto id = Sounds::ReadForm(ref); id) {
				out.push_back(id);
			} else {
				++g_missingForms;
			}
		};
		if (a_value.is_array()) {
			for (const auto& v : a_value) {
				one(v);
			}
		} else {
			one(a_value);
		}
		return out;
	}

	bool Type::HasTag(std::string_view a_tag) const
	{
		const auto tag = Lower(a_tag);
		return std::ranges::find(tags, tag) != tags.end();
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}

	std::shared_ptr<const Type> Find(std::string_view a_idOrAlias)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto it = g_types.find(Lower(a_idOrAlias));
		return it != g_types.end() ? it->second : nullptr;
	}

	std::shared_ptr<const Type> FindOrDefault(std::string_view a_idOrAlias)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto key = Lower(a_idOrAlias);
		if (const auto it = g_types.find(key); it != g_types.end()) {
			return it->second;
		}
		if (const auto it = g_unknown.find(key); it != g_unknown.end()) {
			return it->second;
		}
		// As OStim's getActionAttributesByType: the "default" action's
		// attributes, while the action keeps the type it was given (so
		// lookups by that id still find it). Shown by its id, not "Default".
		auto type = std::make_shared<Type>();
		if (const auto def = g_types.find("default"); def != g_types.end()) {
			*type = *def->second;
		}
		type->id = key;
		type->name = key;
		REX::WARN("Actions: no action found for \"{}\", using default (as OStim)", key);
		std::shared_ptr<const Type> shared = type;
		g_unknown.emplace(key, shared);
		return shared;
	}

	std::string ResolveAlias(std::string_view a_idOrAlias)
	{
		const auto type = Find(a_idOrAlias);
		return type ? type->id : Lower(a_idOrAlias);
	}

	std::vector<std::string> List()
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		return g_ids;
	}

}
