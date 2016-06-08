#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <cstdint>
#include <cstring>
#include <GLFW/glfw3.h>

using namespace std;
using namespace std::chrono;

static void key_callback(GLFWwindow* window, 
                         int key, int scancode, 
                         int action, int mods);

class interpreter {
    array<uint8_t, 4096> memory;
    array<uint16_t, 16> stack;
    struct {
        array<uint8_t, 16> g;
        uint16_t i;
        uint8_t dt;
        uint8_t st;
        uint16_t pc;
        uint8_t sp;
    } reg;
    uint8_t key;
    bool stopped;
    uint8_t locate_key_in;
    struct {
        array<array<uint8_t, 8>, 32> mem;
        bool get(int i, int j) { return (mem[j][i >> 3] >> (i & 7)) & 1; }
        void set(int i, int j) { mem[j][i >> 3] |= (1 << (i & 7)); }
        void unset(int i, int j) { mem[j][i >> 3] &= ~(1 << (i & 7)); }
        void set_xor(int i, int j, bool b) { 
            if (get(i, j) == b) unset(i, j);
            else                set(i, j);
        }
        void draw() {
            glClear(GL_COLOR_BUFFER_BIT);
            glBegin(GL_TRIANGLES);
            for (int i = 0; i < 32; ++i) {
                for (int j = 0; j < 8; ++j) {
                    uint8_t octet = mem[i][j];
                    for (int k = 0; k < 8; ++k) {
                        int l = 8 * j + k;
                        if ((octet >> k) & 1) glColor3f(1.f, 1.f, 1.f);
                        else                  glColor3f(0.f, 0.f, 0.f);
                        glVertex2f(10.f * l, 10.f * i);
                        glVertex2f(10.f * l + 10.f, 10.f * i);
                        glVertex2f(10.f * l + 10.f, 10.f * i + 10.f);
                        glVertex2f(10.f * l, 10.f * i);
                        glVertex2f(10.f * l, 10.f * i + 10.f);
                        glVertex2f(10.f * l + 10.f, 10.f * i + 10.f);
                    }
                }
            }
            glEnd();
        }
        void clear() { for (auto & m : mem) m.fill(0); }
    } display;

    friend void key_callback(GLFWwindow* window, 
                             int key, int scancode, 
                             int action, int mods);
public:
    interpreter() {
        memory.fill(0);
        static uint8_t sprites[] = {
            0xF, 0x0, 0x9, 0x0, 0x9, 0x0, 0x9, 0x0, 0xF, 0x0, // "0"
            0x2, 0x0, 0x6, 0x0, 0x2, 0x0, 0x2, 0x0, 0x7, 0x0, // "1"
            0xF, 0x0, 0x1, 0x0, 0xF, 0x0, 0x8, 0x0, 0xF, 0x0, // "2"
            0xF, 0x0, 0x1, 0x0, 0xF, 0x0, 0x1, 0x0, 0xF, 0x0, // "3"
            0x9, 0x0, 0x9, 0x0, 0xF, 0x0, 0x1, 0x0, 0x1, 0x0, // "4"
            0xF, 0x0, 0x8, 0x0, 0xF, 0x0, 0x1, 0x0, 0xF, 0x0, // "5"
            0xF, 0x0, 0x8, 0x0, 0xF, 0x0, 0x9, 0x0, 0xF, 0x0, // "6"
            0xF, 0x0, 0x1, 0x0, 0x2, 0x0, 0x4, 0x0, 0x4, 0x0, // "7"
            0xF, 0x0, 0x9, 0x0, 0xF, 0x0, 0x9, 0x0, 0xF, 0x0, // "8"
            0xF, 0x0, 0x9, 0x0, 0xF, 0x0, 0x1, 0x0, 0xF, 0x0, // "9"
            0xF, 0x0, 0x9, 0x0, 0xF, 0x0, 0x9, 0x0, 0x9, 0x0, // "A"
            0xE, 0x0, 0x9, 0x0, 0xE, 0x0, 0x9, 0x0, 0xE, 0x0, // "B"
            0xF, 0x0, 0x8, 0x0, 0x8, 0x0, 0x8, 0x0, 0xF, 0x0, // "C"
            0xE, 0x0, 0x9, 0x0, 0x9, 0x0, 0x9, 0x0, 0xE, 0x0, // "D"
            0xF, 0x0, 0x8, 0x0, 0xF, 0x0, 0x8, 0x0, 0xF, 0x0, // "E"
            0xF, 0x0, 0x8, 0x0, 0xF, 0x0, 0x8, 0x0, 0x8, 0x0, // "F"
        };
        copy(begin(sprites), end(sprites), memory.begin());
        stack.fill(0);
        reg.g.fill(0);
        display.clear();
    }

    void load(const string & filename) {
        fstream program(filename, ios_base::in | ios_base::binary);
        copy(istream_iterator<uint8_t>(program), istream_iterator<uint8_t>(), 
             memory.begin() + 0x200);
    }

    void dump_memory() {
        for (int i = 0; i < 128; ++i) {
            for (int j = 0; j < 32; ++j) {
                cout << setw(2) << (int) memory[i * 32 + j]; 
            }
            cout << endl;
        }
        cout << endl;
    }

#define _X      ((instr >> 8) & 0x000f)
#define _Y      ((instr >> 4) & 0x000f)
#define _Z       (instr       & 0x000f)
#define _YZ      (instr       & 0x00ff)
#define _ADDR    (instr       & 0x0fff)

    template <typename T>
    string to_hex(T val) {
        static const char * h = "0123456789abcdef";
        string res = "0x";
        for (int i = 8 * sizeof(T) - 4; i >= 0; i -= 4)
            res += h[(val >> i) & 0xf];
        return res;
    }

    void disassemble(uint16_t from = 0, uint16_t to = 4096) {
        uint16_t ip = from;
        while (ip != to) {
            uint16_t instr = readw(ip);
            cout << to_hex(ip) << ": " << to_hex(instr) << " - ";
            switch (instr & 0xf000) {
                default:
                    cout << "not instruction" << endl; break;
                case 0x0000:
                    if (instr == 0x00e0) { // 00E0 - CLS
                        cout << "cls" << endl;
                    }
                    else if (instr == 0x00ee) { // 00EE - RET
                        cout << "ret" << endl;
                    } else { // 0nnn - SYS addr 
                        cout << "sys " << _ADDR << endl;
                    }
                    break;
                case 0x1000: // 1nnn - JP addr
                    cout << "jp " << _ADDR << endl; break;
                case 0x2000: // 2nnn - CALL addr
                    cout << "call " << _ADDR << endl; break;
                case 0x3000: // 3xkk - SE Vx, byte
                    cout << "se V" << _X << ", " << _YZ << endl;
                    break;
                case 0x4000: // 4xkk - SNE Vx, byte
                    cout << "sne V" << _X << ", " << _YZ << endl;
                    break;
                case 0x5000: // 5xy0 - SE Vx, Vy
                    cout << "se V" << _X << ", V" << _Y << endl;
                    break;
                case 0x6000: // 6xkk - LD Vx, byte
                    cout << "ld V" << _X << ", " << _YZ << endl;
                    break;
                case 0x7000: // 7xkk - ADD Vx, byte
                    cout << "add V" << _X << ", " << _YZ << endl;
                    break;
                case 0x8000:
                    switch (_Z) {
                        default:
                            cout << "not instruction" << endl; break;
                        case 0x0: // 8xy0 - LD Vx, Vy
                            cout << "ld V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0x1: // 8xy1 - OR Vx, Vy
                            cout << "or V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0x2: // 8xy2 - AND Vx, Vy
                            cout << "and V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0x3: // 8xy3 - XOR Vx, Vy
                            cout << "xor V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0x4: // 8xy4 - ADD Vx, Vy
                            cout << "add V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0x5: // 8xy5 - SUB Vx, Vy
                            cout << "sub V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0x6: // 8xy6 - SHR Vx {, Vy}
                            cout << "shr V" << _X << endl; break;
                        case 0x7: // 8xy7 - SUBN Vx, Vy
                            cout << "subn V" << _X 
                                 << ", V" << _Y << endl; break;
                        case 0xe: // 8xyE - SHL Vx {, Vy}
                            cout << "shl V" << _X << endl; break;
                    }
                    break;
                case 0x9000: // 9xy0 - SNE Vx, Vy
                    cout << "sne V" << _X 
                         << ", V" << _Y << endl; break;
                case 0xa000: // Annn - LD I, addr
                    cout << "ld I, V" << _ADDR << endl; break;
                case 0xb000: // Bnnn - JP V0, addr
                    cout << "jp V0, " << _ADDR << endl; break;
                case 0xc000: // Cxkk - RND Vx, byte
                    cout << "rnd V" << _X 
                         << ", " << _YZ << endl; break;
                case 0xd000: // Dxyn - DRW Vx, Vy, nibble
                    cout << "drw V" << _X 
                         << ", V" << _Y 
                         << ", " << _Z << endl; break;
                case 0xe000: 
                    if (_Y == 0x9) { // Ex9E - SKP Vx
                        cout << "skp V" << _X << endl; 
                    } else if (_Y == 0xa) { // ExA1 - SKNP Vx
                        cout << "sknp V" << _X << endl; 
                    } else {
                        cout << "not instruction" << endl; 
                    }
                    break;
                case 0xf000: 
                    switch (_YZ) {
                        default:
                            cout << "not instruction" << endl; break;
                        case 0x07: // Fx07 - LD Vx, DT
                            cout << "ld V" << _X << ", DT" << endl;
                            break;
                        case 0x0a: // Fx0A - LD Vx, K
                            cout << "ld V" << _X << ", K" << endl;
                            break;
                        case 0x15: // Fx15 - LD DT, Vx
                            cout << "ld DT, V" << _X << endl; break;
                        case 0x18: // Fx18 - LD ST, Vx
                            cout << "ld ST, V" << _X << endl; break;
                        case 0x1e: // Fx1E - ADD I, Vx
                            cout << "add I, V" << _X << endl; break;
                        case 0x29: // Fx29 - LD F, Vx
                            cout << "ld F, V" << _X << endl; break;
                        case 0x33: // Fx33 - LD B, Vx
                            cout << "ld B, V" << _X << endl; break;
                        case 0x55: // Fx55 - LD [I], Vx
                            cout << "ld [I], V" << _X << endl; break;
                        case 0x65: // Fx65 - LD Vx, [I]
                            cout << "ld V" << _X << ", [I]" << endl; 
                            break;
                    }
                    break;
            }
            ip += 2;
        }
    }

    void execute() {
        reg.pc = 0x200;
        reg.sp = 1;
        stopped = false;
        locate_key_in = 0;
        stack[0] = 0x200;
        GLFWwindow * window 
            = glfwCreateWindow(640, 320, "Hello World", NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            exit(-1);
        }
        glfwSetKeyCallback(window, key_callback);
        glfwMakeContextCurrent(window);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 640, 320, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0.5, 0.5, 0.5, 1.0);

        while (!glfwWindowShouldClose(window) && reg.sp > 0) {
            if (stopped) {
                cout << "stopped" << endl;
                continue;
            }
            exec_instruction();
            display.draw();
            glfwSwapBuffers(window);
            glfwPollEvents();
            this_thread::sleep_for(milliseconds(15));
            if (reg.dt) --reg.dt;
            if (reg.st) --reg.st;
        }
    }

    uint16_t readw(uint16_t addr) {
        uint16_t res = memory[addr];
        res <<= 8;
        res |= memory[addr + 1];
        return res;
    }

    uint8_t readb(uint16_t addr) { return memory[addr]; }

    void writew(uint16_t addr, uint16_t val) {
        memory[addr] = val & 0x00ff;
        memory[addr + 1] = val >> 8;
    }

    void writeb(uint16_t addr, uint8_t val) { memory[addr] = val; }

    template <typename It>
    void print(It begin, It end) {
        if (begin == end) cout << "[]" << endl;
        cout << "[" << setw(3) << (int) *begin;
        while (++begin != end) {
            cout << "," << setw(3) << (int) *begin;
        }
        cout << "]";
    }

    void print_state(uint16_t instr) {
        cout << hex;
        cout << setw(13) << "stack: "; 
        print(begin(stack), begin(stack) + reg.sp); cout << endl;
        cout << setw(13) << "registers: " << endl;
        for (int i = 0; i < 16; ++i) {
            cout << setw(17) << "v" << i << " = " << (int) reg.g[i] << endl;
        }
        cout << setw(18) << "I " << " = " << (int) reg.i << endl;
        cout << setw(18) << "dt" << " = " << (int) reg.dt << endl;
        cout << setw(18) << "st" << " = " << (int) reg.st << endl;
        cout << setw(18) << "pc" << " = " << (int) reg.pc << endl;
        cout << endl;
        cout << dec;
    }
    
    void exec_instruction() {
        uint16_t instr = readw(reg.pc);
        print_state(instr);
        switch (instr & 0xf000) {
            case 0x0000:
                if (instr == 0x00e0) { // 00E0 - CLS
                    display.clear();
                }
                else if (instr == 0x00ee) { // 00EE - RET
                    reg.pc = stack[--reg.sp];
                    return;
                } else { // 0nnn - SYS addr 
                    reg.pc = _ADDR;
                    return;
                }
                break;
            case 0x1000: // 1nnn - JP addr
                reg.pc = _ADDR; return;
            case 0x2000: // 2nnn - CALL addr
                stack[reg.sp++] = reg.pc; reg.pc = _ADDR; return;
            case 0x3000: // 3xkk - SE Vx, byte
                if (reg.g[_X] == _YZ) reg.pc += 4; break;
            case 0x4000: // 4xkk - SNE Vx, byte
                if (reg.g[_X] != _YZ) reg.pc += 4; break;
            case 0x5000: // 5xy0 - SE Vx, Vy
                if (reg.g[_X] == reg.g[_Y]) reg.pc += 4; break;
            case 0x6000: // 6xkk - LD Vx, byte
                reg.g[_X] = _YZ; break;
            case 0x7000: // 7xkk - ADD Vx, byte
                reg.g[_X] += _YZ; break;
            case 0x8000:
                switch (_Z) {
                    case 0x0: // 8xy0 - LD Vx, Vy
                        reg.g[_X] = reg.g[_Y]; break;
                    case 0x1: // 8xy1 - OR Vx, Vy
                        reg.g[_X] |= reg.g[_Y]; break;
                    case 0x2: // 8xy2 - AND Vx, Vy
                        reg.g[_X] &= reg.g[_Y]; break;
                    case 0x3: // 8xy3 - XOR Vx, Vy
                        reg.g[_X] ^= reg.g[_Y]; break;
                    case 0x4: // 8xy4 - ADD Vx, Vy
                        reg.g[_X] += reg.g[_Y];
                        reg.g[15] = reg.g[_X] < reg.g[_Y]; break;
                    case 0x5: // 8xy5 - SUB Vx, Vy
                        reg.g[15] = reg.g[_X] < reg.g[_Y];
                        reg.g[_X] -= reg.g[_Y]; break;
                    case 0x6: // 8xy6 - SHR Vx {, Vy}
                        reg.g[15] = reg.g[_X] % 2;
                        reg.g[_X] >>= 1; break;
                    case 0x7: // 8xy7 - SUBN Vx, Vy
                        reg.g[15] = reg.g[_X] < reg.g[_Y];
                        reg.g[_Y] -= reg.g[_X]; break;
                    case 0xe: // 8xyE - SHL Vx {, Vy}
                        reg.g[15] = reg.g[_X] >> 7;
                        reg.g[_X] <<= 1; break;
                }
                break;
            case 0x9000: // 9xy0 - SNE Vx, Vy
                if (reg.g[_X] != reg.g[_Y]) reg.pc += 4; break;
            case 0xa000: // Annn - LD I, addr
                reg.i = _ADDR; break;
            case 0xb000: // Bnnn - JP V0, addr
                reg.pc = _ADDR + reg.g[0]; return;
            case 0xc000: // Cxkk - RND Vx, byte
                reg.g[_X] = (rand() % 0xff) & _YZ; break;
            case 0xd000: // Dxyn - DRW Vx, Vy, nibble
                for (int k = 0, j = _Y; k < _Z; ++k, ++j) {
                    uint8_t octet = readb(reg.i + k);
                    for (int l = 7, i = _X; l >= 0; ++i, --l) {
                        bool bit = (octet >> l) & 1;
                        display.set_xor(i, j, bit);
                    }
                }
                break;
            case 0xe000: 
                if (_Y == 0x9) { // Ex9E - SKP Vx
                    if (key == _X) reg.pc += 4; return;
                } else if (_Y == 0xa) { // ExA1 - SKNP Vx
                    if (key != _X) reg.pc += 4; return;
                }
                break;
            case 0xf000: 
                switch (_YZ) {
                    case 0x07: // Fx07 - LD Vx, DT
                        reg.g[_X] = reg.dt; break;
                    case 0x0a: // Fx0A - LD Vx, K
                        stopped = true;
                        locate_key_in = _X;
                        break;
                    case 0x15: // Fx15 - LD DT, Vx
                        reg.dt = reg.g[_X]; break;
                    case 0x18: // Fx18 - LD ST, Vx
                        reg.st = reg.g[_X]; break;
                    case 0x1e: // Fx1E - ADD I, Vx
                        reg.i += reg.g[_X]; break;
                    case 0x29: // Fx29 - LD F, Vx
                        reg.i = _X * 10; break;
                    case 0x33: { // Fx33 - LD B, Vx
                        uint16_t x = reg.g[_X];
                        writeb(reg.i + 2, x % 10); x /= 10;
                        writeb(reg.i + 1, x % 10); x /= 10;
                        writeb(reg.i    , x     );
                        break;
                    }
                    case 0x55: // Fx55 - LD [I], Vx
                        for (int i = 0; i < _X; ++i)
                            writeb(reg.i + i, reg.g[i]);
                        break;
                    case 0x65: // Fx65 - LD Vx, [I]
                        for (int i = 0; i < _X; ++i)
                            reg.g[i] = readb(reg.i + i);
                        break;
                }
                break;
        }
        reg.pc += 2;
    }
};

interpreter interp;

static void key_callback(GLFWwindow* window, 
                         int key, int scancode, 
                         int action, int mods)
{
    if (action != GLFW_PRESS) return;
         if (key == GLFW_KEY_0) interp.key = 0x0;
    else if (key == GLFW_KEY_1) interp.key = 0x1;
    else if (key == GLFW_KEY_2) interp.key = 0x2;
    else if (key == GLFW_KEY_3) interp.key = 0x3;
    else if (key == GLFW_KEY_4) interp.key = 0x4;
    else if (key == GLFW_KEY_5) interp.key = 0x5;
    else if (key == GLFW_KEY_6) interp.key = 0x6;
    else if (key == GLFW_KEY_7) interp.key = 0x7;
    else if (key == GLFW_KEY_8) interp.key = 0x8;
    else if (key == GLFW_KEY_9) interp.key = 0x9;
    else if (key == GLFW_KEY_A) interp.key = 0xa;
    else if (key == GLFW_KEY_B) interp.key = 0xb;
    else if (key == GLFW_KEY_C) interp.key = 0xc;
    else if (key == GLFW_KEY_D) interp.key = 0xd;
    else if (key == GLFW_KEY_E) interp.key = 0xe;
    else if (key == GLFW_KEY_F) interp.key = 0xf;
    interp.reg.g[interp.locate_key_in] = interp.key;
    interp.stopped = false;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        cout << argv[0] << " filename" << endl;
        return 0;
    }
    if (!glfwInit())
        return -1;

    interp.load(argv[1]);
    interp.execute();
    glfwTerminate();
    return 0;
}
