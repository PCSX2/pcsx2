#pragma once
#include <windows.h>
#include <setupapi.h>
#include "hidapi.h"

namespace shared{ namespace rawinput{

	class ParseRawInputCB
	{
		public:
		virtual void ParseRawInput(PRAWINPUT pRawInput) = 0;
	};

	int Initialize(void *hWnd);
	void Uninitialize();

	void RegisterCallback(ParseRawInputCB *cb);
	void UnregisterCallback(ParseRawInputCB *cb);
}}