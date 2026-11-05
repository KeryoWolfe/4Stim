#include "AnimCues.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "RE/B/BSResourceNiBinaryStream.h"
#include "RE/T/TESIdleForm.h"

namespace AnimCues
{
	namespace
	{
		struct Cue
		{
			float       time = 0.0F;
			std::string tag;
			std::string payload;
		};

		struct Clip
		{
			float            duration = 0.0F;
			std::vector<Cue> cues;  // by time
		};

		struct Playing
		{
			std::shared_ptr<const Clip> clip;     // null while the file is being read
			std::string                 key;      // its file, lowercase
			bool                        loops = false;
			float                       time = 0.0F;  // in the clip
			std::size_t                 next = 0;     // the next cue to fire
		};

		std::mutex                                                     g_lock;
		std::unordered_map<std::string, std::shared_ptr<const Clip>>   g_clips;    // by file (lowercase)
		std::unordered_set<std::string>                                g_reading;  // files a worker is reading
		std::unordered_map<std::uint32_t, Playing>                     g_playing;  // by actor
		std::atomic<bool>                                              g_engine = false;

		template <class T>
		bool ReadAt(const std::vector<char>& a_bytes, std::size_t a_offset, T& a_out)
		{
			if (a_offset + sizeof(T) > a_bytes.size()) {
				return false;
			}
			std::memcpy(&a_out, a_bytes.data() + a_offset, sizeof(T));
			return true;
		}

		// A Havok 2014 binary packfile's annotations and clip length. The
		// section headers (0x40 bytes each: a 20-byte name, then the data
		// start and the local, global, virtual fixup, export, import and end
		// offsets) follow "__classnames__". The __data__ section's local
		// fixups point each hkStringPtr at its text: an annotation
		// (hkaAnnotationTrack::Annotation: hkReal time, then the text at +8)
		// is a 4Stim text with its time 8 bytes before the pointer. The
		// virtual fixups name each object's class: the hkaAnimation one
		// (hkaSplineCompressedAnimation...) has its duration at +0x14.
		// Checked against AnimBridge's files (MLC's table spank: spankright
		// at 0.42 s of 1.27 s; OStim's standing spank: spankleft at 1.17 s
		// of 2.67 s). Anything else (a tagfile, an older layout) reads as
		// no cues.
		Clip Parse(const std::vector<char>& a_bytes)
		{
			Clip clip;
			std::int32_t sections = 0;
			if (!ReadAt(a_bytes, 0x14, sections) || sections <= 0 || sections > 8) {
				return clip;
			}
			const std::string_view all(a_bytes.data(), a_bytes.size());
			const auto             headers = all.find("__classnames__");
			if (headers == std::string_view::npos) {
				return clip;
			}
			struct Section
			{
				std::string   name;
				std::uint32_t start = 0, local = 0, global = 0, virt = 0, exports = 0, imports = 0, end = 0;
			};
			std::vector<Section> list;
			for (std::int32_t i = 0; i < sections; ++i) {
				const std::size_t at = headers + static_cast<std::size_t>(i) * 0x40;
				if (at + 0x40 > a_bytes.size()) {
					return clip;
				}
				Section s;
				s.name.assign(a_bytes.data() + at, strnlen(a_bytes.data() + at, 20));
				std::uint32_t v[7]{};
				for (int k = 0; k < 7; ++k) {
					ReadAt(a_bytes, at + 20 + static_cast<std::size_t>(k) * 4, v[k]);
				}
				s.start = v[0];
				s.local = v[1];
				s.global = v[2];
				s.virt = v[3];
				s.exports = v[4];
				s.imports = v[5];
				s.end = v[6];
				list.push_back(std::move(s));
			}
			const auto find = [&](std::string_view a_name) { return std::ranges::find(list, a_name, &Section::name); };
			const auto data = find("__data__");
			const auto names = find("__classnames__");
			if (data == list.end() || names == list.end() || data->start + data->local > a_bytes.size()) {
				return clip;
			}
			const std::size_t base = data->start;
			const auto        text = [&](std::size_t a_offset) {
                const std::size_t at = base + a_offset;
                if (at >= a_bytes.size()) {
                    return std::string{};
                }
                return std::string(a_bytes.data() + at, strnlen(a_bytes.data() + at, std::min<std::size_t>(256, a_bytes.size() - at)));
			};
			// Local fixups: (pointer at, points to) pairs, -1 for padding.
			for (std::size_t p = base + data->local; p + 8 <= base + data->global && p + 8 <= a_bytes.size(); p += 8) {
				std::int32_t src = 0, dst = 0;
				ReadAt(a_bytes, p, src);
				ReadAt(a_bytes, p + 4, dst);
				if (src < 8 || dst < 0) {
					continue;
				}
				auto str = text(static_cast<std::size_t>(dst));
				if (str.size() < 6 || _strnicmp(str.c_str(), "4Stim", 5) != 0) {
					continue;
				}
				float time = 0.0F;
				if (!ReadAt(a_bytes, base + static_cast<std::size_t>(src) - 8, time) || !(time >= 0.0F) || time > 3600.0F) {
					continue;
				}
				Cue cue;
				cue.time = time;
				if (const auto dot = str.find('.'); dot != std::string::npos) {
					cue.tag = str.substr(0, dot);
					cue.payload = str.substr(dot + 1);
				} else {
					cue.tag = std::move(str);
				}
				clip.cues.push_back(std::move(cue));
			}
			// Virtual fixups: (object at, class name section, class name at).
			for (std::size_t p = base + data->virt; p + 12 <= base + data->exports && p + 12 <= a_bytes.size(); p += 12) {
				std::int32_t src = 0, section = 0, nameAt = 0;
				ReadAt(a_bytes, p, src);
				ReadAt(a_bytes, p + 4, section);
				ReadAt(a_bytes, p + 8, nameAt);
				if (src < 0 || nameAt < 0) {
					continue;
				}
				const std::size_t at = names->start + static_cast<std::size_t>(nameAt);
				if (at >= a_bytes.size()) {
					continue;
				}
				const std::string cls(a_bytes.data() + at, strnlen(a_bytes.data() + at, std::min<std::size_t>(64, a_bytes.size() - at)));
				if (cls.find("Animation") == std::string::npos || cls.find("Container") != std::string::npos || cls.find("Binding") != std::string::npos) {
					continue;
				}
				float duration = 0.0F;
				if (ReadAt(a_bytes, base + static_cast<std::size_t>(src) + 0x14, duration) && duration > 0.0F && duration < 3600.0F) {
					clip.duration = duration;
					break;
				}
			}
			std::ranges::stable_sort(clip.cues, {}, &Cue::time);
			return clip;
		}

		std::string Key(const std::string& a_file)
		{
			std::string key = a_file;
			std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return key;
		}

		// Reads and parses a_file on a worker thread: never on the main
		// thread, and never through the game's own file system first. The
		// first build read it on the main thread with BSResourceNiBinaryStream
		// right after PlayIdle had asked the game to load the same file, and
		// froze the game (the owner's "Turn around" from a standing hold, 5
		// November: the log stopped there, no crash). Loose files (Mod
		// Organizer's virtual Data folder included) are read with plain file
		// reads; the game's file system is only tried for a file that isn't
		// loose (one in a .ba2), and only here, off the main thread.
		void ReadAsync(const std::string& a_file, const std::string& a_key)
		{
			{
				std::scoped_lock lock(g_lock);
				if (g_clips.contains(a_key) || !g_reading.insert(a_key).second) {
					return;
				}
			}
			std::thread([a_file, a_key]() {
				std::vector<char> bytes;
				bool              opened = false;
				if (std::ifstream in("Data\\Meshes\\" + a_file, std::ios::binary); in) {
					opened = true;
					bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				} else {
					RE::BSResourceNiBinaryStream stream(("Meshes\\" + a_file).c_str());
					if (stream) {
						opened = true;
						std::vector<char> chunk(1 << 16);
						while (bytes.size() < (16u << 20)) {
							const auto read = stream.binary_read(chunk.data(), chunk.size());
							if (read == 0) {
								break;
							}
							bytes.insert(bytes.end(), chunk.data(), chunk.data() + read);
						}
					}
				}
				auto clip = std::make_shared<Clip>(Parse(bytes));
				if (!opened) {
					REX::WARN("Animation cues: Meshes\\{} couldn't be opened", a_file);
				} else if (!clip->cues.empty()) {
					std::string list;
					for (const auto& cue : clip->cues) {
						list += std::format("{}{}{}{} at {:.2f}s", list.empty() ? "" : ", ", cue.tag, cue.payload.empty() ? "" : ".", cue.payload, cue.time);
					}
					REX::INFO("Animation cues: {} ({:.2f}s): {}", a_file, clip->duration, list);
				}
				std::scoped_lock lock(g_lock);
				g_clips.emplace(a_key, std::move(clip));
				g_reading.erase(a_key);
			}).detach();
		}
	}

	void Played(std::uint32_t a_actor, const RE::TESIdleForm* a_idle, bool a_loops)
	{
		const char* file = a_idle ? a_idle->animFileName.c_str() : nullptr;
		if (!file || !*file) {
			Stop({ a_actor });
			return;
		}
		const auto key = Key(file);
		{
			std::scoped_lock lock(g_lock);
			const auto it = g_clips.find(key);
			if (it != g_clips.end() && it->second->cues.empty()) {
				g_playing.erase(a_actor);
				return;
			}
			g_playing[a_actor] = { it != g_clips.end() ? it->second : nullptr, key, a_loops, 0.0F, 0 };
			if (it != g_clips.end()) {
				return;
			}
		}
		ReadAsync(file, key);  // the clock counts from now; its cues start once it's read
	}

	void Stop(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_actors) {
			g_playing.erase(id);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_playing.clear();
	}

	std::vector<Due> Tick(float a_seconds, const std::function<float(std::uint32_t)>& a_rate)
	{
		std::vector<Due> due;
		if (g_engine || a_seconds <= 0.0F) {
			return due;
		}
		std::scoped_lock lock(g_lock);
		for (auto it = g_playing.begin(); it != g_playing.end();) {
			auto&       p = it->second;
			const float advance = a_seconds * std::max(a_rate(it->first), 0.0F);
			bool        counted = false;
			if (!p.clip) {
				// Still being read: count the time. Once it's in, cues already
				// more than a moment behind are skipped, not fired in a burst.
				p.time += advance;
				counted = true;
				const auto found = g_clips.find(p.key);
				if (found == g_clips.end()) {
					++it;
					continue;
				}
				if (found->second->cues.empty()) {
					it = g_playing.erase(it);
					continue;
				}
				p.clip = found->second;
				if (p.clip->duration > 0.0F && p.loops) {
					p.time = std::fmod(p.time, p.clip->duration);
				}
				while (p.next < p.clip->cues.size() && p.clip->cues[p.next].time < p.time - 0.1F) {
					++p.next;
				}
			}
			const auto& cues = p.clip->cues;
			const float duration = p.clip->duration;
			if (!counted) {
				p.time += advance;
			}
			bool done = false;
			// At most a few laps a tick (a very short clip at a high rate).
			for (int lap = 0; lap < 4; ++lap) {
				const float until = duration > 0.0F ? std::min(p.time, duration) : p.time;
				while (p.next < cues.size() && cues[p.next].time <= until) {
					due.push_back({ it->first, cues[p.next].tag, cues[p.next].payload });
					++p.next;
				}
				if (duration <= 0.0F || p.time < duration) {
					break;
				}
				if (!p.loops) {
					done = true;
					break;
				}
				p.time -= duration;
				p.next = 0;
			}
			it = done ? g_playing.erase(it) : std::next(it);
		}
		return due;
	}

	void EngineForwarded(const char* a_tag)
	{
		if (!g_engine.exchange(true)) {
			REX::INFO("Animation cues: the game sent \"{}\" as an animation event itself: 4Stim stops timing annotation cues from the files", a_tag ? a_tag : "");
		}
	}
}
