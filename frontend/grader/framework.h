#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "helper.h"

struct TestCase;

extern std::vector<TestCase> tests;
extern uint32_t verify_count;
extern uint32_t verify_valid;

class BufferedPrinter : public utils::IPrinter
{
public:
    std::vector<char> display_buffer;

    void setColor(int color) {}
    void print(std::string const & string);
    void newline(void);
};

class FileInputter : public utils::IInputter
{
public:
    void beginInput(void) {}
    bool getChar(char & c);
    void endInput(void) {}
};

class StringInputter : public utils::IInputter
{
public:
    StringInputter(std::string const & source);

    void beginInput(void) {}
    bool getChar(char & c);
    void endInput(void) {}

private:
    std::string source;
    uint32_t pos;
};

struct TestCase
{
    std::string name;
    std::function<void(void)> test_func;
    uint32_t points;
    bool randomize;

    TestCase(char const * name, std::function<void(void)> test_func, uint32_t points, bool randomize)
    {
        this->name = name;
        this->test_func = test_func;
        this->points = points;
        this->randomize = randomize;
    }
};

#define REGISTER_TEST(name, function, points)                     \
    tests.emplace_back( #name , ( function ), ( points ), false); \
    do {} while(false)
#define REGISTER_RANDOM_TEST(name, function, points)              \
    tests.emplace_back( #name , ( function ), ( points ), true);  \
    do {} while(false)
#define VERIFY(check)                                             \
    verify_count += 1;                                            \
    std::cout << "  " << ( #check ) << " => ";                    \
    if(( check ) == true) {                                       \
        verify_valid += 1;                                        \
        std::cout << "yes\n";                                     \
    } else {                                                      \
        std::cout << "no\n";                                      \
    }                                                             \
    do {} while(false)
    
