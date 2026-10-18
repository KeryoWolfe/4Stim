// 4Stim: commonlib-shared's REL/IDDB.cpp (github.com/libxse/commonlib-shared,
// commit 9fbb74d, GPL-3.0 with its EXCEPTIONS), compiled into 4Stim in place
// of the library's copy (the linker takes this one; the library's IDDB.obj
// is never pulled in). Two changes, marked "4Stim":
//
// 1. Older game versions (1.10.163 "old-gen"): 4Stim's addresses are AE
//    Address Library IDs, and that game's Address Library uses another ID
//    space. So 4Stim ships its own table for it,
//    Data\F4SE\Plugins\4Stim\Addresses\version-<game version>.txt ("<AE ID>
//    <RVA>" per line, made from the CommonLibF4RD Runtime Database), and
//    reads it instead when it's there.
// 2. offset() fails on an ID that isn't in the table, instead of returning
//    the next ID's address.
// 3. Any version but AE's (1.11.x with its Address Library): CommonLibF4RD's
//    Runtime Database (Data\F4SE\Plugins\f4rd-runtime.bin), through
//    CommonLibF4RD's own resolver (compat/rd/): the addresses it knows for
//    the running version, and the rest found by searching the game's code,
//    as every CommonLibF4RD plugin does. 4Stim's own table fills in what
//    the database can't (1.10.163).
#include "REL/IDDB.h"

#include "Addresses.h"
#include "rd/RuntimeDatabase.h"

#include "REL/Version.h"

#include "REX/FModule.h"
#include "REX/HASH.h"
#include "REX/LOG.h"
#include "REX/W32/KERNEL32.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace REL
{
	class IDDB::STREAM
	{
	public:
		using stream_type = std::ifstream;
		using pointer = stream_type*;
		using const_pointer = const stream_type*;
		using reference = stream_type&;
		using const_reference = const stream_type&;

		STREAM(const std::filesystem::path a_path, const std::ios_base::openmode a_mode) :
			_stream(a_path, a_mode)
		{
			if (!_stream.is_open())
				REX::FAIL(L"Failed to open Address Library file!\nPath: {}", a_path.wstring());

			_stream.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
		}

		void ignore(const std::streamsize a_count)
		{
			_stream.ignore(a_count);
		}

		template <class T>
		void readin(T& a_val)
		{
			_stream.read(std::bit_cast<char*>(std::addressof(a_val)), sizeof(T));
		}

		template <class T>
			requires(std::is_arithmetic_v<T>)
		T readout()
		{
			T val{};
			readin(val);
			return val;
		}

	private:
		stream_type _stream;
	};

	class IDDB::HEADER_V2
	{
	public:
		HEADER_V2(IDDB::STREAM& a_in)
		{
			a_in.readin(m_gameVersion);

			const auto nameLen = a_in.readout<std::uint32_t>();
			for (std::uint32_t i = 0; i < nameLen; i++)
				a_in.readin(m_name[i]);

			m_name[nameLen] = '\0';

			a_in.readin(m_pointerSize);
			a_in.readin(m_addressCount);
		}

		std::string_view name() const noexcept { return m_name; }
		std::size_t      address_count() const noexcept { return static_cast<std::size_t>(m_addressCount); }
		std::uint64_t    pointer_size() const noexcept { return static_cast<std::uint64_t>(m_pointerSize); }

		Version game_version() const noexcept
		{
			Version version;
			for (std::size_t i = 0; i < 4; i++)
				version[i] = static_cast<std::uint16_t>(m_gameVersion[i]);

			return version;
		}

	private:
		char          m_name[64]{};
		std::uint32_t m_gameVersion[4]{};
		std::int32_t  m_pointerSize{ 0 };
		std::int32_t  m_addressCount{ 0 };
	};

	class IDDB::HEADER_V5
	{
	public:
		HEADER_V5(IDDB::STREAM& a_in)
		{
			a_in.readin(m_gameVersion);
			a_in.readin(m_name);
			a_in.readin(m_pointerSize);
			a_in.readin(m_dataFormat);
			a_in.readin(m_offsetCount);
		}

		std::string_view name() const noexcept { return m_name; }
		std::size_t      offset_count() const noexcept { return static_cast<std::size_t>(m_offsetCount); }

		Version game_version() const noexcept
		{
			Version version;
			for (std::size_t i = 0; i < 4; i++)
				version[i] = static_cast<std::uint16_t>(m_gameVersion[i]);

			return version;
		}

	private:
		std::int32_t  m_fileVersion{ 5 };
		std::uint32_t m_gameVersion[4]{};
		char          m_name[64]{};
		std::int32_t  m_pointerSize{ 0 };
		std::int32_t  m_dataFormat{ 0 };
		std::int32_t  m_offsetCount{ 0 };
	};
}

namespace
{
	// 4Stim: CommonLibF4RD's Runtime Database, when it's the source.
	std::unique_ptr<RD::RuntimeDatabase> gRuntime;
	std::filesystem::path                gRuntimePath;
	std::filesystem::path                gRuntimeLooked;  // where it was looked for (the log)

	std::optional<std::uint64_t> FromRuntime(std::uint64_t a_id, RD::RuntimeDatabase::ResolveFailure* a_failure = nullptr)
	{
		if (!gRuntime)
			return std::nullopt;
		const auto& module = RD::Module::get();
		const auto  result = gRuntime->resolve_detailed(a_id, module.version(), module, RD::RuntimeDatabase::ResolveMode::kNormal);
		if (a_failure)
			*a_failure = result.failure;
		return result ? std::optional<std::uint64_t>{ static_cast<std::uint64_t>(*result.rva) } : std::nullopt;
	}

	std::optional<std::uint64_t> FromTable(std::uint64_t a_id);

	const char* FailureName(RD::RuntimeDatabase::ResolveFailure a_failure)
	{
		using F = RD::RuntimeDatabase::ResolveFailure;
		switch (a_failure) {
		case F::kUnknownID:
			return "not in the database";
		case F::kNoCompatiblePattern:
			return "no search pattern for this version";
		case F::kPatternNotFound:
			return "its pattern isn't in the game's code";
		case F::kPatternAmbiguous:
			return "its pattern matches more than one place";
		case F::kInvalidPattern:
			return "invalid pattern";
		case F::kRuntimeUnavailable:
			return "not in this game version";
		case F::kSemanticValidationFailed:
			return "found, but failed validation";
		default:
			return "unresolved";
		}
	}

	// 4Stim: the address table for this game version, sorted by ID.
	std::vector<REL::IDDB::MAPPING> g4StimTable;
	std::filesystem::path           g4StimTablePath;

	// "<id> <rva>" per line (the RVA in hex with 0x, or decimal); lines
	// starting with # are comments. The format of CommonLibF4RD's .mapping
	// files, so one can be used as it is.
	bool Load4StimTable(const std::filesystem::path& a_path)
	{
		std::ifstream in(a_path);
		if (!in)
			return false;
		std::string line;
		while (std::getline(in, line)) {
			const auto start = line.find_first_not_of(" \t");
			if (start == std::string::npos || line[start] == '#')
				continue;
			std::istringstream fields(line.substr(start));
			std::string        id, rva;
			if (!(fields >> id >> rva))
				continue;
			try {
				g4StimTable.push_back({ std::stoull(id, nullptr, 10), std::stoull(rva, nullptr, 0) });
			} catch (...) {
				return false;
			}
		}
		std::sort(g4StimTable.begin(), g4StimTable.end(), [](auto&& a_lhs, auto&& a_rhs) { return a_lhs.id < a_rhs.id; });
		return !g4StimTable.empty();
	}

	std::optional<std::uint64_t> FromTable(std::uint64_t a_id)
	{
		const auto it = std::lower_bound(g4StimTable.begin(), g4StimTable.end(), REL::IDDB::MAPPING{ a_id, 0 },
			[](auto&& a_lhs, auto&& a_rhs) { return a_lhs.id < a_rhs.id; });
		return it != g4StimTable.end() && it->id == a_id ? std::optional<std::uint64_t>{ it->offset } : std::nullopt;
	}
}

namespace REL
{
	IDDB::IDDB()
	{
		std::unordered_map<Loader, std::vector<std::wstring>> g_rootMap{
			{ Loader::SKSE, { L"versionlib", L"version" } },
			{ Loader::F4SE, { L"version" } },
			{ Loader::SFSE, { L"versionlib" } },
			{ Loader::OBSE, { L"versionlib" } },
		};

		std::unordered_map<Loader, std::pair<std::string, std::wstring>> g_loaderMap{
			{ Loader::SKSE, { "SKSE", L"SKSE" } },
			{ Loader::F4SE, { "F4SE", L"F4SE" } },
			{ Loader::SFSE, { "SFSE", L"SFSE" } },
			{ Loader::OBSE, { "OBSE", L"OBSE" } },
		};

		wchar_t buffer[REX::W32::MAX_PATH];
		REX::W32::GetModuleFileNameW(REX::W32::GetCurrentModule(), buffer, REX::W32::MAX_PATH);
		std::filesystem::path plugin(buffer);

		m_name = plugin.filename().string();
		auto loader = plugin.parent_path().parent_path();
		auto loaderName = loader.filename().wstring();
		std::transform(loaderName.begin(), loaderName.end(), loaderName.begin(), std::towupper);
		for (auto& entry : g_loaderMap)
			if (loaderName == entry.second.second) {
				m_loader = entry.first;
				break;
			}

		if (m_loader == Loader::None)
			REX::FAIL(
				"{}\n\n"
				"Failed to determine Address Library loader!\n"
				"Ensure the plugin is installed in the correct directory.",
				m_name);

		const auto mod = REX::FModule::GetExecutingModule();
		const auto version = mod.GetFileVersion().wstring(L"-");

		// 4Stim: AE (1.11.x) with its Address Library: as CommonLibF4 does
		// (below). Any other version, or AE without it: the Runtime
		// Database, with our own table (if any) behind it.
		const bool ae = mod.GetFileVersion() >= REL::Version{ 1, 11, 0, 0 };
		const auto addressLibrary = plugin.parent_path() / std::format(L"version-{}.bin", version);
		// The same places CommonLibF4RD looks: f4rd-runtime.bin, or
		// f4rd-runtime-<version>.bin, beside the plugin or in
		// Data\F4SE\Plugins under the game's folder.
		std::filesystem::path runtime;
		for (const auto& dir : { plugin.parent_path(), std::filesystem::path(L"Data") / L"F4SE" / L"Plugins" }) {
			for (const auto& name : { std::wstring(L"f4rd-runtime.bin"), std::format(L"f4rd-runtime-{}.bin", version) }) {
				std::error_code ec;
				if (runtime.empty() && std::filesystem::exists(dir / name, ec))
					runtime = dir / name;
			}
		}
		gRuntimeLooked = plugin.parent_path() / L"f4rd-runtime.bin";
		if (!(ae && std::filesystem::exists(addressLibrary)) && !runtime.empty()) {
			try {
				gRuntime = RD::RuntimeDatabase::load(runtime);
			} catch (const std::exception& e) {
				REX::FAIL(L"{}\n\nFailed to read the Runtime Database (f4rd-runtime.bin): {}\nReinstall Runtime Database.", plugin.filename().wstring(),
					std::wstring(e.what(), e.what() + std::strlen(e.what())));
			}
			if (gRuntime) {
				gRuntimePath = runtime;
				m_path = runtime;
				const auto table = plugin.parent_path() / L"4Stim" / L"Addresses" / std::format(L"version-{}.txt", version);
				if (std::filesystem::exists(table) && Load4StimTable(table))
					g4StimTablePath = table;
				return;
			}
		}

		// 4Stim: our own table for this game version, if we have one.
		{
			const auto table = plugin.parent_path() / L"4Stim" / L"Addresses" / std::format(L"version-{}.txt", version);
			if (std::filesystem::exists(table)) {
				if (!Load4StimTable(table))
					REX::FAIL(L"{}\n\nFailed to read 4Stim's address table!\nPath: {}", plugin.filename().wstring(), table.wstring());
				m_path = table;
				g4StimTablePath = table;
				m_format = Format::V0;
				m_v0 = { g4StimTable.data(), g4StimTable.size() };
				return;
			}
		}

		for (const auto& root : g_rootMap[m_loader]) {
			const auto name = std::format(L"{}-{}.bin", root, version);
			const auto path = plugin.parent_path() / name;
			if (std::filesystem::exists(path)) {
				if (m_loader == Loader::F4SE && root == L"version")
					m_format = Format::V0;

				m_path = path;
				break;
			}
		}

		if (m_path.empty())
			REX::FAIL(
				"{}\n\n"
				"Failed to determine Address Library path!\n"
				"Redownload the latest Address Library for your game version.\n"
				"Loader: {}\n"
				"Game Version: {}",
				m_name, g_loaderMap[m_loader].first, mod.GetFileVersion().string());

		if (m_format == Format::V0) {
			load_v0();
			return;
		}

		STREAM     stream(m_path, std::ios::in | std::ios::binary);
		const auto format = stream.readout<std::uint32_t>();
		if (format < 1 || (format > 2 && format < 5) || format > 5)
			REX::FAIL(
				"{}\n\n"
				"Unsupported Address Library format: {}\n"
				"The plugin needs to be updated and recompiled for the current game version.\n",
				m_name, format);

		m_format = static_cast<Format>(format);
		if (m_format == Format::V1 || m_format == Format::V2) {
			load_v2(stream);
		} else if (m_format == Format::V5) {
			load_v5(stream);
		}
	}

	void IDDB::load_v0()
	{
		const auto mod = REX::FModule::GetExecutingModule();
		const auto mapName = std::format("COMMONLIB_IDDB_OFFSETS_{}", mod.GetFileVersion().string("_"));
		if (!m_mmap.create(false, m_path, mapName))
			REX::FAIL(L"Failed to create Address Library MemoryMap!\nError: {}\nPath: {}", REX::W32::GetLastError(), m_path.wstring());

		validate_file();

		m_v0 = {
			reinterpret_cast<MAPPING*>(m_mmap.data() + sizeof(std::uint64_t)),
			*reinterpret_cast<std::uint64_t*>(m_mmap.data())
		};
	}

	void IDDB::load_v2(STREAM& a_stream)
	{
		try {
			HEADER_V2 header(a_stream);

			const auto mod = REX::FModule::GetExecutingModule();
			const auto modVersion = mod.GetFileVersion();
			if (header.game_version() != modVersion) {
				REX::FAIL(
					"{}\n\n"
					"Address Library version mismatch!\n"
					"Redownload the latest Address Library for your game version.\n"
					"Expected Version: {}\n"
					"Actual Version: {}",
					m_name, modVersion.string(), header.game_version().string());
			}

			const auto mapName = std::format("COMMONLIB_IDDB_OFFSETS_{}", modVersion.string("_"));
			const auto byteSize = static_cast<std::size_t>(header.address_count()) * sizeof(MAPPING);
			if (!m_mmap.create(true, mapName, byteSize))
				REX::FAIL("Failed to create Address Library MemoryMap!\nError: {}", REX::W32::GetLastError());

			validate_file();

			m_v0 = { reinterpret_cast<MAPPING*>(m_mmap.data()), header.address_count() };

			if (m_mmap.is_owner()) {
				unpack_file(a_stream, header);
				std::sort(
					m_v0.begin(),
					m_v0.end(),
					[](auto&& a_lhs, auto&& a_rhs) {
						return a_lhs.id < a_rhs.id;
					});
			}
		} catch (const std::system_error&) {
			REX::FAIL(L"Failed to open Address Library file!\nPath: {}", m_path.wstring());
		}
	}

	void IDDB::load_v5(STREAM& a_stream)
	{
		try {
			HEADER_V5 header(a_stream);

			const auto mod = REX::FModule::GetExecutingModule();
			const auto modVersion = mod.GetFileVersion();
			if (header.game_version() != modVersion) {
				REX::FAIL(
					"{}\n\n"
					"Address Library version mismatch!\n"
					"Redownload the latest Address Library for your game version.\n"
					"Expected Version: {}\n"
					"Actual Version: {}",
					m_name, modVersion.string(), header.game_version().string());
			}

			const auto mapName = std::format("COMMONLIB_IDDB_OFFSETS_{}", modVersion.string("_"));
			if (!m_mmap.create(false, m_path, mapName))
				REX::FAIL(L"Failed to create Address Library MemoryMap!\nError: {}\nPath: {}", REX::W32::GetLastError(), m_path.wstring());

			validate_file();

			m_v5 = { reinterpret_cast<std::uint32_t*>(m_mmap.data() + sizeof(HEADER_V5)), header.offset_count() };

		} catch (const std::system_error&) {
			REX::FAIL(L"Failed to open Address Library file!\nPath: {}", m_path.wstring());
		}
	}

	void IDDB::unpack_file(STREAM& a_stream, const HEADER_V2& a_header)
	{
		std::uint8_t  type = 0;
		std::uint64_t id = 0;
		std::uint64_t offset = 0;
		std::uint64_t prevID = 0;
		std::uint64_t prevOffset = 0;
		for (auto& mapping : m_v0) {
			a_stream.readin(type);
			const auto lo = static_cast<std::uint8_t>(type & 0xF);
			const auto hi = static_cast<std::uint8_t>(type >> 4);

			switch (lo) {
				case 0:
					a_stream.readin(id);
					break;
				case 1:
					id = prevID + 1;
					break;
				case 2:
					id = prevID + a_stream.readout<std::uint8_t>();
					break;
				case 3:
					id = prevID - a_stream.readout<std::uint8_t>();
					break;
				case 4:
					id = prevID + a_stream.readout<std::uint16_t>();
					break;
				case 5:
					id = prevID - a_stream.readout<std::uint16_t>();
					break;
				case 6:
					id = a_stream.readout<std::uint16_t>();
					break;
				case 7:
					id = a_stream.readout<std::uint32_t>();
					break;
				default:
					REX::FAIL(
						"{}\n\n"
						"Unhandled type while loading Address Library!\n"
						"The plugin needs to be updated and recompiled for the current game version.\n",
						m_name);
			}

			const std::uint64_t tmp = (hi & 8) != 0 ? (prevOffset / a_header.pointer_size()) : prevOffset;

			switch (hi & 7) {
				case 0:
					a_stream.readin(offset);
					break;
				case 1:
					offset = tmp + 1;
					break;
				case 2:
					offset = tmp + a_stream.readout<std::uint8_t>();
					break;
				case 3:
					offset = tmp - a_stream.readout<std::uint8_t>();
					break;
				case 4:
					offset = tmp + a_stream.readout<std::uint16_t>();
					break;
				case 5:
					offset = tmp - a_stream.readout<std::uint16_t>();
					break;
				case 6:
					offset = a_stream.readout<std::uint16_t>();
					break;
				case 7:
					offset = a_stream.readout<std::uint32_t>();
					break;
				default:
					REX::FAIL(
						"{}\n\n"
						"Unhandled type while loading Address Library!\n"
						"The plugin needs to be updated and recompiled for the current game version.\n",
						m_name);
			}

			if ((hi & 8) != 0) {
				offset *= a_header.pointer_size();
			}

			mapping = { id, offset };

			prevOffset = offset;
			prevID = id;
		}
	}

	void IDDB::validate_file()
	{
		// clang-format off
		std::unordered_map<Loader, std::vector<std::pair<REL::Version, std::string_view>>> g_blacklistMap{
			{ Loader::SKSE, {} },
			{ Loader::F4SE, { 
				{ REL::Version{ 1, 10, 980 }, "2AD60B95388F1B6E77A6F86F17BEB51D043CF95A341E91ECB2E911A393E45FE8156D585D2562F7B14434483D6E6652E2373B91589013507CABAE596C26A343F1"sv },
				{ REL::Version{ 1, 11, 159 }, "686D40387F638ED75AD43BB76CA14170576F1A30E91144F280987D13A3012B1CA6A4E04E6BE7A5B99E46C50332C49BE40C3D9448038E17D3D31C40E72A90AE26"sv },
				{ REL::Version{ 1, 11, 240 }, "5A18BE865BEB3BB3D6A6BE8C50D424D012E5CCF73A83A7837865BE31D4C1D587F74A98F82EEECAEA55B83F5C657D8DC6525DFAD250C9492E958ACF4716575094"sv },
			} },
			{ Loader::SFSE, {} },
			{ Loader::OBSE, {} },
		};
		// clang-format on

		const auto mod = REX::FModule::GetExecutingModule();
		const auto modVersion = mod.GetFileVersion();
		for (auto& check : g_blacklistMap[m_loader]) {
			if (modVersion == check.first) {
				auto sha = REX::SHA512({ m_mmap.data(), m_mmap.size() });
				if (!sha)
					REX::FAIL(
						"{}\n\n"
						"Failed to hash Address Library file!\n"
						"You are using a version of Address Library that is corrupted or invalid.\n"
						"Redownload the latest Address Library for your game version.\n"
						"Game Version: {}",
						m_name, modVersion.string());
				if (*sha == check.second)
					REX::FAIL(
						"{}\n\n"
						"Invalid Address Library loaded!\n"
						"You are using a version of Address Library that is corrupted or invalid.\n"
						"Redownload the latest Address Library for your game version.\n"
						"Game Version: {}",
						m_name, modVersion.string());
			}
		}
	}

	std::uint64_t IDDB::offset(std::uint64_t a_id) const
	{
		const auto mod = REX::FModule::GetExecutingModule();
		// 4Stim: the Runtime Database, then our own table.
		if (gRuntime) {
			RD::RuntimeDatabase::ResolveFailure failure{};
			if (const auto rva = FromRuntime(a_id, &failure))
				return *rva;
			if (const auto rva = FromTable(a_id))
				return *rva;
			REX::FAIL(
				"{}\n\n"
				"The Runtime Database has no address for an ID 4Stim needs ({}).\n"
				"Update Runtime Database, or this game version isn't supported yet.\n"
				"ID: {}\n"
				"Game Version: {}",
				m_name, FailureName(failure), a_id, mod.GetFileVersion().string());
		}
		if (std::to_underlying(m_format) < 5) {
			if (m_v0.empty())
				REX::FAIL("No Address Library has been loaded!"sv);

			const MAPPING elem{ a_id, 0 };
			const auto    it = std::lower_bound(
                m_v0.begin(),
                m_v0.end(),
                elem,
                [](auto&& a_lhs, auto&& a_rhs) {
                    return a_lhs.id < a_rhs.id;
                });

			if (it == m_v0.end() || it->id != a_id) {  // 4Stim: an exact match only
				REX::FAIL(
					"{}\n\n"
					"Failed to find offset for Address Library ID!\n"
					"The plugin needs to be updated and recompiled for the current game version.\n"
					"Invalid ID: {}\n"
					"Game Version: {}",
					m_name, a_id, mod.GetFileVersion().string());
			}

			return static_cast<std::size_t>(it->offset);
		}

		if (m_v5.empty())
			REX::FAIL("No Address Library has been loaded!"sv);

		const auto offset = static_cast<std::uint64_t>(m_v5[a_id]);
		if (!offset) {
			REX::FAIL(
				"{}\n\n"
				"Failed to find offset for Address Library ID!\n"
				"The plugin needs to be updated and recompiled for the current game version.\n"
				"Invalid ID: {}\n"
				"Game Version: {}",
				m_name, a_id, mod.GetFileVersion().string());
		}

		return offset;
	}
}

// 4Stim
namespace Compat
{
	bool UsingAddressTable()
	{
		(void)REL::IDDB::GetSingleton();
		return gRuntime != nullptr || !g4StimTable.empty();
	}

	bool UsingRuntimeDatabase()
	{
		(void)REL::IDDB::GetSingleton();
		return gRuntime != nullptr;
	}

	std::filesystem::path AddressTablePath()
	{
		return gRuntime ? gRuntimePath : g4StimTablePath;
	}

	std::filesystem::path RuntimeDatabasePathLooked()
	{
		return gRuntimeLooked;
	}

	std::string WhyMissing(std::uint64_t a_id)
	{
		if (!gRuntime)
			return "not in the table";
		RD::RuntimeDatabase::ResolveFailure failure{};
		(void)FromRuntime(a_id, &failure);
		return FailureName(failure);
	}

	bool HasAddress(std::uint64_t a_id)
	{
		if (!UsingAddressTable())
			return true;
		return FromRuntime(a_id).has_value() || FromTable(a_id).has_value();
	}
}
