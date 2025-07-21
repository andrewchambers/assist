{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    gnumake
    curl
    scdoc
    pandoc
    mandoc
    pkg-config
    asciinema
  ];

  shellHook = ''
    echo "assist development environment"
  '';
}