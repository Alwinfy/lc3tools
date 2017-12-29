#include <iostream>

#include "core.h"
#include "console_printer.h"

void utils::ConsolePrinter::setColor(int color)
{
    switch(color) {
        case PRINT_COLOR_RED    : std::cout << "\033[31m"; break;
        case PRINT_COLOR_YELLOW : std::cout << "\033[33m"; break;
        case PRINT_COLOR_GREEN  : std::cout << "\033[32m"; break;
        case PRINT_COLOR_MAGENTA: std::cout << "\033[35m"; break;
        case PRINT_COLOR_BLUE   : std::cout << "\033[34m"; break;
        case PRINT_COLOR_GRAY   : std::cout << "\033[31;1m\033[30m"; break;
        case PRINT_COLOR_BOLD   : std::cout << "\033[1m" ; break;
        case PRINT_COLOR_RESET  : std::cout << "\033[0m" ; break;
        default                :                          break;
    }
}

void utils::ConsolePrinter::print(std::string const & string)
{
    std::cout << string << std::flush;
}

void utils::ConsolePrinter::newline(void)
{
    std::cout << "\n";
}
