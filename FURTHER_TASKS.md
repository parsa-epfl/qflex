# Further Tasks

Follow-up work arising from the submodule modernization
(`bxdb`, `fw-qemu`, `timing-qemu`, `WormCache`).

## Tasks

- [x] **Unified build recipe.** All build commands now live in `./justfile`
      (`bxdb`, `flexus`, `timing-qemu`, `fw-qemu`, `all`). The bare, Nix and
      Docker builds call the same recipes; `./build` is a thin wrapper.
- [x] **Modify the building command for `bxdb` (Docker).** The base
      `Dockerfile` installs Rust + `just` and calls the recipes;
      `Dockerfile.WormCache` no longer installs Rust.
- [x] **Modify the building command for `bxdb` (Nix).** `flake.nix` provides
      `cargo`/`rustc` and `just` in the devShell, so `just all` builds `bxdb`
      and points both QEMUs at it.
- [x] **Properly handle Nix.** `flake.nix` is now a `mkShell` with the deps for
      the renamed submodules (`fw-qemu`, `timing-qemu`) and `bxdb`; the nixpkgs
      lock was refreshed for `edition 2024` Rust (>= 1.85).
- [ ] **WormCache `unstable` -> `develop`.** In the `WormCache` repository,
      use the `unstable` branch to override (force) `develop`
      (reset/force-push `develop` to match `unstable`).
