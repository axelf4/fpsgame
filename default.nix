{ pkgs ? import <nixpkgs> {} }: pkgs.stdenv.mkDerivation {
  name = "fpsgame";
  src = ./.;

  nativeBuildInputs = with pkgs; [ pkg-config cmake ];
  buildInputs = with pkgs; [ libGL glew libpng freetype harfbuzz ];

  SDL2DIR = pkgs.SDL2;

  cmakeFlags = [
    "-DSDL2_INCLUDE_DIR='${pkgs.SDL2.dev}/include/SDL2'"
  ];
}
