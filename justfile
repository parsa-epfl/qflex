# QFlex build recipe: the single source of truth for the bare, Nix and Docker
# builds. Each environment only provides the toolchain/dependencies; the
# commands below are shared by all of them:
#
#   * bare    : system packages + rustup                 -> `just all`
#   * nix     : `nix develop -i --keep HOME`, a pure shell  -> `just all`
#               (Flexus uses CMake there)
#   * docker  : the Dockerfile installs the deps and
#               calls the same recipes                   -> `just ...`
#
# Run `just --list` to see every recipe.

set shell := ["bash", "-euo", "pipefail", "-c"]

# Build type: release (default), debug or relwithdebinfo.
profile := env("PROFILE", "release")

# QEMU parallel build width (empty lets the build tool decide).
jobs := env("JOBS", "")

# Absolute path to bxdb. It *must* be absolute: QEMU's configure re-execs
# itself from the auto-created build/ directory, so a relative path would be
# resolved against build/ instead of the source root.
bxdb_root := justfile_directory() + "/bxdb"

# Configure flags shared by timing-qemu and fw-qemu. --disable-werror is
# needed because newer dependencies (e.g. Nix's libslirp 4.9) emit deprecation
# warnings that QEMU turns into errors by default on git builds.
qemu_common := "--target-list=aarch64-softmmu --disable-gtk --enable-capstone --enable-zstd --disable-werror --with-bxdb=" + bxdb_root

# Extra flags only timing-qemu needs (see the fw-qemu flake for its own set).
timing_flags := "--disable-docs --enable-slirp --enable-libqflex --enable-snapvm-external"

# Conan is used everywhere except inside a Nix shell (which supplies the
# dependencies directly). Override with CONAN=0/1.
use_conan := env("CONAN", if env("IN_NIX_SHELL", "") == "" { "1" } else { "0" })

# Extra Conan profile appended to the build (e.g. the custom-gcc profile).
extra_profile := env("EXTRA_PROFILE", "")

# Conan -pr flags for the selected profile (+ optional extra profile).
profile_flags := "-pr flexus/target/_profile/" + profile + (if extra_profile != "" { " -pr " + extra_profile } else { "" })

# ---------------------------------------------------------------------------

# qflex builds WormCache for every experiment; it reads this file to use the same environment (nix or native).
# Record the build environment in out/build-env.
_build-env:
    mkdir -p out && echo {{ if env("IN_NIX_SHELL", "") == "" { "native" } else { "nix" } }} > out/build-env

# The `bxdb` CLI (bxdb-util) indexes and purges checkpoint databases between iterations.
# Build bxdb, the Rust snapshot database linked by both QEMUs, and the `bxdb` CLI.
bxdb: _build-env
    cargo build --manifest-path bxdb/Cargo.toml -p bxdb-core -p bxdb-util --release

# With `mem`, the simulator is built for that many memory controllers and copied to out/kraken/mem<mem>/.
# Build a Flexus simulator (knottykraken by default).
flexus sim="knottykraken" mem="":
    #!/usr/bin/env bash
    set -euo pipefail
    wiring="flexus/target/{{sim}}/wiring.cpp"
    if [ -n "{{mem}}" ]; then
        # The memory controller count is a compile-time constant in the wiring; patch it for this
        # build only and restore the file afterwards.
        cp "$wiring" "$wiring.orig"
        trap 'mv "$wiring.orig" "$wiring"' EXIT
        pattern='(FLEXUS_INSTANTIATE_COMPONENT_ARRAY\( MemoryLoopback, theMemoryCfg, theMemory, FIXED, DIVIDE, )[0-9]+ \);'
        [ "$(grep -cE "$pattern" "$wiring")" = "1" ] || { echo "Cannot find the MemoryLoopback array in $wiring" >&2; exit 1; }
        sed -i -E "s/$pattern/\1{{mem}} );/" "$wiring"
    fi
    if [ "{{use_conan}}" = "1" ]; then
        conan profile detect --force
        conan build flexus {{profile_flags}} --name={{sim}} -of out -b missing
        conan export-pkg flexus -pr "flexus/target/_profile/{{profile}}" --name={{sim}} -of out
        lib=$(ls out/lib/*/lib{{sim}}.so)
    else
        build_dir="flexus/build-{{sim}}"
        cmake -S flexus -B "$build_dir" -G Ninja \
            -DCMAKE_BUILD_TYPE={{profile}} \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -DSIMULATOR={{sim}}
        ninja -C "$build_dir"
        lib="$build_dir/lib{{sim}}.so"
    fi
    if [ -n "{{mem}}" ]; then
        mkdir -p out/kraken/mem{{mem}}
        cp "$lib" out/kraken/mem{{mem}}/
    fi

# Memory controller counts the simulators are built for, covering 1 and 8 (1), 2 and 16 (2), 4 and 32 (4),
# 48 (6) and 64 (8) cores. Must match MEMORY_CONTROLLER_POSITIONS in commands/config.py.
kraken_mem_counts := "1 2 4 6 8"

# qflex picks the variant matching the experiment's core count.
# Build both Flexus simulators for every memory controller count into out/kraken/mem<N>/.
kraken:
    for mem in {{kraken_mem_counts}}; do \
        for sim in knottykraken semikraken; do {{just_executable()}} flexus $sim $mem; done; \
    done

# Build timing-qemu (the detailed timing model).
timing-qemu: bxdb
    cd timing-qemu && ./configure {{qemu_common}} {{timing_flags}} {{ if profile == "debug" { "--enable-debug" } else { "" } }} && make -j{{jobs}}

# Build fw-qemu (the functional / warm-up model).
fw-qemu: bxdb
    cd fw-qemu && ./configure {{qemu_common}} && ninja -C build

# Build the whole stack.
all: bxdb kraken timing-qemu fw-qemu

# Remove generated build outputs.
clean:
    rm -rf out bxdb/target timing-qemu/build fw-qemu/build
    rm -rf flexus/build-knottykraken flexus/build-semikraken
