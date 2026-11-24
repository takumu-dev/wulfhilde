#include "benchmark.hpp"

#include "benchmark_record.hpp"
#include "benchmark_walks.hpp"
#include "cameras.hpp"
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/renderer.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/utsname.h>
#endif

#include <expected>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace viewer {

std::expected<std::vector<BenchmarkFrame>, wulfhilde::DeviceError>
play_benchmark(wulfhilde::Renderer& renderer, std::span<const WalkedFrame> frames,
               wulfhilde::Extent extent) {
    std::vector<BenchmarkFrame> measured;
    measured.reserve(frames.size());
    for (const auto& frame : frames) {
        auto measurement = renderer.measure_frame(camera_at(frame.eye), extent);
        if (!measurement) {
            return std::unexpected(std::move(measurement).error());
        }
        measured.push_back({
            .cpu = frame.simulation + measurement->submission,
            .measurement = std::move(*measurement),
        });
    }
    return measured;
}

std::string operating_system() {
#ifdef _WIN32
    // GetVersionEx answers as the program's manifest allows, so the kernel is asked instead.
    using GetVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
    auto* const ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return "Windows";
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): GetProcAddress's own type.
    const auto get_version = reinterpret_cast<GetVersion>(
        reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
    OSVERSIONINFOW version{.dwOSVersionInfoSize = static_cast<DWORD>(sizeof(OSVERSIONINFOW))};
    if (get_version == nullptr || get_version(&version) != 0) {
        return "Windows";
    }
    return std::format("Windows {}.{}.{}", version.dwMajorVersion, version.dwMinorVersion,
                       version.dwBuildNumber);
#else
    utsname system{};
    if (uname(&system) != 0) {
        return "an unknown system";
    }
    return std::format("{} {} {}", static_cast<const char*>(system.sysname),
                       static_cast<const char*>(system.release),
                       static_cast<const char*>(system.machine));
#endif
}

} // namespace viewer
