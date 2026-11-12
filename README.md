<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="branding/wulfhilde-logo-dark.svg">
    <img src="branding/wulfhilde-logo.svg" alt="wulfhilde" width="320">
  </picture>
</h1>

wulfhilde is a library for games built from sectors joined by portals, in the manner of the Build
engine: stacked sectors, sectors that share the same space, and spaces bigger inside than out. A
game links the modules it needs and keeps `main` and its frame loop to itself.

It is at an early stage. Today it holds three modules, `core`, `world` and `assets`, which a game
builds against as a package or a subproject, and `wulfhilde-viewer`, the map-preview tool, which so
far loads a map and its assets and reports every problem in them.

## Modules

| Module | Links | Holds |
|---|---|---|
| `core` | | checks of preconditions and their handler, the log sink, the checked `narrow`, typed IDs, and the math types: vectors, matrices, rotations, rigid transforms, angles, colours and planes |
| `world` | `core` | the map: made from a plain description that mirrors the map file, checked against the map's rules with every problem reported at its JSON pointer, and asked whether a location is in its sector, the heights there and where a trace stops. It opens no files |
| `assets` | `world` | the files under a content root, read into what the other modules use: so far, a map's file and its materials, with their PNG textures decoded into mip chains, and every problem reported at its file, line, column and JSON pointer. It is the only module that opens files |
| `platform` | `core` | the system's windows, keyboard and mouse, through SDL3, which no public header names: a window that can be resized or cover the display, with relative mouse mode, and events of the window, the keys by their USB HID codes, and the mouse |
| `renderer` | `world`, `assets`, `platform` | drawing with the GPU through Vulkan 1.3 or newer, which no public header names: a map's sectors, drawn in linear light with multisampling and tone-mapped with AgX, into a window frame after frame or into an image in memory |

Each module is a static library, `wulfhilde::<module>`, with its headers under
`<wulfhilde/<module>/...>`. The umbrella target `wulfhilde::wulfhilde` links them all.

## Writing maps

A map is a strict JSON file ending `.map.json`, written by hand, in a content root: a directory
that holds the map and the assets it uses, which asset paths name relative to it, without their
extensions. `schema/map.schema.json` is the map's JSON Schema, installed under
`share/wulfhilde/schema/`, so an editor can check and complete a map as it is written. Point the
map's `$schema` at it, as the maps in `tests/content/` do, or tell the editor to use it for every
`*.map.json`.

Each surface names a material, a strict JSON file ending `.material.json` in the same content
root, with glTF's metallic-roughness properties, each a factor times an optional texture, an
emission in nits, and the real-world size of one repeat of its textures. A texture is a PNG,
named by its asset path without `.png`, whose colour space is the one the property reading it
needs. `schema/material.schema.json` is the material's JSON Schema, installed beside the map's.

The viewer opens a map's file, with the content root defaulting to the file's own directory:

```sh
wulfhilde-viewer tests/content/sectors.map.json
wulfhilde-viewer content/maps/e1m1.map.json --content content
```

A map with mistakes in it has every problem of the first phase that finds any reported, each as
`file:line:column: message (pointer)`, a form that terminals and editors link to. The phases are
the file's syntax, then its structure, then the map's own rules, so no problem is a knock-on of
another.

## Requirements

- A C++23 compiler: GCC 16, Clang 23 (with libstdc++ or libc++) or MSVC 14.52, or newer. Configuring
  with anything older stops with a message naming the floor.
- CMake 4.4 or newer, and Ninja.
- For the engine's own builds, [vcpkg](https://vcpkg.io), with `VCPKG_ROOT` naming a clone of it.
  On Linux, vcpkg builds SDL's X11 and Wayland drivers against the system's headers, from
  `libx11-dev`, `libxext-dev`, `libwayland-dev`, `libxkbcommon-dev` and `libegl-dev` on Debian and
  Ubuntu.

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
| `lint` | the formatting, documentation and asset schema checks |
| `msvc-debug`, `msvc-release`, `msvc-asan`, `msvc-noexcept` | MSVC, from a developer command prompt |

Warnings are errors in every preset. Machine-specific settings, such as a compiler under another
name, belong in a `CMakeUserPresets.json`, which git ignores.

The tests carry one CTest label each: `unit` for tests that need only the CPU, `gpu` for tests that
run on a Vulkan device under its validation layers, and `image` for tests that compare what the
renderer draws with a reference image. To run some of them, use for example
`ctest --preset gcc-debug -L unit`. Only `gcc-release` runs the `image` tests, since their
references are rendered by lavapipe in CI's Linux image, which draws alike on every machine.

## Using the engine in a game

The engine's build only finds its dependencies with `find_package`, and never downloads anything,
so a game can supply them through vcpkg, Conan, system packages or builds of its own:

| Package | Found with | Needed by |
|---|---|---|
| [earcut.hpp](https://github.com/mapbox/earcut.hpp) | `find_package(earcut_hpp CONFIG)` | building `renderer` |
| [lodepng](https://github.com/lvandeve/lodepng) | `find_package(lodepng CONFIG)` | `assets` |
| [pedantjson](https://github.com/takumu-dev/pedantjson) | `find_package(pedantjson CONFIG)` | `assets` |
| [SDL3](https://www.libsdl.org) | `find_package(SDL3 CONFIG)` | `platform` |
| [volk](https://github.com/zeux/volk) | `find_package(volk CONFIG)` | `renderer` |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | `find_package(VulkanHeaders CONFIG)` | building `platform` |
| [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | `find_package(VulkanMemoryAllocator CONFIG)` | building `renderer` |
| [Slang](https://github.com/shader-slang/slang) | `find_package(slang CONFIG)` | building `renderer` |

The renderer's shaders are compiled into it when it builds, and earcut.hpp and
VulkanMemoryAllocator are headers compiled into it too, so a game that uses the installed package
needs none of the three, and nothing links the Vulkan loader, which volk loads when a renderer is
created. A game that adds the engine as a subproject needs them all, such as vcpkg's
`shader-slang` port or the copy of Slang in the Vulkan SDK.

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
- `cmake --workflow --preset lint` checks the formatting with `clang-format`, the documentation
  with Doxygen, and the map and material schemas with `check-jsonschema` against the tests' files.
  Building the `format` target reformats every source.
- Every public declaration has a Doxygen comment. Doxygen writes the reference to `build/docs`.
- Tests come first, through the public interfaces only, under `tests/`, which mirrors `src/` and
  `apps/`. `tests/consumers/` builds a small game against the installed package and as a
  subproject.
- Every pull request runs each Linux preset in CI, inside the image that `ci/linux/` defines: GCC
  16.2, Clang 23 with libc++, CMake 4.4 and lavapipe, with the distro's packages as they stood on
  a fixed date. The image is public, as `ghcr.io/takumu-dev/wulfhilde-ci`, tagged with the hash
  that `ci/image-tag.sh` prints, so CI's compilers and Vulkan driver are one `docker pull` away.
- It also runs each MSVC preset on CircleCI's Windows Server 2025 image with Visual Studio 2026,
  which the scripts in `ci/windows/` set up with MSVC 14.52, the Vulkan loader and a pinned build
  of lavapipe.
- The jobs fan into one check, `ci/circleci: ci`, which `develop` and `main` require.
- Configuring checks the licence of every port vcpkg installs, build tools included, against the
  allowlist in `cmake/licenses.json`, and stops on any it doesn't cover. A port whose licence vcpkg
  doesn't know takes an override there, pinned to the version reviewed, so that an update to the
  port stops the configuration until its licence is reviewed again.
- pedantjson, the strict JSON reader that reads maps, comes from the overlay port in
  `ports/pedantjson/`, a copy of the port in [pedantjson's repository](https://github.com/takumu-dev/pedantjson),
  until vcpkg's registry has it. lodepng, which decodes materials' textures and reads and writes
  the image tests' PNG files, comes from the overlay port in `ports/lodepng/`, at a newer commit
  than the registry's port, until the registry takes it up.
- The `gpu` and `image` tests run under the validation layers that the manifest's `tests` feature
  builds, which the tests point the Vulkan loader at, and any message from them fails the test.
- An image test that fails leaves what it drew, and the pixels where that differs from its
  reference, in `build/<preset>/image-failures/`, which CI keeps. To change a reference, run the
  image tests in CI's Linux image with `WULFHILDE_UPDATE_REFERENCES=1`, which writes what they draw
  over the references in `tests/`; then look at the new images and commit them with the change.
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
