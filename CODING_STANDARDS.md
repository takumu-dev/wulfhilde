# Coding standards

How code in wulfhilde is written. Where a tool can hold a rule, it does: `.clang-format`,
`.clang-tidy`, the warning flags in `cmake/WulfhildeConventions.cmake` and the `Doxyfile` are the
record of those rules. This document covers the rest, and says why. Anything it leaves open follows
the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines); where it
departs from them, it says so.

## Language and build

- C++23, shipped as ordinary headers rather than C++20 modules, with no compiler extensions. Each
  module keeps a boundary that could become one named C++ module later.
- The supported compilers are GCC, and Clang with libstdc++, on Linux; Clang with libc++ on Linux;
  and MSVC on Windows. Their floors are GCC 16, Clang 23 and MSVC 14.52. The floors rise at each
  milestone and never fall, and configuring with anything older stops with a message naming the
  floor.
- CMake 4.4 or newer. Every supported configuration is a preset in `CMakePresets.json`, with a
  workflow preset, so that a developer and CI build alike.
- The build only finds its dependencies with `find_package`, and never downloads anything, so a game
  can supply them through any package manager or none. The engine's own builds take them from the
  vcpkg manifest, with test-only ports in its `tests` feature.
- A dependency must be under one of these licences: Zlib, MIT, BSD-2-Clause, BSD-3-Clause, BSL-1.0,
  ISC, Apache-2.0 with or without the LLVM exception, CC0-1.0 or Unlicense. A game can then ship
  under any terms.

## Modules

- The engine is a library, not a framework: a game owns `main` and its frame loop, and calls into
  the engine. The viewer is a client like any game, and uses only the installed public headers.
- Each module is a static library, `wulfhilde::<module>`, in `src/<module>/`, declared with
  `wulfhilde_add_module`. Its public headers sit under `src/<module>/include/wulfhilde/<module>/`,
  and its private headers beside its sources. A module includes only the headers of the modules it
  links. The umbrella target `wulfhilde::wulfhilde` links them all.
- A module links only what this table allows. Modules still to be written are listed, so that what
  each may link is settled before it arrives.

  | Module | Links |
  |---|---|
  | `core` | nothing |
  | `world` | `core` |
  | `assets` | `world` |
  | `platform` | `core` |
  | `renderer` | `world`, `assets`, `platform` |
  | `collision` | `world` |

- The renderer and collision don't know each other, and platform doesn't know the world. Only
  `assets` opens files. No Vulkan or SDL type appears in a public header.
- Each module is used from the thread that created the window, and any parallelism stays inside it.
  Only the log sink and the assertion handler may be called from any thread.

## Naming and spelling

- `PascalCase` for types, concepts, type aliases and template parameters.
- `snake_case` for functions, variables, parameters, namespaces, enumerators and `constexpr`
  constants, with no `k` prefix.
- Private and protected data members end in an underscore (`walls_`). A plain struct's fields are
  bare.
- Enumerations are `enum class`.
- Macros only where nothing else will do, named `WULFHILDE_UPPER_CASE`.
- Everything is in one namespace, `wulfhilde`, with internals in `wulfhilde::detail`. The viewer is a
  client, not part of the engine, so it has a namespace of its own, `viewer`.
- Prose is British: comments, documentation, the README and commit messages. Every identifier is
  American, file-format fields included, so a comment "normalises the colour" sits above
  `normalize_color`. One rule for every identifier means no word needs a judgement call.
- Names from the engine's domain use its terms, and never the words they replace:

  | Say | Not |
  |---|---|
  | map | level, scene |
  | sector | room, area, zone |
  | loop | ring, contour, polygon |
  | wall | line, linedef, edge |
  | portal | red wall, two-sided wall, room over room |
  | portal transform | teleporter, warp |
  | opening | hole, gap |
  | solid wall | one-sided wall, white wall |
  | surface | face, plane |
  | material | texture, shader, skin |
  | upper strip, lower strip | upper or lower texture, top, bottom |
  | location | position, for a point standing for a location |
  | trace | ray cast, hitscan |
  | start position | spawn point, player start |
  | view | pass, layer, sub-view |
  | light | lamp |
  | directional light | sun, sky light |
  | virtual light | ghost light, mirrored light |
  | light view | light pass, shadow view |
  | ambient light | shade, sector brightness |
  | sky | parallax, skybox |
  | backdrop | skybox, panorama, environment map |
  | body | actor, entity, collider |
  | gravity | gravity vector, up vector, custom gravity |
  | kept gravity | carried gravity, own gravity, wall-walk |
  | upright | level, vertical |
  | straddle | overlap, which is for sectors sharing space |
  | righting | re-orientation, levelling |
  | ground | footing, floor for what a body stands on |
  | eye | camera position, head |
  | controller | character controller, pawn |
  | content root | data directory, asset folder, game directory |
  | asset | resource |
  | asset path | filename, file path, URI |
  | texture | map for an image (a normal texture, not a normal map), skin |
  | problem | error for one fault in a file, diagnostic, issue |

## Files and formatting

- Files are `snake_case.hpp` and `snake_case.cpp`, with one main type per header, named after it.
  Headers use `#pragma once`.
- Public headers are included as `<wulfhilde/<module>/...>`, and a target's private headers as
  `"..."`.
- `clang-format` decides the layout, and groups includes as: the file's own header, this project's
  headers, third-party headers, then the standard library. Build the `format` target to apply it.

## Errors and assertions

- A failure that correct code can meet, such as a bad file, a missing asset or a device without a
  feature, is returned as a value: the function returns `[[nodiscard]] std::expected<T, E>`. This
  departs from the Core Guidelines' E.2.
- Constructors never fail. Anything that can fail to come into being is made by a factory function.
- Each failure domain has an error type of its own, which carries an `enum class` code for programs
  to branch on, a message built where the failure happens, and a `std::formatter`. There is no
  engine-wide error type, so that a caller can tell which codes a function returns.
- A load of a file reports every problem it finds, in phases, so that a hand-written file doesn't
  take one load per mistake.
- A broken precondition is a bug, not an error. `WULFHILDE_ASSERT(condition, message)` checks the
  cheap preconditions of the public interface in every build, release included.
  `WULFHILDE_DEBUG_ASSERT(condition, message)` checks internal invariants and expensive conditions
  where `NDEBUG` isn't defined. The message says what must hold, such as "the value fits the type it
  is narrowed to".
- A failed check is logged as an error, then handed to the assertion handler, which aborts unless a
  game has replaced it. Every other diagnostic the engine doesn't return goes to the log sink. No
  logging library appears in a public interface.
- No `throw`, `try` or `catch`, in the engine or in its tests. The engine builds with exceptions
  disabled, and stays exception-neutral: RAII throughout, so that an exception from the standard
  library or from a game's callback unwinds cleanly. A standard-library failure such as
  `std::bad_alloc` isn't caught.
- `noexcept` goes on moves, `swap`, and functions that have no precondition and can't fail. It never
  goes on a function with a precondition (the Lakos rule), so that a throwing handler stays
  possible.

## Ownership and types

- Values first, and `std::unique_ptr` for a single owner. No `std::shared_ptr` without a comment
  saying why, and no bare `new` or `delete`.
- `T*` and `T&` never own. `T*` appears only where null means something, such as "restore the
  default".
- Vulkan and SDL handles are owned by move-only RAII wrappers.
- Modules refer to the world's objects by typed IDs, `Id<Tag>`, each wrapping a `std::uint32_t`.
  "No object" is an empty `std::optional`, never a sentinel value.
- Counts and container indices are `std::size_t`, departing from the Core Guidelines' ES.107. Signed
  types are for arithmetic that can go negative. Every narrowing conversion of an integer goes
  through the checked `wulfhilde::narrow`. There is no dependency on the GSL.

## Warnings and clang-tidy

- The compiler warnings in `cmake/WulfhildeConventions.cmake` are errors in every preset, through
  `CMAKE_COMPILE_WARNING_AS_ERROR`. Give an engine or viewer target its settings with
  `wulfhilde_target_conventions`, and a test target with `wulfhilde_target_warnings`. The settings
  are private to each target, so none of them reaches the installed package's interface or a
  game's build. Third-party headers come in as system includes.
- clang-tidy runs the groups in `.clang-tidy`, with every warning an error. A finding that has to
  stand is suppressed with `NOLINT(<check>): <reason>` or `NOLINTNEXTLINE(<check>): <reason>`,
  never a bare `NOLINT`. Keep the reason on that line: `NOLINTNEXTLINE` covers only the line after
  its own, and any longer explanation goes in a comment above it.

## Documentation

- Every declaration in a public header has a `///` Doxygen comment, those in `detail` included, and
  each public header begins with a `\file` comment. Clang's `-Wdocumentation` checks each comment
  against its declaration, and Doxygen fails on any warning, an undocumented declaration among them.
- A comment says why, not what.

## Tests

- Tests come first: a test that fails, then the code that passes it. A new test that passes at once
  is checked by breaking the code it covers, since a test that can't fail proves nothing.
- A test exercises a public interface only, and survives any refactoring that keeps the behaviour.
  Its name is a sentence saying what holds, such as "a null log sink restores the default".
- `tests/` mirrors `src/` and `apps/`. Beside them, `tests/cmake/` tests the configuration,
  `tests/consumers/` the package, and `tests/support/` holds what the tests share.
- Only the tests of `assets` read files. Every other module's tests build their input in C++.
- Every test carries exactly one CTest label: `unit` (the CPU only), `gpu` (a Vulkan device, where
  any validation message fails the test) or `image` (rendered and compared with a reference image).
  Its name begins with its module, matching its Catch2 tag (`[core]`), or with `build` or
  `consumers`.
- Each kind of test has a helper in `tests/CMakeLists.txt`:
  - `wulfhilde_add_tests` adds a module's Catch2 test cases, each registered with CTest. A test
    case's name must not begin with `-`, which Catch2 would read as one of its options.
  - `wulfhilde_add_program_test` adds a program whose ending or output is the behaviour, which
    `tests/support/check_program.cmake` checks: an abort, an exit status, or what reaches stdout
    and stderr.
  - `wulfhilde_add_precondition_test` adds a program that breaks one precondition, which CTest
    expects to fail. It fails through a replaced handler rather than aborting, since CTest counts
    an abort as a crash even then. These are used sparingly.

## Commits and branches

- Commit subjects follow Conventional Commits, `type(scope)!: description`, with the type one of
  `build`, `chore`, `ci`, `docs`, `feat`, `fix`, `perf`, `refactor`, `revert`, `style` or `test`.
  The scope and the `!` of a breaking change are optional. A message describes the change and why.
- Branches follow git flow. `main` holds released versions only, and each merge into it is tagged
  with its version. `develop` is where work comes together. A change is made on `feature/<slug>`
  from `develop`, its slug naming the change; a release on `release/<version>`; and a fix to a
  release on `hotfix/<version>` from `main`. Every merge is `--no-ff`.
- Every branch gets a pull request, which must pass the `ci` check before the branch is finished
  with `git flow`.
