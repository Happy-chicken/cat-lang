
cmake --build ./build;
./build/CatLang ./test/testjit.cat;
# lli 
clang++-14 -O3 -I/usr/include/gc ./output.ll /usr/lib/x86_64-linux-gnu/libgc.a -o catlang;
./catlang;