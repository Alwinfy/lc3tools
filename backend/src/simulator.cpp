#include <fstream>
#include <mutex>
#include <thread>

#include "device_regs.h"
#include "simulator.h"
#include "utils.h"

using namespace lc3::core;

namespace lc3
{
namespace core
{
    std::mutex g_io_lock;
};
};

Simulator::Simulator(lc3::sim & simulator, lc3::utils::IPrinter & printer, lc3::utils::IInputter & inputter,
    uint32_t log_level) : state(simulator, logger), logger(printer, log_level), inputter(inputter),
    collecting_input(false)
{
    state.mem.resize(1 << 16);
    reset();

    state.pre_instruction_callback_v = false;
    state.post_instruction_callback_v = false;
    state.interrupt_enter_callback_v = false;
    state.interrupt_exit_callback_v = false;
}

void Simulator::loadObjectFile(std::string const & filename)
{
    std::ifstream file(filename);
    if(! file) {
        logger.printf(lc3::utils::PrintType::P_ERROR, true, "could not open file \'%s\' for reading",
            filename.c_str());
        throw utils::exception("could not open file");
    }

    uint32_t fill_pc = 0;
    uint32_t offset = 0;
    bool first_orig_set = false;
    while(! file.eof()) {
        MemEntry statement;
        file >> statement;

        if(file.eof()) {
            break;
        }

        if(statement.isOrig()) {
            if(! first_orig_set) {
                state.pc = statement.getValue();
                first_orig_set = true;
            }
            fill_pc = statement.getValue();
            offset = 0;
        } else {
            logger.printf(lc3::utils::PrintType::P_DEBUG, true, "0x%0.4x: %s (0x%0.4x)", fill_pc + offset,
                statement.getLine().c_str(), statement.getValue());
            state.mem[fill_pc + offset] = statement;
            offset += 1;
        }

    }
    uint16_t value = state.mem[MCR].getValue();
    state.mem[MCR].setValue(value | 0x8000);
}

void Simulator::loadOS(std::string const & os_path)
{
    loadObjectFile(os_path);
    state.pc = RESET_PC;
}

void Simulator::simulate(void)
{
    std::thread input_thread;
    utils::exception exception;
    bool exception_valid = false;

    try {
        state.running = true;
        collecting_input = true;
        inputter.beginInput();

        input_thread = std::thread(&core::Simulator::handleInput, this);
        while(state.running && (state.mem[MCR].getValue() & 0x8000) != 0) {
            if(! state.hit_breakpoint) {
                executeEvent(std::make_shared<CallbackEvent>(state.pre_instruction_callback_v,
                    state.pre_instruction_callback));
                if(state.hit_breakpoint) {
                    break;
                }
            }
            state.hit_breakpoint = false;

            std::vector<PIEvent> events = executeInstruction();
            events.push_back(std::make_shared<CallbackEvent>(state.post_instruction_callback_v,
                state.post_instruction_callback));
            executeEventChain(events);
            updateDevices();
            events = checkAndSetupInterrupts();
            executeEventChain(events);
        }
    } catch(utils::exception & e) {
        exception = e;
        exception_valid = true;
    } catch(std::exception & e) { }

    state.running = false;
    collecting_input = false;
    if(input_thread.joinable()) {
        input_thread.join();
    }
    inputter.endInput();

    if(exception_valid) {
        throw exception;
    }
}

void Simulator::pause(void)
{
    state.running = false;
}

std::vector<PIEvent> Simulator::executeInstruction(void)
{
    uint32_t encoded_inst = state.mem[state.pc].getValue();

    bool valid;
    PIInstruction candidate = decoder.findInstructionByEncoding(encoded_inst, valid);
    if(! valid) {
        logger.printf(lc3::utils::PrintType::P_ERROR, true, "invalid instruction 0x%0.4x", encoded_inst);
        throw utils::exception("invalid instruction");
    }

    candidate->assignOperands(encoded_inst);
    logger.printf(lc3::utils::PrintType::P_EXTRA, true, "executing PC 0x%0.4x: %s (0x%0.4x)", state.pc,
        state.mem[state.pc].getLine().c_str(), encoded_inst);
    state.pc = (state.pc + 1) & 0xffff;
    std::vector<PIEvent> events = candidate->execute(state);

    return events;
}

void Simulator::updateDevices(void)
{
    uint16_t value = state.mem[DSR].getValue();
    state.mem[DSR].setValue(value | 0x8000);
}

std::vector<PIEvent> Simulator::checkAndSetupInterrupts(void)
{
    std::vector<PIEvent> ret;

    uint32_t value = state.mem[KBSR].getValue();
    if((value & 0xc000) == 0xc000) {
        ret.push_back(std::make_shared<SwapSPEvent>(MachineState::SPType::SSP));
        ret.push_back(std::make_shared<RegEvent>(6, state.regs[6] - 1));
        ret.push_back(std::make_shared<MemWriteEvent>(state.regs[6] - 1, state.mem[PSR].getValue()));
        ret.push_back(std::make_shared<RegEvent>(6, state.regs[6] - 2));
        ret.push_back(std::make_shared<MemWriteEvent>(state.regs[6] - 2, state.pc));
        ret.push_back(std::make_shared<PSREvent>((state.mem[PSR].getValue() & 0x78ff) | 0x0400));
        ret.push_back(std::make_shared<PCEvent>(state.mem[0x0180].getValue()));
        ret.push_back(std::make_shared<MemWriteEvent>(KBSR, state.mem[KBSR].getValue() & 0x7fff));
        ret.push_back(std::make_shared<CallbackEvent>(state.interrupt_enter_callback_v,
            state.interrupt_enter_callback));
        ret.push_back(std::make_shared<PushSysCallTypeEvent>(MachineState::SysCallType::INT));
    }

    return ret;
}

void Simulator::executeEventChain(std::vector<PIEvent> & events)
{
    for(PIEvent event : events) {
        executeEvent(event);
    }

    events.clear();
}

void Simulator::executeEvent(PIEvent event)
{
    if(event->getOutputString(state) != "") {
        logger.printf(lc3::utils::PrintType::P_EXTRA, false, "  %s", event->getOutputString(state).c_str());
    }
    event->updateState(state);
}

void Simulator::reset(void)
{
    for(uint32_t i = 0; i < 8; i += 1) {
        state.regs[i] = 0;
    }

    state.pc = RESET_PC;
    state.sp_type_in_use = MachineState::SPType::USP;
    state.backup_sp = 0x3000;

    for(uint32_t i = 0; i < (1 << 16); i += 1) {
        state.mem[i].setValue(0);
    }

    state.mem[PSR].setValue(0x8002);
    state.mem[MCR].setValue(0x8000);  // indicate the machine is running
    state.running = true;
    state.hit_breakpoint = false;
}

void Simulator::registerPreInstructionCallback(callback_func_t func)
{
    state.pre_instruction_callback_v = true;
    state.pre_instruction_callback = func;
}

void Simulator::registerPostInstructionCallback(callback_func_t func)
{
    state.post_instruction_callback_v = true;
    state.post_instruction_callback = func;
}

void Simulator::registerInterruptEnterCallback(callback_func_t func)
{
    state.interrupt_enter_callback_v = true;
    state.interrupt_enter_callback = func;
}

void Simulator::registerInterruptExitCallback(callback_func_t func)
{
    state.interrupt_exit_callback_v = true;
    state.interrupt_exit_callback = func;
}

void Simulator::registerSubEnterCallback(callback_func_t func)
{
    state.sub_enter_callback_v = true;
    state.sub_enter_callback = func;
}

void Simulator::registerSubExitCallback(callback_func_t func)
{
    state.sub_exit_callback_v = true;
    state.sub_exit_callback = func;
}

void Simulator::handleInput(void)
{
    while(collecting_input) {
        char c;
        if(inputter.getChar(c)) {
            std::lock_guard<std::mutex> guard(g_io_lock);
            state.mem[KBSR].setValue(state.mem[KBSR].getValue() | 0x8000);
            state.mem[KBDR].setValue(((uint32_t) c) & 0xFF);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
