#include "console_printer.h"
#include "console_inputter.h"
#include "framework.h"

void setup(void);
void testBringup(lc3::sim & sim);
void testTeardown(lc3::sim & sim);

std::vector<TestCase> tests;
uint32_t verify_count;
uint32_t verify_valid;


bool endsWith(std::string const & search, std::string const & suffix)
{
    if(suffix.size() > search.size()) { return false; }
    return std::equal(suffix.rbegin(), suffix.rend(), search.rbegin());
}

void BufferedPrinter::print(std::string const & string)
{
    std::copy(string.begin(), string.end(), std::back_inserter(display_buffer));
}

void BufferedPrinter::newline(void) { display_buffer.push_back('\n'); }

bool FileInputter::getChar(char & c)
{
    return false;
}

StringInputter::StringInputter(std::string const & source)
{
    this->source = source;
    this->pos = 0;
}

bool StringInputter::getChar(char & c)
{
    if(pos == source.size()) {
        return false;
    }

    c = source[pos];
    pos += 1;
    return true;
}

int main(int argc, char ** argv)
{
    lc3::ConsolePrinter asm_printer;
    lc3::as assembler(asm_printer);

    std::vector<std::string> obj_filenames;
    bool valid_assembly = true;
    for(int i = 1; i < argc; i += 1) {
        std::string filename(argv[i]);

        
	if(endsWith(filename, ".bin")) {
	    assembler.convertBin(filename);
	} else {
            auto result = assembler.assemble(filename);
            if(! result.first) { valid_assembly = false; }
            obj_filenames.push_back(result.second);
	}

    }

    setup();

    uint32_t total_points_earned = 0;
    uint32_t total_possible_points = 0;

    if(valid_assembly) {
        for(TestCase const & test : tests) {
            lc3::utils::NullPrinter sim_printer;
            FileInputter sim_inputter;
            lc3::sim simulator(sim_printer, sim_inputter, 0);

            testBringup(simulator);

            verify_count = 0;
            verify_valid = 0;

            total_possible_points += test.points;

            std::cout << "Test: " << test.name;
            if(test.randomize) {
                simulator.randomize();
                std::cout << " (Randomized Machine)";
            }
            std::cout << std::endl;
            for(std::string const & obj_filename : obj_filenames) {
                if(! simulator.loadObjectFile(obj_filename)) {
                    std::cout << "could not init simulator\n";
                    return 2;
                }
            }

            try {
                test.test_func(simulator);
            } catch(lc3::utils::exception const & e) {
                continue;
            }

            testTeardown(simulator);

            float percent_points_earned = ((float) verify_valid) / verify_count;
            uint32_t points_earned = (uint32_t) ( percent_points_earned * test.points);
            std::cout << "test points earned: " << points_earned << "/" << test.points << " ("
                      << (percent_points_earned * 100) << "%)\n";
            std::cout << "==========\n";

            total_points_earned += points_earned;
        }
    }

    std::cout << "==========\n";
    float percent_points_earned = ((float) total_points_earned) / total_possible_points;
    std::cout << "total points earned: " << total_points_earned << "/" << total_possible_points << " ("
              << (percent_points_earned * 100) << "%)\n";

    return 0;
}
