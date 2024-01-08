{ pkgs ? import <nixpkgs> {}}:

pkgs.mkShell {
  buildInputs = with pkgs; [
    rustc
    cargo
    rustfmt
    rust-analyzer
    clippy
    tokei
    pkg-config
    openssl
  ];

  RUST_BACKTRACE = 1;
  RUST_LOG = "debug";
}
