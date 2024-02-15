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

		virtual ~Application() = default;

		static Application*			Create(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);
		static Application*			Get();
		virtual void				Shutdown() = 0;
		// For now we do all ui here
		virtual bool				Tick() = 0;
		// Registers a newly created window
		virtual void				ParentWindow(Window* inWindow) = 0;		
		
		virtual Theme*				GetTheme() = 0;
		// Merges inTheme styles with the prevous theme overriding existent styles 
		virtual void				SetTheme(Theme* inTheme) = 0;

	};
}
