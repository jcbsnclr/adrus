{ pkgs ? import <nixpkgs> {}}:

pkgs.mkShell rec {
  nativeBuildInputs = with pkgs; [
    pkg-config
  ];
  
  buildInputs = with pkgs; [
    tokei
    pkg-config
    gcc
    clang-tools
    gnumake
    linenoise
    lldb
    bear
    gdb
  ];

  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath buildInputs;
  ADRUS_DIR = "test/";
  LOG_FILTER = "trace";

  shellHook = ''
    export PATH="$PWD/util:$PATH"
  '';
}
