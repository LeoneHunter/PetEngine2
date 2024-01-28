#pragma once
#include <functional>

#include "Runtime/Core/BaseTypes.h"
#include "Style.h"
#include "Widgets.h"

namespace UI {

	class Application {
	public:

		static Application* Create(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);
		static Application* Get();

		~Application() = default;

		// For now we do all ui here
		virtual bool	Tick() = 0;
		// Returns top level root widget containter
		virtual Widget* GetRoot() = 0;
		virtual void	Shutdown() = 0;
		virtual Theme*	GetTheme() = 0;
		virtual void	SetTheme(Theme*) = 0;
	};
}
