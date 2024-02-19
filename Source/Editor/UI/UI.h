#pragma once
#include <functional>

#include "Runtime/Core/BaseTypes.h"
#include "Style.h"
#include "Widgets.h"

namespace UI {

	using TimerCallback = std::function<bool()>;

	/*
	* Some global state information of the application
	* Can be accesed at any time in read only mode
	* Users should not cache this because its valid only for one frame
	*/
	struct FrameState {
		Point					MousePosGlobal;
		// Number of buttons currently held
		u8						MouseButtonHeldNum = 0;
		MouseButtonMask			MouseButtonsPressedBitField = MouseButtonMask::None;
		// Position of the mouse cursor when the first mouse button has been pressed
		Point					MousePosOnCaptureGlobal;
		KeyModifiersArray		KeyModifiersState{false};
		float2					WindowSize;
		Theme*					Theme = nullptr;
	};

	/*
	* Creates new OS window and a custor renderer for widgets
	* Handles OS input events and dispatches them to widgets
	* Then draw widgets via custom renderer
	* Uses ImGui drawlist and renderer as a backend
	*/
	class Application {
	public:

		virtual ~Application() = default;

		static Application*		Create(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);
		static Application*		Get();
		static FrameState		GetFrameState();

		virtual void			Shutdown() = 0;
		// For now we do all ui here
		virtual bool			Tick() = 0;
		// Registers a newly created window
		virtual void			ParentWindow(Window* inWindow) = 0;		
		
		virtual Theme*			GetTheme() = 0;
		// Merges inTheme styles with the prevous theme overriding existent styles 
		virtual void			SetTheme(Theme* inTheme) = 0;

		virtual void			AddTimer(Widget* inWidget, const TimerCallback& inCallback, u64 inPeriodMs) = 0;
	};
}
