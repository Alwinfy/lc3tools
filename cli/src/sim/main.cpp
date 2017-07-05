#include <iostream>
#include <stdexcept>
#include <string>

#include "core.h"

#include "../common/console_printer.h"

int main(int argc, char *argv[])
{
    utils::IPrinter * printer = new utils::ConsolePrinter();
    core::lc3 interface(*printer);

    if(argc < 2) {
        //printer->printf(utils::PRINT_TYPE_ERROR, "usage: %s file [file ...]", argv[0]);
    } else {
        for(int i = 1; i < argc; i += 1) {
            try {
                interface.loadSimulator(std::string(argv[i]));
            } catch (core::exception const & e) {
                printer->print(e.what());
                printer->newline();
            }
        }

        try {
            interface.simulate();
        } catch(core::exception const & e) {
            printer->print(e.what());
            printer->newline();
        }
    }

    delete printer;

    return 0;
}
