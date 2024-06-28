{
  rev     ? "89c49874fb15f4124bf71ca5f42a04f2ee5825fd", # nixos-24.05
  sha256  ? "07mr5xmdba3i5qw68kvxs0w1l70pv6pg636dqqxi6s91hiazv4n8",
  nixpkgs ? builtins.fetchTarball {
    name   = "nixpkgs-${rev}";
    url    = "https://github.com/arximboldi/nixpkgs/archive/${rev}.tar.gz";
    sha256 = sha256;
  },
  command ? "bash",
}:

with import nixpkgs {};

let
  libomp-5 = runCommand "libomp-5" {} ''
    mkdir -p $out/lib
    ln -s ${llvmPackages.openmp}/lib/libomp.so $out/lib/libomp.so.5
  '';

in
(pkgs.buildFHSUserEnv {
  name = "video2x-env";
  targetPkgs = pkgs: (with pkgs; [
    python3
    swig
    pdm
    vulkan-headers
    vulkan-tools
    vulkan-loader
    linuxHeaders
    glslang
    shaderc
    mesa
    mesa.drivers
    libGL
    glib
    libomp-5
    llvmPackages.openmp
  ]);
  profile = ''
    # evdev fails to build when these are not set
    export CC=cc
    export CXX=c++

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib
  '';
  runScript = command;
}).env
