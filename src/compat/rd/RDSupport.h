#pragma once

// 4Stim: what CommonLibF4RD's RuntimeDatabase.cpp takes from the rest of
// CommonLibF4RD (its REL::Version, REL::Module, REL::Segment, F4SE::WinAPI
// and the rsm-mmio file map), in namespace RD so it doesn't meet 4Stim's
// CommonLibF4 REL. Adapted from CommonLibF4RD's include/REL/Relocation.h and
// src/F4SE/Impl/WinAPI.cpp (MIT License, see LICENSE-CommonLibF4RD.txt).

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RD
{
	class Version
	{
	public:
		using value_type = std::uint16_t;

		constexpr Version() noexcept = default;
		explicit constexpr Version(std::array<value_type, 4> a_version) noexcept :
			_impl(a_version) {}
		constexpr Version(value_type a_v1, value_type a_v2 = 0, value_type a_v3 = 0, value_type a_v4 = 0) noexcept :
			_impl{ a_v1, a_v2, a_v3, a_v4 } {}

		[[nodiscard]] constexpr value_type&       operator[](std::size_t a_idx) noexcept { return _impl[a_idx]; }
		[[nodiscard]] constexpr const value_type& operator[](std::size_t a_idx) const noexcept { return _impl[a_idx]; }
		[[nodiscard]] constexpr decltype(auto)    begin() const noexcept { return _impl.begin(); }
		[[nodiscard]] constexpr decltype(auto)    end() const noexcept { return _impl.end(); }

		[[nodiscard]] constexpr std::strong_ordering compare(const Version& a_rhs) const noexcept
		{
			for (std::size_t i = 0; i < _impl.size(); ++i) {
				if (_impl[i] != a_rhs._impl[i]) {
					return _impl[i] < a_rhs._impl[i] ? std::strong_ordering::less : std::strong_ordering::greater;
				}
			}
			return std::strong_ordering::equal;
		}

		[[nodiscard]] std::string string() const
		{
			std::string result;
			for (const auto ver : _impl) {
				result += std::to_string(ver);
				result += '-';
			}
			result.pop_back();
			return result;
		}

	private:
		std::array<value_type, 4> _impl{ 0, 0, 0, 0 };
	};

	[[nodiscard]] constexpr bool                 operator==(const Version& a_lhs, const Version& a_rhs) noexcept { return a_lhs.compare(a_rhs) == 0; }
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const Version& a_lhs, const Version& a_rhs) noexcept { return a_lhs.compare(a_rhs); }

	enum class RuntimeFamily : std::uint8_t
	{
		kOG,
		kNG,
		kAE
	};

	[[nodiscard]] constexpr RuntimeFamily runtime_family(const Version& a_version) noexcept
	{
		if (a_version >= Version{ 1, 11, 0, 0 }) {
			return RuntimeFamily::kAE;
		}
		if (a_version >= Version{ 1, 10, 980, 0 }) {
			return RuntimeFamily::kNG;
		}
		return RuntimeFamily::kOG;
	}

	class Segment
	{
	public:
		enum Name : std::size_t
		{
			text,
			interpr,
			idata,
			rdata,
			data,
			pdata,
			tls,
			total
		};

		constexpr Segment() noexcept = default;
		constexpr Segment(std::uintptr_t a_proxyBase, std::uintptr_t a_address, std::uintptr_t a_size) noexcept :
			_proxyBase(a_proxyBase), _address(a_address), _size(a_size) {}

		[[nodiscard]] constexpr std::uintptr_t address() const noexcept { return _address; }
		[[nodiscard]] constexpr std::size_t    offset() const noexcept { return address() - _proxyBase; }
		[[nodiscard]] constexpr std::size_t    size() const noexcept { return _size; }
		[[nodiscard]] void*                    pointer() const noexcept { return reinterpret_cast<void*>(address()); }

		template <class T>
		[[nodiscard]] T* pointer() const noexcept
		{
			return static_cast<T*>(pointer());
		}

	private:
		std::uintptr_t _proxyBase{ 0 };
		std::uintptr_t _address{ 0 };
		std::size_t    _size{ 0 };
	};

	// The game's executable, as loaded.
	class Module
	{
	public:
		Module(const Module&) = delete;
		Module& operator=(const Module&) = delete;

		[[nodiscard]] static Module& get();

		[[nodiscard]] constexpr std::uintptr_t base() const noexcept { return _base; }
		[[nodiscard]] constexpr std::size_t    image_size() const noexcept { return _imageSize; }
		[[nodiscard]] constexpr std::uintptr_t preferred_base() const noexcept { return _preferredBase; }
		[[nodiscard]] constexpr Segment        segment(Segment::Name a_segment) const noexcept { return _segments[a_segment]; }
		[[nodiscard]] constexpr Version        version() const noexcept { return _version; }
		[[nodiscard]] void*                    pointer() const noexcept { return reinterpret_cast<void*>(base()); }

		template <class T>
		[[nodiscard]] T* pointer() const noexcept
		{
			return static_cast<T*>(pointer());
		}

	private:
		Module();

		std::array<Segment, Segment::total> _segments;
		Version                             _version;
		std::uintptr_t                      _base{ 0 };
		std::size_t                         _imageSize{ 0 };
		std::uintptr_t                      _preferredBase{ 0 };
	};

	namespace WinAPI
	{
		[[nodiscard]] bool          CloseHandle(void* a_handle) noexcept;
		[[nodiscard]] void*         CreateFileMapping(std::size_t a_size, const wchar_t* a_name) noexcept;
		[[nodiscard]] void*         CreateMutex(const wchar_t* a_name) noexcept;
		[[nodiscard]] std::uint32_t GetCurrentProcessID() noexcept;
		[[nodiscard]] bool          IsExecutableAddress(const void* a_address) noexcept;
		[[nodiscard]] bool          IsReadableAddress(const void* a_address) noexcept;
		[[nodiscard]] void*         MapViewOfFile(void* a_mapping, std::size_t a_size) noexcept;
		[[nodiscard]] bool          ReleaseMutex(void* a_mutex) noexcept;
		[[nodiscard]] bool          UnmapViewOfFile(const void* a_address) noexcept;
		[[nodiscard]] bool          WaitForMutex(void* a_mutex) noexcept;
	}

	// 4Stim.log.
	void LogErrorText(const std::string& a_text);
	void LogWarnText(const std::string& a_text);
	void LogInfoText(const std::string& a_text);

	template <class... Args>
	void LogError(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		LogErrorText(std::format(a_fmt, std::forward<Args>(a_args)...));
	}
	template <class... Args>
	void LogWarn(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		LogWarnText(std::format(a_fmt, std::forward<Args>(a_args)...));
	}
	template <class... Args>
	void LogInfo(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		LogInfoText(std::format(a_fmt, std::forward<Args>(a_args)...));
	}
}

namespace mmio
{
	// A read-only map of a whole file (rsm-mmio's, as the database uses it).
	class mapped_file_source
	{
	public:
		mapped_file_source() noexcept = default;
		mapped_file_source(const mapped_file_source&) = delete;
		mapped_file_source& operator=(const mapped_file_source&) = delete;
		~mapped_file_source() { close(); }

		bool open(const std::filesystem::path& a_path) noexcept;
		void close() noexcept;

		[[nodiscard]] bool               is_open() const noexcept { return _view != nullptr; }
		[[nodiscard]] const std::byte*   data() const noexcept { return static_cast<const std::byte*>(_view); }
		[[nodiscard]] std::size_t        size() const noexcept { return _size; }

	private:
		void*       _file{ nullptr };
		void*       _mapping{ nullptr };
		const void* _view{ nullptr };
		std::size_t _size{ 0 };
	};
}
