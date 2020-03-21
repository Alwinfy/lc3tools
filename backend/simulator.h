/*
 * Copyright 2020 McGraw-Hill Education. All rights reserved. No reproduction or distribution without the prior written consent of McGraw-Hill Education.
 */
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <cstdint>
#include <unordered_map>
#include <queue>
#include <set>

#include "inputter.h"
#include "event.h"
#include "logger.h"
#include "printer.h"
#include "state.h"

namespace std
{
    using namespace lc3::core;

    template<> struct greater<PIEvent>
    {
        bool operator()(PIEvent lhs, PIEvent rhs)
        {
            return std::greater<uint64_t>()(lhs->time, rhs->time);
        }
    };
};

namespace lc3
{
namespace core
{
    class Simulator
    {
    public:
        using Callback = std::function<void(CallbackType, MachineState &)>;

        Simulator(lc3::utils::IPrinter & printer, lc3::utils::IInputter & inputter, uint32_t print_level);
        void simulate(void);
        void loadObj(std::string const & name, std::istream & buffer);
        void startup(uint64_t t_delta = 0);
        void resume(uint64_t t_delta = 0);
        void triggerSuspend(uint64_t t_delta = 0);
        void registerCallback(CallbackType type, Callback func);
        void addBreakpoint(uint16_t pc);
        void removeBreakpoint(uint16_t pc);
        MachineState & getMachineState(void);
        MachineState const & getMachineState(void) const;

        void setPrintLevel(uint32_t print_level);

    private:
        std::priority_queue<PIEvent, std::vector<PIEvent>, std::greater<PIEvent>> events;
        uint64_t time;

        MachineState state;
        std::vector<PIDevice> devices;

        lc3::utils::Logger logger;
        lc3::utils::IInputter & inputter;

        std::unordered_map<CallbackType, Callback> callbacks;
        std::set<uint16_t> breakpoints;

        void executeEvents(void);
        void handleDevices(void);
        void handleInstruction(sim::Decoder & decoder);
        void handleCallbacks(void);

        uint16_t pre_inst_pc;
        std::vector<uint16_t> stack_trace;
        static void callbackDispatcher(Simulator * sim, CallbackType type, MachineState & state);
    };
};
};

#endif

