{
  description = "AtlasNet";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
      in {
 devShells.default = pkgs.buildFHSEnv {
  name = "atlasnet-dev-shell";

  targetPkgs = pkgs': with pkgs'; [
    bashInteractive
    starship
    git
    tini
    gdb
    binutils
    automake
    libtool
    m4
    autoconf
    curl
    zip
    unzip
    less
    ccache
    coreutils
    gnumake
    gcc
    cmake
    pkg-config

    util-linux

    mesa
    libGL
    libGLU

    xorg.xauth
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXrandr
    xorg.libXrender
    xorg.libXext

    glfw
    glew

    vulkan-loader
    vulkan-headers
    vulkan-validation-layers
    vulkan-tools
    shaderc

    premake5

    boost
    libunwind
    libexecinfo
  ];

  multiPkgs = pkgs': with pkgs'; [
    stdenv.cc.cc
    stdenv.cc.libc
  ];

  profile = ''
    export CC=gcc
    export CXX=g++
    export CCACHE_DIR=$PWD/.ccache
    export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [
      pkgs.libunwind
      pkgs.libexecinfo
      pkgs.gcc.cc.lib
      pkgs.glibc
      pkgs.mesa
      pkgs.libGL
      pkgs.boost
    ]}
  '';

  runScript = "bash";
};
  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
    pkgs.libunwind
    pkgs.libexecinfo
  ];


        packages.default = pkgs.stdenv.mkDerivation {
          pname = "myproject";
          version = "1.0";
          src = ./.;

          nativeBuildInputs = [ pkgs.cmake pkgs.pkg-config ];
          buildInputs = with pkgs; [
            glfw
            glew
            libGL
            vulkan-loader
            vulkan-headers
          ];

          cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];
        };
      });
}
