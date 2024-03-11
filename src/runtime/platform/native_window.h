#pragma once
#include <functional>
#include <string>

#include "runtime/core/types.h"
#include "runtime/core/math_util.h"
#include "platform.h"

/**
 * Interface to the OS window functionality
 * Handles input events
 */
class INativeWindow {
public:

	static INativeWindow* createWindow(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);

	virtual ~INativeWindow() = default;

	virtual void* GetNativeHandle() = 0;

	virtual void Destroy() = 0;

	virtual void Show() = 0;

	virtual void SetMouseCursor(MouseCursor inCursorType) = 0;

	/**
	 * Poll events from the system and delegate then to the application
	 * @return false if window close is requested
	 */
	virtual bool PollEvents() = 0;

	virtual float2 GetSize() = 0;

	virtual void SetOnCursorMoveCallback(const std::function<void(float, float)>& inCallback) = 0;
	virtual void SetOnMouseButtonCallback(const std::function<void(KeyCode, bool)>& inCallback) = 0;
	virtual void SetOnKeyboardButtonCallback(const std::function<void(KeyCode, bool)>& inCallback) = 0;
	virtual void SetOnWindowResizedCallback(const std::function<void(float2)>& inCallback) = 0;
	virtual void SetOnMouseScrollCallback(const std::function<void(float)>& inCallback) = 0;
	virtual void SetOnCharInputCallback(const std::function<void(wchar_t)>& inCallback) = 0;

	virtual void GetClipboardText(std::wstring& outString) const = 0;
	virtual void SetClipboardText(const std::wstring& inString) const = 0;
};