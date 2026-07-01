#pragma once

#include <Debugger/DebugCommands/IDebugCommand.h>

class DebugCommandStack
{
public:
	// Address is a function base address
	template <class Command, class...Args>
	void Push(ccc::Address address, Args&&... args)
	{
		Stack& addressStack = m_commands[address];

		// erase commands to keep history consistent
		if (addressStack.index != addressStack.size())
		{
			addressStack.ClearBranch();
			EnableHistory(address);
		}

		if (addressStack.index == -1)
			addressStack.index = 0;

		std::unique_ptr<IDebugActionCommand> command = std::make_unique<Command>(std::forward<Args>(args)...);

		addressStack.commands.emplace_back(std::move(command));
	}

	void Do(DebugInterface& cpu, ccc::Address address);
	void Undo(DebugInterface& cpu, ccc::Address address);

	bool CanDo(DebugInterface& cpu, ccc::Address address);
	bool CanUndo(DebugInterface& cpu, ccc::Address address);

	std::string GetUndoName(ccc::Address address);
	std::string GetRedoName(ccc::Address address);

	bool MoveCurrent(ccc::Address origin, ccc::Address destination);

	void DisableHistory(ccc::Address source);
	void EnableHistory(ccc::Address source);

	std::optional<std::string> GetErrorMessage(ccc::Address address);
	void SetError(ccc::Address address, std::string message);

protected:
	ccc::Address GetAddress(ccc::Address);

	struct Stack
	{
		bool enabled = true;
		std::vector<std::unique_ptr<IDebugActionCommand>> commands;
		size_t index = -1;

		std::optional<std::string> error = std::nullopt;

		auto size() { return commands.size(); }

		void ResetHistory() noexcept;
		void ClearBranch() noexcept;

		bool DoCurrent(CommandContext& ctx);
		bool UndoCurrent(CommandContext& ctx);

		bool InDoRange() const;
		bool InUndoRange() const;
		bool InAccessRange() const;
		bool NextInAccessRange() const;

		void Down()
		{
			if (index == -1)
				return;

			index--;
		}

		void Up()
		{
			if (index == commands.size())
				return;

			index++;
		}
	};


	std::map<ccc::Address, Stack> m_commands; // key is function base address
	std::map<ccc::Address, ccc::Address> m_redirections; // to ; from (disabled rn)
};
