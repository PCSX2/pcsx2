// Copyright 2023, VIXL authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef VIXL_INCLUDE_SIMULATOR_AARCH64

#include "debugger-aarch64.h"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <errno.h>
#include <limits>

namespace vixl {
namespace aarch64 {


Debugger::Debugger(Simulator* sim)
    : sim_(sim), input_stream_(&std::cin), ostream_(sim->GetOutputStream()) {
  // Register all basic debugger commands.
  RegisterCmd<HelpCmd>();
  RegisterCmd<BreakCmd>();
  RegisterCmd<StepCmd>();
  RegisterCmd<ContinueCmd>();
  RegisterCmd<PrintCmd>();
  RegisterCmd<TraceCmd>();
  RegisterCmd<GdbCmd>();
}


template <class T>
void Debugger::RegisterCmd() {
  auto new_command = std::make_unique<T>(sim_);

  // Check that the new command word and alias, don't already exist.
  std::string_view new_cmd_word = new_command->GetCommandWord();
  std::string_view new_cmd_alias = new_command->GetCommandAlias();
  for (const auto& cmd : debugger_cmds_) {
    std::string_view cmd_word = cmd->GetCommandWord();
    std::string_view cmd_alias = cmd->GetCommandAlias();

    if (new_cmd_word == cmd_word) {
      VIXL_ABORT_WITH_MSG("Command word matches an existing command word.");
    } else if (new_cmd_word == cmd_alias) {
      VIXL_ABORT_WITH_MSG("Command word matches an existing command alias.");
    }

    if (new_cmd_alias != "") {
      if (new_cmd_alias == cmd_word) {
        VIXL_ABORT_WITH_MSG("Command alias matches an existing command word.");
      } else if (new_cmd_alias == cmd_alias) {
        VIXL_ABORT_WITH_MSG("Command alias matches an existing command alias.");
      }
    }
  }

  debugger_cmds_.push_back(std::move(new_command));
}


bool Debugger::IsAtBreakpoint() const {
  return IsBreakpoint(reinterpret_cast<uint64_t>(sim_->ReadPc()));
}


void Debugger::Debug() {
  DebugReturn done = DebugContinue;
  while (done == DebugContinue) {
    // Disassemble the next instruction to execute.
    PrintDisassembler print_disasm = PrintDisassembler(ostream_);
    print_disasm.Disassemble(sim_->ReadPc());

    // Read the command line.
    fprintf(ostream_, "sim> ");
    std::string line;
    std::getline(*input_stream_, line);

    // Remove all control characters from the command string.
    line.erase(std::remove_if(line.begin(),
                              line.end(),
                              [](char c) { return std::iscntrl(c); }),
               line.end());

    // Assume input from std::cin has already been output (e.g: by a terminal)
    // but input from elsewhere (e.g: from a testing input stream) has not.
    if (input_stream_ != &std::cin) {
      fprintf(ostream_, "%s\n", line.c_str());
    }

    // Parse the command into tokens.
    std::vector<std::string> tokenized_cmd = Tokenize(line);
    if (!tokenized_cmd.empty()) {
      done = ExecDebugCommand(tokenized_cmd);
    }
  }
}


std::optional<uint64_t> Debugger::ParseUint64String(std::string_view uint64_str,
                                                    int base) {
  // Clear any previous errors.
  errno = 0;

  // strtoull uses 0 to indicate that no conversion was possible so first
  // check that the string isn't zero.
  if (IsZeroUint64String(uint64_str, base)) {
    return 0;
  }

  // Cannot use stoi as it might not be possible to use exceptions.
  char* end;
  uint64_t value = std::strtoull(uint64_str.data(), &end, base);
  if (value == 0 || *end != '\0' || errno == ERANGE) {
    return std::nullopt;
  }

  return value;
}


std::optional<Debugger::RegisterParsedFormat> Debugger::ParseRegString(
    std::string_view reg_str) {
  // A register should only have 2 (e.g: X0) or 3 (e.g: X31) characters.
  if (reg_str.size() < 2 || reg_str.size() > 3) {
    return std::nullopt;
  }

  // Check for aliases of registers.
  if (reg_str == "lr") {
    return {{'X', kLinkRegCode}};
  } else if (reg_str == "sp") {
    return {{'X', kSpRegCode}};
  }

  unsigned max_reg_num;
  char reg_prefix = std::toupper(reg_str.front());
  switch (reg_prefix) {
    case 'W':
      VIXL_FALLTHROUGH();
    case 'X':
      max_reg_num = kNumberOfRegisters - 1;
      break;
    case 'V':
      max_reg_num = kNumberOfVRegisters - 1;
      break;
    case 'Z':
      max_reg_num = kNumberOfZRegisters - 1;
      break;
    case 'P':
      max_reg_num = kNumberOfPRegisters - 1;
      break;
    default:
      return std::nullopt;
  }

  std::string_view str_code = reg_str.substr(1, reg_str.size());
  auto reg_code = ParseUint64String(str_code, 10);
  if (!reg_code) {
    return std::nullopt;
  }

  if (*reg_code > max_reg_num) {
    return std::nullopt;
  }

  return {{reg_prefix, static_cast<unsigned int>(*reg_code)}};
}


void Debugger::PrintUsage() {
  for (const auto& cmd : debugger_cmds_) {
    // Print commands in the following format:
    //  foo / f
    //      foo <arg>
    //      A description of the foo command.
    //

    std::string_view cmd_word = cmd->GetCommandWord();
    std::string_view cmd_alias = cmd->GetCommandAlias();
    if (cmd_alias != "") {
      fprintf(ostream_, "%s / %s\n", cmd_word.data(), cmd_alias.data());
    } else {
      fprintf(ostream_, "%s\n", cmd_word.data());
    }

    std::string_view args_str = cmd->GetArgsString();
    if (args_str != "") {
      fprintf(ostream_, "\t%s %s\n", cmd_word.data(), args_str.data());
    }

    std::string_view description = cmd->GetDescription();
    if (description != "") {
      fprintf(ostream_, "\t%s\n", description.data());
    }
  }
}


std::vector<std::string> Debugger::Tokenize(std::string_view input_line,
                                            char separator) {
  std::vector<std::string> words;

  if (input_line.empty()) {
    return words;
  }

  for (auto separator_pos = input_line.find(separator);
       separator_pos != input_line.npos;
       separator_pos = input_line.find(separator)) {
    // Skip consecutive, repeated separators.
    if (separator_pos != 0) {
      words.push_back(std::string{input_line.substr(0, separator_pos)});
    }

    // Remove characters up to and including the separator.
    input_line.remove_prefix(separator_pos + 1);
  }

  // Add the rest of the string to the vector.
  words.push_back(std::string{input_line});

  return words;
}


DebugReturn Debugger::ExecDebugCommand(
    const std::vector<std::string>& tokenized_cmd) {
  std::string cmd_word = tokenized_cmd.front();
  for (const auto& cmd : debugger_cmds_) {
    if (cmd_word == cmd->GetCommandWord() ||
        cmd_word == cmd->GetCommandAlias()) {
      const std::vector<std::string> args(tokenized_cmd.begin() + 1,
                                          tokenized_cmd.end());

      // Call the handler for the command and pass the arguments.
      return cmd->Action(args);
    }
  }

  fprintf(ostream_, "Error: command '%s' not found\n", cmd_word.c_str());
  return DebugContinue;
}


bool Debugger::IsZeroUint64String(std::string_view uint64_str, int base) {
  // Remove any hex prefixes.
  if (base == 0 || base == 16) {
    std::string_view prefix = uint64_str.substr(0, 2);
    if (prefix == "0x" || prefix == "0X") {
      uint64_str.remove_prefix(2);
    }
  }

  if (uint64_str.empty()) {
    return false;
  }

  // Check all remaining digits in the string for anything other than zero.
  for (char c : uint64_str) {
    if (c != '0') {
      return false;
    }
  }

  return true;
}


DebuggerCmd::DebuggerCmd(Simulator* sim,
                         std::string cmd_word,
                         std::string cmd_alias,
                         std::string args_str,
                         std::string description)
    : sim_(sim),
      ostream_(sim->GetOutputStream()),
      command_word_(cmd_word),
      command_alias_(cmd_alias),
      args_str_(args_str),
      description_(description) {}


DebugReturn HelpCmd::Action(const std::vector<std::string>& args) {
  USE(args);
  sim_->GetDebugger()->PrintUsage();
  return DebugContinue;
}


DebugReturn BreakCmd::Action(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    fprintf(ostream_, "Error: Use `break <address>` to set a breakpoint\n");
    return DebugContinue;
  }

  std::string arg = args.front();
  auto break_addr = Debugger::ParseUint64String(arg);
  if (!break_addr) {
    fprintf(ostream_, "Error: Use `break <address>` to set a breakpoint\n");
    return DebugContinue;
  }

  if (sim_->GetDebugger()->IsBreakpoint(*break_addr)) {
    sim_->GetDebugger()->RemoveBreakpoint(*break_addr);
    fprintf(ostream_,
            "Breakpoint successfully removed at: 0x%" PRIx64 "\n",
            *break_addr);
  } else {
    sim_->GetDebugger()->RegisterBreakpoint(*break_addr);
    fprintf(ostream_,
            "Breakpoint successfully added at: 0x%" PRIx64 "\n",
            *break_addr);
  }

  return DebugContinue;
}


DebugReturn StepCmd::Action(const std::vector<std::string>& args) {
  if (args.size() > 1) {
    fprintf(ostream_,
            "Error: use `step [number]` to step an optional number of"
            " instructions\n");
    return DebugContinue;
  }

  // Step 1 instruction by default.
  std::optional<uint64_t> number_of_instructions_to_execute{1};

  if (args.size() == 1) {
    // Parse the argument to step that number of instructions.
    std::string arg = args.front();
    number_of_instructions_to_execute = Debugger::ParseUint64String(arg);
    if (!number_of_instructions_to_execute) {
      fprintf(ostream_,
              "Error: use `step [number]` to step an optional number of"
              " instructions\n");
      return DebugContinue;
    }
  }

  while (!sim_->IsSimulationFinished() &&
         *number_of_instructions_to_execute > 0) {
    sim_->ExecuteInstruction();
    (*number_of_instructions_to_execute)--;

    // The first instruction has already been printed by Debug() so only
    // enable instruction tracing after the first instruction has been
    // executed.
    sim_->SetTraceParameters(sim_->GetTraceParameters() | LOG_DISASM);
  }

  // Disable instruction tracing after all instructions have been executed.
  sim_->SetTraceParameters(sim_->GetTraceParameters() & ~LOG_DISASM);

  if (sim_->IsSimulationFinished()) {
    fprintf(ostream_,
            "Debugger at the end of simulation, leaving simulator...\n");
    return DebugExit;
  }

  return DebugContinue;
}


DebugReturn ContinueCmd::Action(const std::vector<std::string>& args) {
  USE(args);

  fprintf(ostream_, "Continuing...\n");

  if (sim_->GetDebugger()->IsAtBreakpoint()) {
    // This breakpoint has already been hit, so execute it before continuing.
    sim_->ExecuteInstruction();
  }

  return DebugExit;
}


DebugReturn PrintCmd::Action(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    fprintf(ostream_,
            "Error: use `print <register|all>` to print the contents of a"
            " specific register or all registers.\n");
    return DebugContinue;
  }

  if (args.front() == "all") {
    sim_->PrintRegisters();
    sim_->PrintZRegisters();
  } else if (args.front() == "system") {
    sim_->PrintSystemRegisters();
  } else if (args.front() == "ffr") {
    sim_->PrintFFR();
  } else {
    auto reg = Debugger::ParseRegString(args.front());
    if (!reg) {
      fprintf(ostream_,
              "Error: incorrect register format, use e.g: X0, x0, etc...\n");
      return DebugContinue;
    }

    // Ensure the stack pointer is printed instead of the zero register.
    if ((*reg).second == kSpRegCode) {
      (*reg).second = kSPRegInternalCode;
    }

    // Registers are printed in different ways depending on their type.
    switch ((*reg).first) {
      case 'W':
        sim_->PrintRegister(
            (*reg).second,
            static_cast<Simulator::PrintRegisterFormat>(
                Simulator::PrintRegisterFormat::kPrintWReg |
                Simulator::PrintRegisterFormat::kPrintRegPartial));
        break;
      case 'X':
        sim_->PrintRegister((*reg).second,
                            Simulator::PrintRegisterFormat::kPrintXReg);
        break;
      case 'V':
        sim_->PrintVRegister((*reg).second);
        break;
      case 'Z':
        sim_->PrintZRegister((*reg).second);
        break;
      case 'P':
        sim_->PrintPRegister((*reg).second);
        break;
      default:
        // ParseRegString should only allow valid register characters.
        VIXL_UNREACHABLE();
    }
  }

  return DebugContinue;
}


DebugReturn TraceCmd::Action(const std::vector<std::string>& args) {
  if (args.size() != 0) {
    fprintf(ostream_, "Error: use `trace` to toggle tracing of registers.\n");
    return DebugContinue;
  }

  int trace_params = sim_->GetTraceParameters();
  if ((trace_params & LOG_ALL) != LOG_ALL) {
    fprintf(ostream_,
            "Enabling disassembly, registers and memory write tracing\n");
    sim_->SetTraceParameters(trace_params | LOG_ALL);
  } else {
    fprintf(ostream_,
            "Disabling disassembly, registers and memory write tracing\n");
    sim_->SetTraceParameters(trace_params & ~LOG_ALL);
  }

  return DebugContinue;
}


DebugReturn GdbCmd::Action(const std::vector<std::string>& args) {
  if (args.size() != 0) {
    fprintf(ostream_,
            "Error: use `gdb` to enter GDB from the simulator debugger.\n");
    return DebugContinue;
  }

  HostBreakpoint();
  return DebugContinue;
}


}  // namespace aarch64
}  // namespace vixl

#endif  // VIXL_INCLUDE_SIMULATOR_AARCH64
