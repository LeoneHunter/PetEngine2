#pragma once
#include "widgets.h"
#include "containers.h"

namespace ui {

class TooltipPortal;

struct TooltipBuildContext {
	Point 			mousePosGlobal;
	TooltipPortal* 	sourceWidget{};
};

using TooltipBuilderFunc = std::function<std::unique_ptr<Widget>(const TooltipBuildContext&)>;

/*
 * Builds a simple text tooltip
 */
struct TextTooltipBuilder {
	TextTooltipBuilder& Text(const std::string& text) { this->text = text; return *this; }
	TextTooltipBuilder& ClipText(bool bClip) { bClipText = bClip; return *this; }
	TextTooltipBuilder& StyleClass(StringID styleName) { styleClass = styleName; return *this; }

	std::unique_ptr<Widget> New() {
		return Container::Build()
			.StyleClass(styleClass)
			.ClipContent(bClipText)
			.Child(TextBox::New(text))
			.New();
	}

private:
	StringID    styleClass = "Tooltip";
	std::string text;
	bool        bClipText = false;
};

/*
 * Opens a tooltip when hovered
 * TODO: maybe convert to state and use MouseRegion and notifications instead
 */
class TooltipPortal: public MouseRegion {
	WIDGET_CLASS(TooltipPortal, MouseRegion)
public:

	static auto New(const TooltipBuilderFunc& builder, std::unique_ptr<Widget>&& child) {
		return std::make_unique<TooltipPortal>(builder, std::move(child));
	}

	TooltipPortal(const TooltipBuilderFunc& builder, std::unique_ptr<Widget>&& child)
		: MouseRegion(MouseRegionConfig{
			.onMouseLeave = [this]() { OnMouseLeave(); },
			.onMouseHover = [this](const auto& e) { OnMouseHover(e); }, 
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
			.bHandleButtonAlways = true}
		)
		, builder_(builder) {
		Parent(std::move(child));
	}

	void DebugSerialize(PropertyArchive& ar) override {
		Super::DebugSerialize(ar);
		ar.PushProperty("Timer", (uintptr_t)sharedState.timerHandle);
		ar.PushProperty("Tooltip", sharedState.widget ? sharedState.widget->GetDebugID() : "null");
		ar.PushProperty("Disabled", sharedState.bDisabled);
	}

	bool UpdateWith(const Widget* newWidget) override {
		if(auto* w = newWidget->As<TooltipPortal>()) {
			CopyConfiguration(*w);
			return true;
		}
		return false;
	}

private:
	void OnMouseButton(const MouseButtonEvent& event) {
		CloseTooltip();
		if(sharedState.timerHandle) {
			Application::Get()->RemoveTimer(sharedState.timerHandle);
			sharedState.timerHandle = {};
		}
		sharedState.bDisabled = true;
	}

	void OnMouseHover(const HoverEvent& event) {
		constexpr unsigned delayMs = 500;

		// FIXME: doesn't work properly on rebuild
		const bool bOverlapping = sharedState.portal && sharedState.portal != this;
		// If this tooltip area covers another tooltip area
		// For example a tab and a tab close button with different tooltips
		if(bOverlapping && event.bHoverEnter) {
			// Reset state and close tooltip
			if(sharedState.timerHandle) {
				Application::Get()->RemoveTimer(sharedState.timerHandle);
				sharedState.timerHandle = {};
			} else if(sharedState.widget) {
				CloseTooltip();
			}
			sharedState.bDisabled = false;
		}
		const bool bStateClean = !sharedState.bDisabled && !sharedState.widget && !sharedState.timerHandle;

		if(bStateClean) {
			const auto timerCallback = [this]()->bool {
				OpenTooltip();
				return false;
			};
			sharedState.timerHandle = Application::Get()->AddTimer(this, timerCallback, delayMs);
			sharedState.portal = this;
			return;
		}
	}

	void OnMouseLeave() {
		if(sharedState.timerHandle) {
			Application::Get()->RemoveTimer(sharedState.timerHandle);
			sharedState.timerHandle = {};
		} else if(sharedState.widget) {
			CloseTooltip();
		}
		sharedState.bDisabled = false;
		sharedState.portal = nullptr;
	}

	void OpenTooltip() {
		auto ctx           = Application::Get()->GetState();
		auto widget 	   = builder_(TooltipBuildContext{ctx.mousePosGlobal, this});
		sharedState.widget = widget.get();
		auto* layout       = LayoutWidget::FindNearest(sharedState.widget);
		Assertf(layout, "Tooltip widget {} has no LayoutWidget child, so it won't be visible.", sharedState.widget->GetDebugID());

		layout->SetOrigin(ctx.mousePosGlobal + float2(15));
		layout->SetFloatLayout(true);
		Application::Get()->Parent(std::move(widget), Layer::Overlay);
		sharedState.timerHandle = {};
	}

	void CloseTooltip() {
		if(sharedState.widget) {
			sharedState.widget->Destroy();
			sharedState.widget = nullptr;
		}
	}

	void CopyConfiguration(const TooltipPortal& other) {
		//Super::CopyConfiguration(other);
		builder_ = other.builder_;
	}

private:
	// We use static shared state because there's no point in 
	//     having multipler tooltips visible at once
	struct StateEnum {
		TooltipPortal*  portal{};
		TimerHandle 	timerHandle{};
		Widget* 		widget = nullptr;
		bool			bDisabled = false;
	};
	static inline StateEnum sharedState{};

	TooltipBuilderFunc builder_;
};
}