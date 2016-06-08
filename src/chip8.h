#pragma once

#include <string>
#include <memory>

class chip8 {

    class impl_t;
    impl_t * impl;

public:

    chip8();
    chip8(const chip8 & c) = delete;
    chip8 & operator=(const chip8 & c) = delete;

    void load(std::istream & source);
    void start();

};

void disassemble(std::istream & source, std::ostream & out);
