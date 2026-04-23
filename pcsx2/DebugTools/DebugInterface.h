// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "BiosDebugData.h"
#include "ExpressionParser.h"
#include "SymbolGuardian.h"
#include "SymbolImporter.h"
#include "MipsStackWalk.h"

#include "common/MemoryInterface.h"

#include <string>

enum
{
	EECAT_GPR,
	EECAT_CP0,
	EECAT_FPR,
	EECAT_FCR,
	EECAT_VU0F,
	EECAT_VU0I,
	EECAT_GSPRIV,
	EECAT_COUNT
};
enum
{
	IOPCAT_GPR,
	IOPCAT_COUNT
};
enum BreakPointCpu
{
	BREAKPOINT_EE = 0x01,
	BREAKPOINT_IOP = 0x02,
	BREAKPOINT_IOP_AND_EE = 0x03
};

inline const std::array<BreakPointCpu, 2> DEBUG_CPUS = {
	BREAKPOINT_EE,
	BREAKPOINT_IOP,
};

class DebugInterface : public MemoryInterface
{
public:
	enum RegisterType
	{
		NORMAL,
		SPECIAL
	};

	// register stuff
	virtual int getRegisterCategoryCount() = 0;
	virtual const char* getRegisterCategoryName(int cat) = 0;
	virtual int getRegisterSize(int cat) = 0;
	virtual int getRegisterCount(int cat) = 0;
	virtual RegisterType getRegisterType(int cat) = 0;
	virtual const char* getRegisterName(int cat, int num) = 0;
	virtual u128 getRegister(int cat, int num) = 0;
	virtual std::string getRegisterString(int cat, int num) = 0;
	virtual u128 getHI() = 0;
	virtual u128 getLO() = 0;
	virtual u32 getPC() = 0;
	virtual bool getCPCOND0() = 0;
	virtual void setPc(u32 newPc) = 0;
	virtual void setRegister(int cat, int num, u128 newValue) = 0;

	virtual std::string disasm(u32 address, bool simplify) = 0;
	virtual bool isValidAddress(u32 address) = 0;
	virtual u32 getCycles() = 0;
	virtual BreakPointCpu getCpuType() = 0;
	virtual SymbolGuardian& GetSymbolGuardian() const = 0;
	virtual SymbolImporter* GetSymbolImporter() const = 0;
	virtual std::vector<std::unique_ptr<BiosThread>> GetThreadList() const = 0;
	virtual std::vector<MipsStackWalk::StackFrame> StackTrace(const BiosThread& thread) = 0;
	virtual std::vector<IopMod> GetModuleList() const = 0;

	bool isAlive();
	bool isCpuPaused();
	void pauseCpu();
	void resumeCpu();
	char* stringFromPointer(u32 p);

	std::optional<u32> getCallerStackPointer(const ccc::Function& currentFunction);
	std::optional<u32> getStackFrameSize(const ccc::Function& currentFunction);

	bool evaluateExpression(const char* expression, u64& dest, std::string& error);
	bool initExpression(const char* exp, PostfixExpression& dest, std::string& error);
	bool parseExpression(PostfixExpression& exp, u64& dest, std::string& error);

	static void setPauseOnEntry(bool pauseOnEntry) { m_pause_on_entry = pauseOnEntry; };
	static bool getPauseOnEntry() { return m_pause_on_entry; }

	static DebugInterface& get(BreakPointCpu cpu);
	static const char* cpuName(BreakPointCpu cpu);
	static const char* longCpuName(BreakPointCpu cpu);

private:
	static bool m_pause_on_entry;
};

class R5900DebugInterface : public DebugInterface
{
public:
	u8 Read8(u32 address, bool* valid = nullptr) override;
	u16 Read16(u32 address, bool* valid = nullptr) override;
	u32 Read32(u32 address, bool* valid = nullptr) override;
	u64 Read64(u32 address, bool* valid = nullptr) override;
	u128 Read128(u32 address, bool* valid = nullptr) override;
	bool ReadBytes(u32 address, void* dest, u32 size) override;

	bool Write8(u32 address, u8 value) override;
	bool Write16(u32 address, u16 value) override;
	bool Write32(u32 address, u32 value) override;
	bool Write64(u32 address, u64 value) override;
	bool Write128(u32 address, u128 value) override;
	bool WriteBytes(u32 address, void* src, u32 size) override;

	bool CompareBytes(u32 address, void* src, u32 size) override;

	// register stuff
	int getRegisterCategoryCount() override;
	const char* getRegisterCategoryName(int cat) override;
	int getRegisterSize(int cat) override;
	int getRegisterCount(int cat) override;
	RegisterType getRegisterType(int cat) override;
	const char* getRegisterName(int cat, int num) override;
	u128 getRegister(int cat, int num) override;
	std::string getRegisterString(int cat, int num) override;
	u128 getHI() override;
	u128 getLO() override;
	u32 getPC() override;
	bool getCPCOND0() override;
	void setPc(u32 newPc) override;
	void setRegister(int cat, int num, u128 newValue) override;
	SymbolGuardian& GetSymbolGuardian() const override;
	SymbolImporter* GetSymbolImporter() const override;
	std::vector<std::unique_ptr<BiosThread>> GetThreadList() const override;
	std::vector<MipsStackWalk::StackFrame> StackTrace(const BiosThread& thread) override;
	std::vector<IopMod> GetModuleList() const override;

	std::string disasm(u32 address, bool simplify) override;
	bool isValidAddress(u32 address) override;
	u32 getCycles() override;
	BreakPointCpu getCpuType() override;
};

class R3000DebugInterface : public DebugInterface
{
public:
	u8 Read8(u32 address, bool* valid = nullptr) override;
	u16 Read16(u32 address, bool* valid = nullptr) override;
	u32 Read32(u32 address, bool* valid = nullptr) override;
	u64 Read64(u32 address, bool* valid = nullptr) override;
	u128 Read128(u32 address, bool* valid = nullptr) override;
	bool ReadBytes(u32 address, void* dest, u32 size) override;

	bool Write8(u32 address, u8 value) override;
	bool Write16(u32 address, u16 value) override;
	bool Write32(u32 address, u32 value) override;
	bool Write64(u32 address, u64 value) override;
	bool Write128(u32 address, u128 value) override;
	bool WriteBytes(u32 address, void* src, u32 size) override;

	bool CompareBytes(u32 address, void* src, u32 size) override;

	// register stuff
	int getRegisterCategoryCount() override;
	const char* getRegisterCategoryName(int cat) override;
	int getRegisterSize(int cat) override;
	int getRegisterCount(int cat) override;
	RegisterType getRegisterType(int cat) override;
	const char* getRegisterName(int cat, int num) override;
	u128 getRegister(int cat, int num) override;
	std::string getRegisterString(int cat, int num) override;
	u128 getHI() override;
	u128 getLO() override;
	u32 getPC() override;
	bool getCPCOND0() override;
	void setPc(u32 newPc) override;
	void setRegister(int cat, int num, u128 newValue) override;
	SymbolGuardian& GetSymbolGuardian() const override;
	SymbolImporter* GetSymbolImporter() const override;
	std::vector<std::unique_ptr<BiosThread>> GetThreadList() const override;
	std::vector<MipsStackWalk::StackFrame> StackTrace(const BiosThread& thread) override;
	std::vector<IopMod> GetModuleList() const override;

	std::string disasm(u32 address, bool simplify) override;
	bool isValidAddress(u32 address) override;
	u32 getCycles() override;
	BreakPointCpu getCpuType() override;
};

// Provides access to the loadable segments from the ELF as they are on disk.
class ElfMemoryReader : public MemoryInterface
{
public:
	ElfMemoryReader(const ccc::ElfFile& elf);

	u8 Read8(u32 address, bool* valid = nullptr) override;
	u16 Read16(u32 address, bool* valid = nullptr) override;
	u32 Read32(u32 address, bool* valid = nullptr) override;
	u64 Read64(u32 address, bool* valid = nullptr) override;
	u128 Read128(u32 address, bool* valid = nullptr) override;
	bool ReadBytes(u32 address, void* dest, u32 size) override;

	bool Write8(u32 address, u8 value) override;
	bool Write16(u32 address, u16 value) override;
	bool Write32(u32 address, u32 value) override;
	bool Write64(u32 address, u64 value) override;
	bool Write128(u32 address, u128 value) override;
	bool WriteBytes(u32 address, void* src, u32 size) override;

	bool CompareBytes(u32 address, void* src, u32 size) override;

protected:
	const ccc::ElfFile& m_elf;
};

class MipsExpressionFunctions : public IExpressionFunctions
{
public:
	MipsExpressionFunctions(
		DebugInterface* cpu, const ccc::SymbolDatabase* symbolDatabase, bool shouldEnumerateSymbols);

	bool parseReference(char* str, u64& referenceIndex) override;
	bool parseSymbol(char* str, u64& symbolValue) override;
	u64 getReferenceValue(u64 referenceIndex) override;
	ExpressionType getReferenceType(u64 referenceIndex) override;
	bool getMemoryValue(u32 address, int size, u64& dest, std::string& error) override;

protected:
	void enumerateSymbols(const ccc::SymbolDatabase& database);
	bool parseSymbol(char* str, u64& symbolValue, const ccc::SymbolDatabase& database);
	DebugInterface* m_cpu;
	const ccc::SymbolDatabase* m_database;
	std::map<std::string, ccc::FunctionHandle> m_mangled_function_names_to_handles;
	std::map<std::string, ccc::GlobalVariableHandle> m_mangled_global_names_to_handles;
};

extern R5900DebugInterface r5900Debug;
extern R3000DebugInterface r3000Debug;
