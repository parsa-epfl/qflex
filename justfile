# QFlex build recipe: the single source of truth for the bare, Nix and Docker
# builds. Each environment only provides the toolchain/dependencies; the
# commands below are shared by all of them:
#
#   * bare    : system packages + rustup                 -> `just all`
#   * nix     : `nix develop` (Flexus uses CMake there)  -> `just all`
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

# Configure flags shared by timing-qemu and fw-qemu.
qemu_common := "--target-list=aarch64-softmmu --disable-gtk --enable-capstone --enable-zstd --with-bxdb=" + bxdb_root

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

# Build bxdb, the Rust snapshot database linked by both QEMUs.
bxdb:
    cargo build --manifest-path bxdb/Cargo.toml -p bxdb-core --release

# Build a Flexus simulator (knottykraken by default).
flexus sim="knottykraken":
    #!/usr/bin/env bash
    set -euo pipefail
    if [ "{{use_conan}}" = "1" ]; then
        conan profile detect --force
        conan build flexus {{profile_flags}} --name={{sim}} -of out -b missing
        conan export-pkg flexus -pr "flexus/target/_profile/{{profile}}" --name={{sim}} -of out
    else
        build_dir="flexus/build-{{sim}}"
        cmake -S flexus -B "$build_dir" -G Ninja \
            -DCMAKE_BUILD_TYPE={{profile}} \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -DSIMULATOR={{sim}}
        ninja -C "$build_dir"
    fi

# Build timing-qemu (the detailed timing model).
timing-qemu: bxdb
    cd timing-qemu && ./configure {{qemu_common}} {{timing_flags}} {{ if profile == "debug" { "--enable-debug" } else { "" } }} && make -j{{jobs}}

# Build fw-qemu (the functional / warm-up model).
fw-qemu: bxdb
    cd fw-qemu && ./configure {{qemu_common}} && ninja -C build

# Build the whole stack.
all: bxdb (flexus "knottykraken") (flexus "semikraken") timing-qemu fw-qemu

# Remove generated build outputs.
clean:
    rm -rf out bxdb/target timing-qemu/build fw-qemu/build
    rm -rf flexus/build-knottykraken flexus/build-semikraken
