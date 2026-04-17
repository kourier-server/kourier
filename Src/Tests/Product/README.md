export CC=clang
export CXX=clang++
export LD=lld
Qt6_ROOT=/home/glauco/Programming/Qt/SDK/6.10.3/gcc_64 cmake -G'Ninja' /home/glauco/Programming/MyProjects/Kourier
cmake --build . --target Lib --target Spectator --parallel
cmake --install . --prefix install-root

export CC=clang
export CXX=clang++
export LD=lld
Qt6_ROOT=/home/glauco/Programming/Qt/SDK/6.10.3/gcc_64 Kourier_DIR=/tmp/kourier-build/install-root/lib/cmake Spectator_DIR=/tmp/kourier-build/install-root/lib/cmake cmake -G'Ninja' /home/glauco/Programming/MyProjects/Kourier/Src/Tests/Product
cmake --build . --target ProductTests --parallel
./ProductTests
