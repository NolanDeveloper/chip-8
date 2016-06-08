#include "chip8.h"
#include <iostream>
#include <cstring>
#include <fstream>

using namespace std;

int main(int argc, char ** argv) {
    if (argc < 3) {
        cout << argv[0] << " -r <program file> ; to run program" << endl;
        cout << argv[0] << " -d <program file> ; to disassemble program" << endl;
        return 0;
    }
    if (strcmp(argv[1], "-r") && strcmp(argv[1], "-d")) {
        cout << "unknown argument: " << argv[2] << endl;
        return -1;
    }
    if (argv[1][1] == 'r') {
        chip8 chip;
        fstream source(argv[2], ios_base::in | ios_base::binary);   
        if (!source) {
            cout << "error: can't open file" << endl;
            return -1;
        }
        chip.load(source);
        chip.start();
    } else {
        fstream source(argv[2], ios_base::in | ios_base::binary);   
        if (!source) {
            cout << "error: can't open file" << endl;
            return -1;
        }
        disassemble(source, cout);
    }
    return 0;
}
