# Further Tasks

Follow-up work arising from the submodule modernization
(`bxdb`, `fw-qemu`, `timing-qemu`, `WormCache`).

## Tasks

- [ ] **Modify the building command for `bxdb`.** `bxdb` is a new Rust
      workspace (`Cargo.toml`, `justfile`) that is not wired into the QFlex
      build yet. Add/update the build step (Dockerfile, `build` script, or a
      dedicated Cargo/Conan step) so it is compiled together with the rest of
      the stack.
- [ ] **Properly handle Nix.** Update `flake.nix` and any Nix build entry
      points for the renamed submodules (`fw-qemu`, `timing-qemu`,
      `WormCache`) and the new `bxdb` component.
- [ ] **WormCache `unstable` -> `develop`.** In the `WormCache` repository,
      use the `unstable` branch to override (force) `develop`
      (reset/force-push `develop` to match `unstable`).
