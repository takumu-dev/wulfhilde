#!/bin/sh
# Checks that the image keeps the promises CI relies on: the compilers at their floors, each
# standard library where the presets expect it, the tools the presets and vcpkg call, and lavapipe
# as the only Vulkan device, rendering with fixed CPU features. The image's build runs this last,
# so an image that breaks a promise is never pushed. It reports every broken promise, not just the
# first.
set -u

failures=0

fail() {
    echo "check-image: $*" >&2
    failures=$((failures + 1))
}

# Runs a command and expects its output to match an extended regular expression.
expect() {
    what=$1
    pattern=$2
    shift 2
    if ! output=$("$@" 2>&1); then
        fail "$what: \`$*\` failed: $output"
    elif ! printf '%s\n' "$output" | grep -Eq -- "$pattern"; then
        fail "$what: \`$*\` printed: $output"
    fi
}

expect_command() {
    for name in "$@"; do
        command -v "$name" > /dev/null || fail "$name isn't on the PATH"
    done
}

# Builds a program with the given compiler and flags, runs it, and expects every shared library it
# loads from a standard library to come from the given directory.
expect_program() {
    what=$1
    library_dir=$2
    shift 2
    program="$work/$(printf '%s' "$what" | tr -c '[:alnum:]' '_')"
    if ! output=$("$@" -std=c++23 -o "$program" "$work/program.cpp" 2>&1); then
        fail "$what: \`$*\` failed to build the program: $output"
        return
    fi
    expect "$what" '^42$' "$program"
    for library in $(ldd "$program" | awk '$1 ~ /^lib(stdc|c)\+\+/ && $2 == "=>" { print $3 }'); do
        library=$(readlink -f "$library")
        case $library in
            "$library_dir"/*) ;;
            *) fail "$what: the program loads $library, not one from $library_dir" ;;
        esac
    done
}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The floor compilers, under their plain names, since the presets name them so.
for compiler in gcc g++ cc c++; do
    expect "$compiler is GCC 16.2" '^16\.2\.' "$compiler" -dumpfullversion
done
expect "clang is Clang 23" 'clang version 23\.' clang --version
expect "clang++ is Clang 23" 'clang version 23\.' clang++ --version
expect "clang-tidy is LLVM 23's" 'LLVM version 23\.' clang-tidy --version
expect "clang-format is LLVM 23's" 'clang-format version 23\.' clang-format --version

# Each standard library, built against and loaded at run time. Clang must take GCC 16's
# libstdc++, not the distro's older one.
cat > "$work/program.cpp" << 'EOF'
#include <print>
#include <version>

#if defined(EXPECT_LIBCXX)
static_assert(_LIBCPP_VERSION >= 230000, "libc++ is LLVM 23's");
#else
static_assert(_GLIBCXX_RELEASE == 16, "libstdc++ is GCC 16's");
#endif

int main() { std::println("{}", 42); }
EOF
expect_program "GCC with libstdc++" /opt/gcc-16/lib64 g++
expect_program "Clang with libstdc++" /opt/gcc-16/lib64 clang++
expect_program "Clang with libc++" /usr/lib/x86_64-linux-gnu clang++ -stdlib=libc++ -DEXPECT_LIBCXX
expect_program "Clang with AddressSanitizer and UndefinedBehaviorSanitizer" /opt/gcc-16/lib64 \
    clang++ -fsanitize=address,undefined -fno-sanitize-recover=all

# The build tools the presets name, and what vcpkg and the CI steps call.
expect "CMake is 4.4" '^cmake version 4\.4\.' cmake --version
expect "Doxygen is 1.15" '^1\.15\.' doxygen --version
expect_command ninja check-jsonschema git curl tar zip unzip xz zstd pkg-config python3 jq

# Lavapipe is the only device, with its CPU features capped at AVX2 and its vectors 256 bits wide
# on every host.
expect "lavapipe's CPU features are capped" '^avx$' printenv GALLIUM_OVERRIDE_CPU_CAPS
if ! summary=$(vulkaninfo --summary 2>&1); then
    fail "\`vulkaninfo --summary\` failed: $summary"
else
    devices=$(printf '%s\n' "$summary" | sed -n 's/^[[:space:]]*deviceName *= *//p')
    case $devices in
        *"
"*) fail "lavapipe isn't the only Vulkan device: $devices" ;;
        "llvmpipe (LLVM "*", 256 bits)") ;;
        *) fail "the Vulkan device isn't lavapipe with 256-bit vectors: $devices" ;;
    esac
fi

if [ "$failures" -ne 0 ]; then
    echo "check-image: promises broken: $failures" >&2
    exit 1
fi
echo "check-image: every promise holds"
