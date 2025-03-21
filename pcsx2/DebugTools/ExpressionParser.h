// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"
#include <string>
#include <vector>

typedef std::pair<u64,u64> ExpressionPair;
typedef std::vector<ExpressionPair> PostfixExpression;

enum ExpressionType
{
	EXPR_TYPE_UINT = 0,
	EXPR_TYPE_FLOAT = 2,
};

class IExpressionFunctions
{
public:
	virtual bool parseReference(char* str, u64& referenceIndex) = 0;
	virtual bool parseSymbol(char* str, u64& symbolValue) = 0;
	virtual u64 getReferenceValue(u64 referenceIndex) = 0;
	virtual ExpressionType getReferenceType(u64 referenceIndex) = 0;
	virtual bool getMemoryValue(u32 address, int size, u64& dest, std::string& error) = 0;
};

bool initPostfixExpression(const char* infix, IExpressionFunctions* funcs, PostfixExpression& dest, std::string& error);
bool parsePostfixExpression(PostfixExpression& exp, IExpressionFunctions* funcs, u64& dest, std::string& error);
bool parseExpression(const char* exp, IExpressionFunctions* funcs, u64& dest, std::string& error);
