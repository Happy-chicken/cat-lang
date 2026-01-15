#pragma once
struct Location {
    int line;
    int column;

    Location() : line(0), column(0) {}
    Location(unsigned int l, unsigned int c) : line(l), column(c) {}

    static Location builtIn() {
        return Location{-1, -1};
    }
};