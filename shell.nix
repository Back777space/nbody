{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    gcc
    gdb
    pkg-config
  ];

  buildInputs = with pkgs; [
    glfw3
    glm
    imgui
    libGL
    mesa

    xorg.libXcursor
    xorg.libXinerama
    xorg.libXext
    xorg.libXxf86vm
    xorg.libX11
  ];

  shellHook = ''
    echo "N-Body OpenGL development environment loaded!"
  '';
}
