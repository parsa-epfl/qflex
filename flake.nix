{
  description = "QFlex build environment (flexus, timing-qemu, fw-qemu and bxdb)";

  # Each time this url is changed, please rerun `nix flake update nixpkgs`
  # to update the lock file.
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
  let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };

    # Dependencies required by the recipes in ./justfile. The build commands
    # themselves live there so that the bare, Nix and Docker builds all follow
    # the same path.
    buildInputs = with pkgs; [
      # Build tooling
      ninja
      cmake
      pkg-config

      # QEMU + Flexus native dependencies
      glib
      pixman
      capstone
      libslirp
      libgcrypt
      zstd

      # QEMU's mkvenv bootstraps its build venv and needs distlib.
      (python3.withPackages (ps: [ ps.distlib ]))
      git
      pbzip2

      flex
      bison

      # Boost 1.83 matches the Conan profile. Newer Boost made Boost.System
      # header-only and dropped its CMake component config, which breaks
      # flexus' find_package(Boost ... COMPONENTS system ...).
      boost183

      libtinfo
      ncurses6

      # Rust toolchain: builds bxdb (and WormCache at runtime)
      cargo
      rustc

      # Single build recipe runner
      just
    ];
  in
  {
    devShells.${system}.default = pkgs.mkShell.override {
      stdenv = pkgs.llvmPackages_19.stdenv;
    } {
      packages = buildInputs;
      shellHook = ''
        echo "QFlex Nix shell ready. Build the whole stack with: just all"
      '';
    };
  };
}
