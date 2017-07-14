#ifndef CONSOLE_PRINTER_H
#define CONSOLE_PRINTER_H

namespace utils
{
    class ConsolePrinter : public IPrinter
    {
    public:
        virtual void setColor(int color) override;
        virtual void print(std::string const & string) override;
        virtual void newline(void) override;
    };
};

#endif
