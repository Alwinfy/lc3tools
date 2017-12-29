#ifndef OBJECT_FILE_UTILS_H
#define OBJECT_FILE_UTILS_H

namespace utils {
    class Statement
    {
    public:
        Statement(void) = default;
        Statement(uint16_t value, bool orig, std::string const & line) : value(value), orig(orig),
            line(line) {}

        uint16_t getValue(void) const { return value; }
        void setValue(uint16_t value) { this->value = value; }
        bool isOrig(void) const { return orig; }
        std::string getLine(void) const { return line; }

        friend std::ostream & operator<<(std::ostream & out, Statement const & in);
        friend std::istream & operator>>(std::istream & in, Statement & out);

    private:
        uint16_t value;
        bool orig;
        std::string line;
    };
};

#endif