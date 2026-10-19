<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="branding/wulfhilde-logo-dark.svg">
    <img src="branding/wulfhilde-logo.svg" alt="wulfhilde" width="320">
  </picture>
</h1>

wulfhilde is a library for games built from sectors joined by portals, in the manner of the Build
engine: stacked sectors, sectors that share the same space, and spaces bigger inside than out. A
game links the modules it needs and keeps `main` and its frame loop to itself.

It is at an early stage. Today it holds `core`, the package a game builds against, and
`wulfhilde-viewer`, the map-preview tool, which so far only prints its usage.

## Modules

| Module | Links | Holds |
|---|---|---|
| `core` | | checks of preconditions and their handler, the log sink, the checked `narrow`, typed IDs, and the math types: vectors, matrices, rotations, rigid transforms, angles, colours and planes |

Each module is a static library, `wulfhilde::<module>`, with its headers under
`<wulfhilde/<module>/...>`. The umbrella target `wulfhilde::wulfhilde` links them all.

## Requirements

- A C++23 compiler: GCC 16, Clang 23 (with libstdc++ or libc++) or MSVC 14.52, or newer. Configuring
  with anything older stops with a message naming the floor.
- CMake 4.4 or newer, and Ninja.
- For the engine's own builds, [vcpkg](https://vcpkg.io), with `VCPKG_ROOT` naming a clone of it.

## Building

Every supported configuration is a preset in `CMakePresets.json`, and a workflow preset
configures, builds and tests in one step:

```sh
cmake --workflow --preset gcc-debug
```

| Preset | Configuration |
|---|---|
| `gcc-debug`, `gcc-release` | GCC; the release build has the engine and the viewer without exceptions |
| `clang-debug`, `clang-release` | Clang with libstdc++ |
| `libcxx-debug`, `libcxx-release` | Clang with libc++ |
| `sanitize` | Clang with AddressSanitizer and UndefinedBehaviorSanitizer |
| `tidy` | Clang, with `clang-tidy` run on every source |
| `lint` | the formatting and documentation checks |
| `msvc-debug`, `msvc-release`, `msvc-asan`, `msvc-noexcept` | MSVC, from a developer command prompt |

Warnings are errors in every preset. Machine-specific settings, such as a compiler under another
name, belong in a `CMakeUserPresets.json`, which git ignores.

The tests carry one CTest label each: `unit` for tests that need only the CPU. To run some of them,
use for example `ctest --preset gcc-debug -L unit`.

## Using the engine in a game

The engine's build only finds its dependencies with `find_package`, and never downloads anything,
so a game can supply them through vcpkg, Conan, system packages or builds of its own.

As an installed package, for every module and the umbrella target:

```cmake
find_package(wulfhilde CONFIG REQUIRED)
target_link_libraries(game PRIVATE wulfhilde::wulfhilde)
```

or for some modules only:

```cmake
find_package(wulfhilde CONFIG REQUIRED COMPONENTS core)
target_link_libraries(game PRIVATE wulfhilde::core)
```

As a subproject, through a git submodule or `FetchContent`:

```cmake
add_subdirectory(external/wulfhilde)
target_link_libraries(game PRIVATE wulfhilde::wulfhilde)
```

As a subproject, the engine builds its libraries alone, without its tests, the viewer or install
rules. The options `WULFHILDE_BUILD_TESTS`, `WULFHILDE_BUILD_VIEWER` and `WULFHILDE_INSTALL` change
that.

The engine builds with exceptions enabled or disabled, and never throws. Failures that correct code
can meet are returned as values. A broken precondition is a bug: it is logged and then handed to
the assertion handler, which aborts by default and which a game can replace with
`wulfhilde::set_assertion_handler`. Other diagnostics go to the log sink, which writes to stderr
until a game installs its own with `wulfhilde::set_log_sink`.

## Working on the engine

- `CODING_STANDARDS.md` says how the code is written, tested and committed.
- `cmake --workflow --preset lint` checks the formatting with `clang-format` and the documentation
  with Doxygen. Building the `format` target reformats every source.
- Every public declaration has a Doxygen comment. Doxygen writes the reference to `build/docs`.
- Tests come first, through the public interfaces only, under `tests/`, which mirrors `src/` and
  `apps/`. `tests/consumers/` builds a small game against the installed package and as a
  subproject.
- Every pull request runs each Linux preset in CI, inside the image that `ci/linux/` defines: GCC
  16.2, Clang 23 with libc++, CMake 4.4 and lavapipe, with the distro's packages as they stood on
  a fixed date. The image is public, as `ghcr.io/takumu-dev/wulfhilde-ci`, tagged with the hash
  that `ci/image-tag.sh` prints, so CI's compilers and Vulkan driver are one `docker pull` away.
- It also runs each MSVC preset on GitHub's Windows Server 2025 image with Visual Studio 2026,
  which the scripts in `ci/windows/` set up with MSVC 14.52, the Vulkan loader and a pinned build
  of lavapipe.
- The jobs fan into one check, `ci`, which `develop` and `main` require.
- Configuring checks the licence of every port vcpkg installs, build tools included, against the
  allowlist in `cmake/licenses.json`, and stops on any it doesn't cover. A port whose licence vcpkg
  doesn't know takes an override there, pinned to the version reviewed, so that an update to the
  port stops the configuration until its licence is reviewed again.
- `THIRD_PARTY_NOTICES.md` lists the ports that ship with the engine: the manifest's default
  dependencies and what they depend on, without build tools or the tests' dependencies. After a
  change to them, build the `notices` target of the `gcc-release` preset to write it again; that
  preset's tests fail while it is out of date.

## Acknowledgements

The logo's wordmark and the social preview's tagline are drawn from
[Chakra Petch](https://fonts.google.com/specimen/Chakra+Petch), under the SIL Open Font License.

## Licence

wulfhilde is released under the zlib licence. See `LICENSE`. The libraries it is built with are
listed in `THIRD_PARTY_NOTICES.md`, with the notices their licences ask a game to pass on. The
package installs both.
