
cmake --build ./build;
./build/CatLang ./test.cat;
# lli 
clang++ -O0 -I/usr/include/gc ./output.ll /usr/lib/x86_64-linux-gnu/libgc.a -o catlang;
./catlang;