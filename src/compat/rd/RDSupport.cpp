// 4Stim: RDSupport.h's Windows side (from CommonLibF4RD's
// src/F4SE/Impl/WinAPI.cpp and REL::Module, MIT License).
#include "RDSupport.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// windows.h's macros would rename RDSupport.h's functions (CreateMutex ->
// CreateMutexW): as CommonLibF4RD's WinAPI.cpp, use the W calls by name.
#undef CreateFileMapping
#undef CreateMutex

#include <sstream>

namespace RD
{
	Module& Module::get()
	{
		static Module singleton;
		return singleton;
	}

	Module::Module()
	{
		const auto handle = ::GetModuleHandleW(nullptr);  // the game's executable
		_base = reinterpret_cast<std::uintptr_t>(handle);

		// Its file version ("1.10.163.0"...).
		wchar_t path[MAX_PATH]{};
		::GetModuleFileNameW(handle, path, MAX_PATH);
		DWORD dummy = 0;
		std::vector<char> buf(::GetFileVersionInfoSizeW(path, &dummy));
		if (!buf.empty() && ::GetFileVersionInfoW(path, 0, static_cast<DWORD>(buf.size()), buf.data())) {
			VS_FIXEDFILEINFO* info = nullptr;
			UINT len = 0;
			if (::VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void**>(&info), &len) && info) {
				_version = Version{ static_cast<std::uint16_t>(HIWORD(info->dwFileVersionMS)), static_cast<std::uint16_t>(LOWORD(info->dwFileVersionMS)),
					static_cast<std::uint16_t>(HIWORD(info->dwFileVersionLS)), static_cast<std::uint16_t>(LOWORD(info->dwFileVersionLS)) };
			}
		}

		// Its sections.
		static constexpr std::array<std::string_view, Segment::total> SEGMENTS{ ".text", ".interpr", ".idata", ".rdata", ".data", ".pdata", ".tls" };
		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(_base);
		const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(_base + dos->e_lfanew);
		_imageSize = nt->OptionalHeader.SizeOfImage;
		_preferredBase = static_cast<std::uintptr_t>(nt->OptionalHeader.ImageBase);
		const auto* sections = IMAGE_FIRST_SECTION(nt);
		for (std::size_t i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
			const auto& section = sections[i];
			const auto  end = std::find(std::begin(section.Name), std::end(section.Name), std::uint8_t{});
			const auto  length = static_cast<std::size_t>(std::distance(std::begin(section.Name), end));
			for (std::size_t s = 0; s < SEGMENTS.size(); ++s) {
				if (SEGMENTS[s].size() == length && std::memcmp(SEGMENTS[s].data(), section.Name, length) == 0) {
					_segments[s] = Segment{ _base, _base + section.VirtualAddress, section.Misc.VirtualSize };
				}
			}
		}
	}

	namespace WinAPI
	{
		bool CloseHandle(void* a_handle) noexcept { return ::CloseHandle(static_cast<HANDLE>(a_handle)) != 0; }

		void* CreateFileMapping(std::size_t a_size, const wchar_t* a_name) noexcept
		{
			const auto size = static_cast<std::uint64_t>(a_size);
			return ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), a_name);
		}

		void* CreateMutex(const wchar_t* a_name) noexcept { return ::CreateMutexW(nullptr, FALSE, a_name); }

		std::uint32_t GetCurrentProcessID() noexcept { return static_cast<std::uint32_t>(::GetCurrentProcessId()); }

		namespace
		{
			bool Committed(const void* a_address, MEMORY_BASIC_INFORMATION& a_info) noexcept
			{
				return a_address && ::VirtualQuery(a_address, &a_info, sizeof(a_info)) == sizeof(a_info) && a_info.State == MEM_COMMIT &&
				       (a_info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
			}
		}

		bool IsExecutableAddress(const void* a_address) noexcept
		{
			MEMORY_BASIC_INFORMATION info{};
			if (!Committed(a_address, info)) {
				return false;
			}
			switch (info.Protect & 0xFFu) {
			case PAGE_EXECUTE:
			case PAGE_EXECUTE_READ:
			case PAGE_EXECUTE_READWRITE:
			case PAGE_EXECUTE_WRITECOPY:
				return true;
			default:
				return false;
			}
		}

		bool IsReadableAddress(const void* a_address) noexcept
		{
			MEMORY_BASIC_INFORMATION info{};
			if (!Committed(a_address, info)) {
				return false;
			}
			switch (info.Protect & 0xFFu) {
			case PAGE_READONLY:
			case PAGE_READWRITE:
			case PAGE_WRITECOPY:
			case PAGE_EXECUTE_READ:
			case PAGE_EXECUTE_READWRITE:
			case PAGE_EXECUTE_WRITECOPY:
				return true;
			default:
				return false;
			}
		}

		void* MapViewOfFile(void* a_mapping, std::size_t a_size) noexcept
		{
			return ::MapViewOfFile(static_cast<HANDLE>(a_mapping), FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(a_size));
		}

		bool ReleaseMutex(void* a_mutex) noexcept { return ::ReleaseMutex(static_cast<HANDLE>(a_mutex)) != 0; }

		bool UnmapViewOfFile(const void* a_address) noexcept { return ::UnmapViewOfFile(a_address) != 0; }

		bool WaitForMutex(void* a_mutex) noexcept
		{
			const auto result = ::WaitForSingleObject(static_cast<HANDLE>(a_mutex), INFINITE);
			return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
		}
	}
}

namespace mmio
{
	bool mapped_file_source::open(const std::filesystem::path& a_path) noexcept
	{
		close();
		_file = ::CreateFileW(a_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (_file == INVALID_HANDLE_VALUE) {
			_file = nullptr;
			return false;
		}
		LARGE_INTEGER size{};
		if (!::GetFileSizeEx(_file, &size) || size.QuadPart <= 0) {
			close();
			return false;
		}
		_size = static_cast<std::size_t>(size.QuadPart);
		_mapping = ::CreateFileMappingW(_file, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (!_mapping) {
			close();
			return false;
		}
		_view = ::MapViewOfFile(_mapping, FILE_MAP_READ, 0, 0, 0);
		if (!_view) {
			close();
			return false;
		}
		return true;
	}

	void mapped_file_source::close() noexcept
	{
		if (_view) {
			::UnmapViewOfFile(_view);
			_view = nullptr;
		}
		if (_mapping) {
			::CloseHandle(_mapping);
			_mapping = nullptr;
		}
		if (_file) {
			::CloseHandle(_file);
			_file = nullptr;
		}
		_size = 0;
	}
}
