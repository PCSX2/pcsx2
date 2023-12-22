// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "DebugTools/BiosDebugData.h"
#include "MemoryTypes.h"
#include "ExpressionParser.h"
#include "SymbolMap.h"

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

class DebugInterface
{
public:
	enum RegisterType
	{
		NORMAL,
		SPECIAL
	};

	virtual u32 read8(u32 address) = 0;
	virtual u32 read8(u32 address, bool& valid) = 0;
	virtual u32 read16(u32 address) = 0;
	virtual u32 read16(u32 address, bool& valid) = 0;
	virtual u32 read32(u32 address) = 0;
	virtual u32 read32(u32 address, bool& valid) = 0;
	virtual u64 read64(u32 address) = 0;
	virtual u64 read64(u32 address, bool& valid) = 0;
	virtual u128 read128(u32 address) = 0;
	virtual void write8(u32 address, u8 value) = 0;
	virtual void write32(u32 address, u32 value) = 0;

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
	[[nodiscard]] virtual SymbolMap& GetSymbolMap() const = 0;
	[[nodiscard]] virtual std::vector<std::unique_ptr<BiosThread>> GetThreadList() const = 0;

	bool initExpression(const char* exp, PostfixExpression& dest);
	bool parseExpression(PostfixExpression& exp, u64& dest);
	bool isAlive();
	bool isCpuPaused();
	void pauseCpu();
	void resumeCpu();
	char* stringFromPointer(u32 p);

	static void setPauseOnEntry(bool pauseOnEntry) { m_pause_on_entry = pauseOnEntry; };
	static bool getPauseOnEntry() { return m_pause_on_entry; }

private:
	static bool m_pause_on_entry;
};

class R5900DebugInterface : public DebugInterface
{
public:
	u32 read8(u32 address) override;
	u32 read8(u32 address, bool& valid) override;
	u32 read16(u32 address) override;
	u32 read16(u32 address, bool& valid) override;
	u32 read32(u32 address) override;
	u32 read32(u32 address, bool& valid) override;
	u64 read64(u32 address) override;
	u64 read64(u32 address, bool& valid) override;
	u128 read128(u32 address) override;
	void write8(u32 address, u8 value) override;
	void write32(u32 address, u32 value) override;

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
	[[nodiscard]] SymbolMap& GetSymbolMap() const override;
	[[nodiscard]] std::vector<std::unique_ptr<BiosThread>> GetThreadList() const override;

	std::string disasm(u32 address, bool simplify) override;
	bool isValidAddress(u32 address) override;
	u32 getCycles() override;
	BreakPointCpu getCpuType() override;
};


class R3000DebugInterface : public DebugInterface
{
public:
	u32 read8(u32 address) override;
	u32 read8(u32 address, bool& valid) override;
	u32 read16(u32 address) override;
	u32 read16(u32 address, bool& valid) override;
	u32 read32(u32 address) override;
	u32 read32(u32 address, bool& valid) override;
	u64 read64(u32 address) override;
	u64 read64(u32 address, bool& valid) override;
	u128 read128(u32 address) override;
	void write8(u32 address, u8 value) override;
	void write32(u32 address, u32 value) override;

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
	[[nodiscard]] SymbolMap& GetSymbolMap() const override;
	[[nodiscard]] std::vector<std::unique_ptr<BiosThread>> GetThreadList() const override;

	std::string disasm(u32 address, bool simplify) override;
	bool isValidAddress(u32 address) override;
	u32 getCycles() override;
	BreakPointCpu getCpuType() override;
};

extern R5900DebugInterface r5900Debug;
extern R3000DebugInterface r3000Debug;
