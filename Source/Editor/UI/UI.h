#pragma once
#include <functional>

#include "Runtime/Core/BaseTypes.h"
#include "Style.h"
#include "Widgets.h"

namespace UI {

	/*
	* Creates new OS window and a custor renderer for widgets
	* Handles OS input events and dispatches them to widgets
	* Then draw widgets via custom renderer
	* Uses ImGui drawlist and renderer as a backend
	*/
	class Application {
	public:

		static Application* Create(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);
		static Application* Get();

		~Application() = default;

		// For now we do all ui here
		virtual bool				Tick() = 0;
		// Returns top level root widget slot
		virtual WidgetAttachSlot&	GetRoot() = 0;
		virtual void				Shutdown() = 0;
		virtual Theme*				GetTheme() = 0;
		virtual void				SetTheme(Theme* inTheme) = 0;
	};
}
