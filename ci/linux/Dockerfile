# The image every Linux job of CI runs in, published as ghcr.io/takumu-dev/wulfhilde-ci with a tag
# named by the hash of this directory (see ci/image-tag.sh). A tag is built once and never rebuilt,
# so any change here, the snapshot date included, makes a new image. Lavapipe's output can change
# with it, so the change that makes a new image also regenerates the reference images.
#
# Distro packages come from the Ubuntu archive as it stood at the snapshot date, which pins
# lavapipe to the distro's Mesa. GCC is built from source, since Ubuntu 26.04 carries only a
# GCC 16 snapshot from before 16.1, and Clang comes from apt.llvm.org.

ARG UBUNTU=ubuntu:26.04@sha256:c098726dd9b9733f3e885f8de0ef375206a311b0a317eadc40d347b1115fb944

FROM ${UBUNTU} AS base

ARG DEBIAN_FRONTEND=noninteractive
ARG SNAPSHOT=20261016T000000Z

# apt checks every file it fetches against the archive's signed InRelease, so TLS adds nothing to
# what gets installed. The snapshot server is HTTPS only, so until ca-certificates is installed,
# its certificate goes unchecked; after that, it's checked.
RUN echo "APT::Snapshot \"${SNAPSHOT}\";" > /etc/apt/apt.conf.d/50snapshot \
    && apt-get -o Acquire::https::Verify-Peer=false update \
    && apt-get -o Acquire::https::Verify-Peer=false install --yes --no-install-recommends \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*


FROM base AS gcc

ARG DEBIAN_FRONTEND=noninteractive
ARG GCC_VERSION=16.2.0
ARG GCC_SHA256=e6738e29597f733270731aa90600f37ffdc045079dfc27ec7e8192cc81085c3e

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        curl file g++ libgmp-dev libmpc-dev libmpfr-dev libzstd-dev make xz-utils zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Built once, by the distro's GCC: bootstrapping it with itself would triple the build for the
# same compiler.
WORKDIR /usr/src/gcc
RUN curl --fail --silent --show-error --location --retry 3 --output gcc.tar.xz \
        "https://mirrors.kernel.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz" \
    && echo "${GCC_SHA256}  gcc.tar.xz" | sha256sum --check --strict \
    && mkdir source build \
    && tar --extract --file gcc.tar.xz --directory source --strip-components 1 \
    && cd build \
    && ../source/configure \
        --prefix=/opt/gcc-16 \
        --enable-languages=c,c++ \
        --enable-checking=release \
        --disable-bootstrap \
        --disable-multilib \
        --disable-nls \
        --with-gcc-major-version-only \
        --with-system-zlib \
    && make --jobs "$(nproc)" \
    && make install-strip \
    && ln -s gcc /opt/gcc-16/bin/cc \
    && cd / \
    && rm -rf /usr/src/gcc


FROM base

LABEL org.opencontainers.image.source=https://github.com/takumu-dev/wulfhilde
LABEL org.opencontainers.image.description="wulfhilde's Linux CI: GCC 16, Clang 23, CMake 4.4 and lavapipe"

ARG DEBIAN_FRONTEND=noninteractive
ARG CMAKE_VERSION=4.4.4
ARG CMAKE_SHA256=e5bb807f7728cb60cd8b27ebc97a2edb469b68655f21e844a600c3575b76f5bb

COPY apt.llvm.org.asc /etc/apt/keyrings/
COPY apt.llvm.org.sources /etc/apt/sources.list.d/

#   GCC 16 at run time: binutils, libc6-dev, libgmp10, libmpc3, libmpfr6, libzstd1
#   Clang 23 and its tools: clang-23 to libclang-rt-23-dev
#   the presets' tools: doxygen, ninja-build, python3-check-jsonschema
#   vcpkg and the CI steps: curl, git, jq, pkgconf, python3, tar, unzip, xz-utils, zip, zstd
#   the Vulkan loader, lavapipe and vulkaninfo: libvulkan1, mesa-vulkan-drivers, vulkan-tools
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        binutils libc6-dev libgmp10 libmpc3 libmpfr6 libzstd1 \
        clang-23 clang-format-23 clang-tidy-23 libc++-23-dev libc++abi-23-dev libclang-rt-23-dev \
        doxygen ninja-build python3-check-jsonschema \
        curl git jq pkgconf python3 tar unzip xz-utils zip zstd \
        libvulkan1 mesa-vulkan-drivers vulkan-tools \
    && rm -rf /var/lib/apt/lists/*

RUN curl --fail --silent --show-error --location --retry 3 --output /tmp/cmake.tar.gz \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
    && echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum --check --strict \
    && mkdir /opt/cmake \
    && tar --extract --gzip --file /tmp/cmake.tar.gz --directory /opt/cmake --strip-components 1 \
    && rm /tmp/cmake.tar.gz

COPY --from=gcc /opt/gcc-16 /opt/gcc-16
COPY clang.cfg /usr/lib/llvm-23/bin/clang.cfg
COPY clang.cfg /usr/lib/llvm-23/bin/clang++.cfg

# Programs built against GCC 16's libstdc++ load it at run time, ahead of the distro's.
RUN echo /opt/gcc-16/lib64 > /etc/ld.so.conf.d/00-gcc-16.conf && ldconfig

# Plain compiler and tool names, as the presets use them: GCC 16 first, then CMake, then LLVM 23.
ENV PATH=/opt/gcc-16/bin:/opt/cmake/bin:/usr/lib/llvm-23/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV LANG=C.UTF-8

# Lavapipe compiles shaders for the CPU it runs on, and GitHub's runners mix Intel and AMD
# processors, some with AVX-512. Taking AVX-512 away leaves the AVX2 and FMA that every runner has,
# and with the vector width fixed too, every runner renders with the same generated code. Lavapipe
# is the only driver the loader sees.
ENV GALLIUM_OVERRIDE_CPU_CAPS=avx
ENV LP_NATIVE_VECTOR_WIDTH=256
ENV VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json

COPY check-image.sh /usr/local/bin/check-image
RUN check-image
