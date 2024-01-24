#pragma once
#include <functional>

#include "Runtime/Core/BaseTypes.h"
#include "Style.h"
#include "Widgets.h"

namespace UI {

	void	Init(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);

	// For now we do all ui here
	bool	Tick();

	// Returns top level root widget containter
	Widget* GetRoot();

	Theme*	GetDefaultTheme();

	void	Shutdown();
}
