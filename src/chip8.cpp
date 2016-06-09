#include "chip8.h"
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iterator>
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace std;
using namespace std::chrono;
using namespace sf;

template <typename T>
string hex(T val) {
    static const char * h = "0123456789abcdef";
    const auto len = sizeof(T) * 2;
    string res;
    res.resize(len);
    for (int i = 0; i < len; ++i)
        res[i] = h[(val >> (4 * (len - 1 - i))) & 0xf];
    return res;
}

struct unknown_instruction_exception : runtime_error {
    unknown_instruction_exception(uint16_t in)
            : runtime_error("unknown instruction: " + hex(in)) { }
};

vector<string> show(uint16_t in) {
    uint16_t nnn = in & 0x0fff;
    uint8_t n    = in & 0x000f;
    uint8_t x    = (in >> 8) & 0x000f;
    uint8_t y    = (in >> 4) & 0x000f;
    uint8_t kk   = in & 0x00ff;
    vector<string> res;
    switch (in & 0xf000) {
    case 0x0000: 
        switch (kk) {
        default: throw unknown_instruction_exception(in);
        case 0xee: res = { "ret" }; break;
        case 0xe0: res = { "cls" }; break;
        } break;
    case 0x1000: res = { "jp", "0x" + hex(nnn) }; break;
    case 0x2000: res = { "call", "0x" + hex(nnn) }; break;
    case 0x3000: res = { "se", "V" + hex(x), "0x" + hex(kk) }; break;
    case 0x4000: res = { "sne", "V" + hex(x), "0x" + hex(kk) }; break;
    case 0x5000: res = { "se", "V" + hex(x), "V" + hex(y) }; break;
    case 0x6000: res = { "ld", "V" + hex(x), "0x" + hex(kk) }; break;
    case 0x7000: res = { "add", "V" + hex(x), "0x" + hex(kk) }; break;
    case 0x8000: 
        switch (n) {
        default: throw unknown_instruction_exception(in);
        case 0x0: res = { "ld", "V" + hex(x), "V" + hex(y) }; break;
        case 0x1: res = { "or", "V" + hex(x), "V" + hex(y) }; break;
        case 0x2: res = { "and", "V" + hex(x), "V" + hex(y) }; break;
        case 0x3: res = { "xor", "V" + hex(x), "V" + hex(y) }; break;
        case 0x4: res = { "add", "V" + hex(x), "V" + hex(y) }; break;
        case 0x5: res = { "sub", "V" + hex(x), "V" + hex(y) }; break;
        case 0x6: res = { "shr", "V" + hex(x) }; break;
        case 0x7: res = { "subn", "V" + hex(x), "V" + hex(y) }; break;
        case 0xe: res = { "shl", "V" + hex(x) }; break;
        } break;
    case 0x9000: res = { "sne", "V" + hex(x), "V" + hex(y) }; break;
    case 0xa000: res = { "ld", "I", "0x" + hex(nnn) }; break;
    case 0xb000: res = { "jp", "V0", "0x" + hex(nnn) }; break;
    case 0xc000: res = { "rnd", "V" + hex(x), "0x" + hex(kk) }; break;
    case 0xd000: res = { "drw", "V" + hex(x), "V" + hex(y), "0x" + hex(n) }; 
                 break;
    case 0xe000: 
        switch (kk) {
        default: throw unknown_instruction_exception(in);
        case 0x9e: res = { "skp", "V" + hex(x) }; break;
        case 0xa1: res = { "sknp", "V" + hex(x) }; break;
        } break;
    case 0xf000: 
        switch (kk) {
        default: throw unknown_instruction_exception(in);
        case 0x07: res = { "ld", "V" + hex(x), "DT" }; break;
        case 0x0a: res = { "ld", "V" + hex(x), "K" }; break;
        case 0x15: res = { "ld", "DT", "V" + hex(x) }; break;
        case 0x18: res = { "ld", "ST", "V" + hex(x) }; break;
        case 0x1e: res = { "add", "I", "V" + hex(x) }; break;
        case 0x29: res = { "ld", "F", "V" + hex(x) }; break;
        case 0x33: res = { "ld", "B", "V" + hex(x) }; break;
        case 0x55: res = { "ld", "[I]", "V" + hex(x) }; break;
        case 0x65: res = { "ld", "V" + hex(x), "[I]" }; break;
        }
    }
    return res;
}

struct chip8::impl_t {

    struct display_t {
        array<array<bool, 32>, 64> mem;

        bool draw(int i, int j, const vector<uint8_t> & sprite) {
            bool erased = false;
            for (int k = 0; k < sprite.size(); ++k)
                for (int l = 7; l >= 0; --l) {
                    bool bit = (sprite[k] >> l) & 1;
                    if (!bit) continue;
                    bool & m = mem[(i + 7 - l + 64) % 64][(j + k + 32) % 32];
                    if (m) erased = true;
                    m = !m;
                }
            return erased;
        }

        void clear() { for (auto & col : mem) col.fill(0); }

        void redraw() {
            glClear(GL_COLOR_BUFFER_BIT);
            glColor3f(0.8, 0.8, 0.8);
            glBegin(GL_TRIANGLES);
            for (int i = 0; i < 64; ++i) {
                for (int j = 0; j < 32; ++j) {
                    if (!mem[i][j]) continue;
                    // top left
                    glVertex2f(10. * i, 10. * j);
                    glVertex2f(10. * i + 10., 10. * j);
                    glVertex2f(10. * i, 10. * j + 10.);
                    // bottom right
                    glVertex2f(10. * i + 10., 10. * j);
                    glVertex2f(10. * i, 10. * j + 10.);
                    glVertex2f(10. * i + 10., 10. * j + 10.);
                }
            }
            glEnd();
        }
    };

    /*
        Memory layout:
        0x0000-0x0050 sprites
        0x0200-0x0fdf program
        0x0fe0-0x3fff stack
    */

    static const uint16_t PROGRAM_START_ADDRESS = 0x0200;
    static const uint16_t STACK_ADDRESS = 0x0fe0;

    array<uint8_t, 4096> memory;
    array<uint8_t, 16> v;
    uint8_t key;
    uint16_t i;
    uint16_t pc;
    uint16_t sp;
    uint8_t dt;
    uint8_t st;
    display_t display;
    Window window;
    bool wait_for_key;
    uint8_t put_key_in;

    impl_t()
            : i(0), pc(PROGRAM_START_ADDRESS), dt(0)
            , st(0), sp(STACK_ADDRESS - 1) {
        memory.fill(0);
        display.clear();
        static uint8_t sprites[] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // "0"
            0x20, 0x60, 0x20, 0x20, 0x70, // "1"
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // "2"
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // "3"
            0x90, 0x90, 0xF0, 0x10, 0x10, // "4"
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // "5"
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // "6"
            0xF0, 0x10, 0x20, 0x40, 0x40, // "7"
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // "8"
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // "9"
            0xF0, 0x90, 0xF0, 0x90, 0x90, // "A"
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // "B"
            0xF0, 0x80, 0x80, 0x80, 0xF0, // "C"
            0xE0, 0x90, 0x90, 0x90, 0xE0, // "D"
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // "E"
            0xF0, 0x80, 0xF0, 0x80, 0x80, // "F"
        };
        copy(sprites, end(sprites), memory.begin());
        v.fill(0);
    }

    void load(istream & source) {
        copy(istreambuf_iterator<char>(source), 
             istreambuf_iterator<char>(),
             memory.begin() + 0x200);
    }

    uint16_t read_word(uint16_t addr) {
        uint16_t word = memory[addr];
        word = (word << 8) | memory[addr + 1];
        return word;
    }

    void write_word(uint16_t addr, uint16_t word) {
        memory[addr + 1] = word;
        memory[addr] = word >> 8;
    }

    vector<uint8_t> read_bytes(uint16_t addr, uint8_t length) {
        vector<uint8_t> result(length);
        copy_n(memory.begin() + addr, length, result.begin());
        return result;
    }

    void write_bcd(uint8_t x) {
        uint8_t val = v[x];
        memory[i    ] = val % 10;
        val /= 10;
        memory[i + 1] = val % 10;
        val /= 10;
        memory[i + 2] = val % 10;
    }

    void step() {
        uint16_t in  = read_word(pc);
        //try {
        //    auto s = show(in);
        //    cout << "0x" << hex(pc) << ":   " 
        //         << "<" << hex(in) << ">    " 
        //         << setw(6) << left << s[0] << " ";
        //    if (s.size() > 1) 
        //        cout << s[1];
        //    for (int i = 2; i < s.size(); ++i) 
        //        cout << ", " << s[i];
        //    cout << endl;
        //} catch (unknown_instruction_exception e) {
        //    cout << e.what() << endl;
        //}
        uint16_t nnn = in & 0x0fff;
        uint8_t n    = in & 0x000f;
        uint8_t x    = (in >> 8) & 0x000f;
        uint8_t y    = (in >> 4) & 0x000f;
        uint8_t kk   = in & 0x00ff;
        vector<string> res;
        switch (in & 0xf000) {
        case 0x0000: 
            switch (kk) {
            default: throw unknown_instruction_exception(in); // todo: ?
            case 0xee: pc = read_word(sp); sp -= 2; break;
            case 0xe0: display.clear(); break;
            } break;
        case 0x1000: pc = nnn - 2; break;
        case 0x2000: sp += 2; write_word(sp, pc); pc = nnn - 2; break;
        case 0x3000: pc += (v[x] == kk) ? 2 : 0; break;
        case 0x4000: pc += (v[x] != kk) ? 2 : 0; break;
        case 0x5000: pc += (v[x] == v[y]) ? 2 : 0; break;
        case 0x6000: v[x] = kk; break;
        case 0x7000: v[x] += kk; break;
        case 0x8000: 
            switch (n) {
            default: throw unknown_instruction_exception(in);
            case 0x0: v[x] = v[y]; break;
            case 0x1: v[x] |= v[y]; break;
            case 0x2: v[x] &= v[y]; break;
            case 0x3: v[x] ^= v[y]; break;
            case 0x4: v[x] += v[y]; v[0xf] = v[x] < v[y]; break;
            case 0x5: v[0xf] = v[x] > v[y]; v[x] -= v[y]; break;
            case 0x6: v[0xf] = v[x] & 1; v[x] >>= 1; break;
            case 0x7: v[0xf] = v[y] > v[x]; v[y] -= v[x]; break;
            case 0xe: v[0xf] = v[x] & (1 << 7); v[x] <<= 1; break;
            } break;
        case 0x9000: pc += (v[x] != v[y]) ? 2 : 0; break;
        case 0xa000: i = nnn; break;
        case 0xb000: pc = nnn + v[0] - 2; break;
        case 0xc000: v[x] = (rand() & 0xff) & kk; break;
        case 0xd000: v[0xf] = display.draw(v[x], v[y], read_bytes(i, n)); break;
        case 0xe000: 
            switch (kk) {
            default: throw unknown_instruction_exception(in);
            case 0x9e: pc += (v[x] == key && key < 0x10) ? 2 : 0; break;
            case 0xa1: pc += (v[x] != key && key < 0x10) ? 2 : 0; break;
            } break;
        case 0xf000: 
            switch (kk) {
            default: throw unknown_instruction_exception(in);
            case 0x07: v[x] = dt; break;
            case 0x0a: wait_for_key = true; put_key_in = x; break;
            case 0x15: dt = v[x]; break;
            case 0x18: st = v[x]; break;
            case 0x1e: i += v[x]; break;
            case 0x29: i = v[x] * 5; break;
            case 0x33: write_bcd(x); break;
            case 0x55: copy_n(v.begin(), x, memory.begin() + i); break;
            case 0x65: copy_n(memory.begin() + i, x, v.begin()); break;
            }
        }
        pc += 2;
    }

    void process_events() {
        Event event;
        while (window.pollEvent(event)) {
            switch (event.type) {
            case Event::Closed:
                window.close();
                break;
            case Event::KeyPressed:
                switch (event.key.code) {
                default: continue;
                /* 1 2 3 c
                   4 5 6 d
                   7 8 9 e
                   a 0 b f */
                case Keyboard::Num1: key = 0x1; break;
                case Keyboard::Num2: key = 0x2; break;
                case Keyboard::Num3: key = 0x3; break;
                case Keyboard::Num4: key = 0xc; break;
                case Keyboard::Q:    key = 0x4; break;
                case Keyboard::W:    key = 0x5; break;
                case Keyboard::E:    key = 0x6; break;
                case Keyboard::R:    key = 0xd; break;
                case Keyboard::A:    key = 0x7; break;
                case Keyboard::S:    key = 0x8; break;
                case Keyboard::D:    key = 0x9; break;
                case Keyboard::F:    key = 0xe; break;
                case Keyboard::Z:    key = 0xa; break;
                case Keyboard::X:    key = 0x0; break;
                case Keyboard::C:    key = 0xb; break;
                case Keyboard::V:    key = 0xf; break;
                }
                if (wait_for_key) {
                    wait_for_key = false;
                    v[put_key_in] = key;
                }
                break;
            case Event::KeyReleased: 
                key = 0x10; break;
            default:
                break;
            }
        }
    }

    void start() {
        using namespace std::literals::chrono_literals;

        window.create({ 640, 320 }, "Chip-8", 
                      Style::Titlebar | 
                      Style::Close);
        glOrtho(0, 640, 320, 0, -1, 1);
        high_resolution_clock::time_point prev_dec;
        high_resolution_clock::time_point prev_step;
        while (window.isOpen()) {
            process_events();
            auto elapsed = high_resolution_clock::now() - prev_step;
            if (!wait_for_key && elapsed >= 1ms) {
                try {
                    step();
                } catch (unknown_instruction_exception e) {
                    window.close();
                }
                prev_step = high_resolution_clock::now();
            }
            elapsed = high_resolution_clock::now() - prev_dec;
            if (elapsed >= 15ms) {
                if (dt) --dt;
                if (st) {
                    cout << '\a' << flush;
                    --st;
                }
                prev_dec = high_resolution_clock::now();
            }
            display.redraw();
            window.display();
        }
    }
};

chip8::chip8() : impl(new impl_t()) { }

void chip8::load(istream & source) { impl->load(source); }

void chip8::start() { impl->start(); }

void disassemble(istream & source, ostream & out) { 
    uint16_t addr = 0x200;
    uint16_t in;
    while (true) {
        source.read((char *) &in, 2);
        if (!source) break;
        in = (in >> 8) | (in << 8);
        try {
            auto s = show(in);
            out << "0x" << hex(addr) << ":   " 
                 << "<" << hex(in) << ">    " 
                 << setw(6) << left << s[0] << " ";
            if (s.size() > 1) 
                out << s[1];
            for (int i = 2; i < s.size(); ++i) 
                out << ", " << s[i];
            out << endl;
        } catch (unknown_instruction_exception e) {
            out << e.what() << endl;
        }
        addr += 2;
    }
}
