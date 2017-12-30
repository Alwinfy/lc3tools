#ifndef SIM_H
#define SIM_H

#include <atomic>
#include <functional>

#include "inputter.h"
#include "instruction_decoder.h"
#include "logger.h"
#include "printer.h"
#include "state.h"

namespace lc3
{
    class sim;

    namespace core
    {
        class Simulator
        {
        public:
            Simulator(sim & simulator, lc3::utils::IPrinter & printer, utils::IInputter & inputter,
                uint32_t print_level);
            ~Simulator(void) = default;

            void loadObjectFile(std::string const & obj_file);
            void loadOS(void);
            void simulate(void);
            void reset(void);

            void registerPreInstructionCallback(callback_func_t func);
            void registerPostInstructionCallback(callback_func_t func);
            void registerInterruptEnterCallback(callback_func_t func);
            void registerInterruptExitCallback(callback_func_t func);
            void registerSubEnterCallback(callback_func_t func);
            void registerSubExitCallback(callback_func_t func);

            MachineState & getMachineState(void) { return state; }
            MachineState const & getMachineState(void) const { return state; }
        private:
            InstructionDecoder decoder;

            MachineState state;

            lc3::utils::Logger logger;
            utils::IInputter & inputter;

            std::atomic<bool> collecting_input;

            std::vector<core::IEvent const *> executeInstruction(void);
            std::vector<core::IEvent const *> checkAndSetupInterrupts();
            void executeEventChain(std::vector<core::IEvent const *> & events);
            void executeEvent(core::IEvent const & event);
            void updateDevices(void);
            void handleInput(void);
        };
    };
};

#endif
