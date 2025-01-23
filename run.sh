
cmake --build ./build;
./build/CatLang ./test/testjit.cat;
# lli-14 -extra-archive /usr/lib/x86_64-linux-gnu/libgc.a ./output.ll;
clang++-14 -O3 -I/usr/include/gc ./output.ll /usr/lib/x86_64-linux-gnu/libgc.a -o catlang;
./catlang;