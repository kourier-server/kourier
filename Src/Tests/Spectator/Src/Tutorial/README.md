export CC=clang
export CXX=clang++
export LD=lld
Qt6_ROOT=/home/glauco/Programming/Qt/SDK/6.10.3/gcc_64 cmake -G'Ninja' /home/glauco/Programming/MyProjects/Spectator
cmake --build . --target Spectator --parallel
cmake --install . --prefix install-root

export CC=clang
export CXX=clang++
export LD=lld
Qt6_ROOT=/home/glauco/Programming/Qt/SDK/6.10.3/gcc_64 Spectator_DIR=/tmp/spectator-build/install-root/lib/cmake cmake -G'Ninja' /home/glauco/Programming/MyProjects/Spectator/Src/Tutorial
cmake --build . --target SpectatorTutorialApp --parallel
./SpectatorTutorialApp