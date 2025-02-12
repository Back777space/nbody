{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    libgcc
    gdb
  ];

  buildInputs = with pkgs; [
    glfw
    glfw3
    glm
    libGL
    libGLU
    mesa
    pkg-config

    xorg.libXcursor
    xorg.libXinerama
    xorg.libXext
    xorg.libXxf86vm

    libdecor
  ];

  shellHook = ''
    echo "OpenGL development environment is ready!"
  '';
}
