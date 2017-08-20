#ifndef STATE_H
#define STATE_H

namespace core
{
    class IEvent;

    struct MachineState
    {
        MachineState(Logger & logger) : logger(logger) {}

        std::vector<utils::Statement> mem;
        std::array<uint32_t, 8> regs;
        uint32_t pc;
        uint32_t psr;
        uint32_t backup_sp;


        Logger & logger;
        std::vector<char> console_buffer;

        bool running;
        bool hit_breakpoint;

        uint32_t readMem(uint32_t addr, bool & change_mem, IEvent *& change) const;

    private:
        bool pre_instruction_callback_v;
        bool post_instruction_callback_v;
        bool interrupt_enter_callback_v;;
        bool interrupt_exit_callback_v;;
        std::function<void(MachineState & state)> pre_instruction_callback;
        std::function<void(MachineState & state)> post_instruction_callback;
        std::function<void(MachineState & state)> interrupt_enter_callback;
        std::function<void(MachineState & state)> interrupt_exit_callback;

        friend class Simulator;
    };

    enum class EventType {
          EVENT_REG
        , EVENT_PSR
        , EVENT_PC
        , EVENT_MEM
        , EVENT_SWAP_SP
        , EVENT_CALLBACK
    };

    class IEvent
    {
    public:
        EventType type;

        IEvent(EventType type) : type(type) {}
        virtual ~IEvent(void) = default;

        virtual void updateState(MachineState & state) const = 0;
        virtual std::string getOutputString(MachineState const & state) const = 0;
    };

    class RegEvent : public IEvent
    {
    public:
        RegEvent(uint32_t reg, uint32_t value) : IEvent(EventType::EVENT_REG), reg(reg), value(value) {}

        virtual void updateState(MachineState & state) const override { state.regs[reg] = value; }
        virtual std::string getOutputString(MachineState const & state) const override {
            return utils::ssprintf("R%d: 0x%0.4x => 0x%0.4x", reg, state.regs[reg], value); }
    private:
        uint32_t reg;
        uint32_t value;
    };

    class PSREvent : public IEvent
    {
    public:
        PSREvent(uint32_t value) : IEvent(EventType::EVENT_PSR), value(value) {}

        virtual void updateState(MachineState & state) const override { state.psr = value; }
        virtual std::string getOutputString(MachineState const & state) const override {
            return utils::ssprintf("PSR: 0x%0.4x => 0x%0.4x", state.psr, value); }
    private:
        uint32_t value;
    };

    class PCEvent : public IEvent
    {
    public:
        PCEvent(uint32_t value) : IEvent(EventType::EVENT_PC), value(value) {}

        virtual void updateState(MachineState & state) const override { state.pc = value; }
        virtual std::string getOutputString(MachineState const & state) const override {
            return utils::ssprintf("PC: 0x%0.4x => 0x%0.4x", state.pc, value); }
    private:
        uint32_t value;
    };

    class MemWriteEvent : public IEvent
    {
    public:
        MemWriteEvent(uint32_t addr, uint32_t value) : IEvent(EventType::EVENT_MEM), addr(addr), value(value) {}

        virtual void updateState(MachineState & state) const override;
        virtual std::string getOutputString(MachineState const & state) const override {
            return utils::ssprintf("MEM[0x%0.4x]: 0x%0.4x => 0x%0.4x", addr, state.mem[addr].getValue(), value); }
    private:
        uint32_t addr;
        uint32_t value;
    };

    class SwapSPEvent : public IEvent
    {
    public:
        SwapSPEvent() : IEvent(EventType::EVENT_SWAP_SP) {}

        virtual void updateState(MachineState & state) const override;
        virtual std::string getOutputString(MachineState const & state) const override {
            return utils::ssprintf("R6 <=> SP : 0x%0.4x <=> 0x%0.4x", state.regs[6], state.backup_sp);
        }
    };

    class CallbackEvent : public IEvent
    {
    public:
        CallbackEvent(bool callback_v, std::function<void(MachineState & state)> callback) :
            IEvent(EventType::EVENT_CALLBACK), callback_v(callback_v), callback(callback) {}
        virtual void updateState(MachineState & state) const override { if(callback_v) { callback(state); } }
        virtual std::string getOutputString(MachineState const & state) const override { (void)state; return ""; }
    private:
        bool callback_v;
        std::function<void(MachineState & state)> callback;
    };
};

#endif
