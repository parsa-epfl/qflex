{
  description = "A flake for building QEMU";

  # Each time this url is changed, please rerun `nix flake lock --update-input nixpkgs` to update the lock file
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
  let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in
  {

    devShells.${system}.default = pkgs.llvmPackages_19.stdenv.mkDerivation {
      pname = "QFlex";
      version = "1";
      src = ".";

      buildInputs = [
        pkgs.ninja

        pkgs.glib
        pkgs.pkg-config
        pkgs.pixman
        pkgs.capstone
        pkgs.libslirp
        pkgs.libgcrypt
        pkgs.zstd

        pkgs.python3
        pkgs.git
        pkgs.pbzip2

        pkgs.flex
        pkgs.bison

        pkgs.cmake
        pkgs.boost

        pkgs.libtinfo
        pkgs.ncurses6
      ];

      # This property is not required by mkDerivation, but appears as a environmental variable.
      # They can be used directly in the develop shell.
    };
  };
}
