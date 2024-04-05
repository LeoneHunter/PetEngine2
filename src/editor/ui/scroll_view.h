#pragma once
#include "widgets.h"
#include "containers.h"
#include "button.h"

namespace UI {

constexpr auto scrollbarThickness = 8.f;

class ScrollViewState;
class Scrolled;

class ScrollChangedEvent: public IEvent {
	EVENT_CLASS(ScrollChangedEvent, IEvent)
public:
	float2 offset;
};

// Called by a Scrollbar when dragged or pressed on scrollbar
// Parameter is normalized scroll position 0.0 - 1.0 of the scrollbar
using OnScrollbarChangedFunc = std::function<void(float)>;

/*
* Draws a scrollbar
*/
class ScrollbarState: public WidgetState {
    WIDGET_CLASS(ScrollbarState, WidgetState)
public:

	ScrollbarState(Axis axis, const OnScrollbarChangedFunc& onScroll)
		: axis_(axis) 
		, visibilityRatio_(1.f)
		, offset_(0.f)
		, trackSize_(0.f)
		, thumb_(this)
		, onScroll_(onScroll)
	{}

    std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) {
		static StringID containerID{"ScrollbarContainer"};
		if(visibilityRatio_ >= 1.f) {
			return {};
		}
		std::vector<std::unique_ptr<Widget>> children;

		if(offset_ > 0.001f) {
			children.push_back(Flexible::New(
				offset_,
				Button::Build()
					.ID("ScrollbarTrackStart")
					.StyleClass("ScrollbarTrack")
					.SizeExpanded()
					.OnPress([this](const ButtonEvent& e) {
						if(e.button == MouseButton::ButtonLeft && e.bPressed) {
							OnTrackClick(true);
						}
					})
					.New()
			));
		}
		children.push_back(Flexible::New(
			visibilityRatio_,
			StatefulWidget::New(&thumb_)
		));
		if(offset_ + visibilityRatio_ < 1.f) {
			children.push_back(Flexible::New(
				1.f - (visibilityRatio_ + offset_),
				Button::Build()
					.ID("ScrollbarTrackEnd")
					.StyleClass("ScrollbarTrack")
					.SizeExpanded()
					.OnPress([this](const ButtonEvent& e) {
						if(e.button == MouseButton::ButtonLeft && e.bPressed) {
							OnTrackClick(false);
						}
					})
					.New()
			));
		}
		return EventListener::New(
			[this](IEvent* e) {
				if(auto* layoutNotification = e->As<LayoutNotification>()) {
					if(layoutNotification->depth == 0 && layoutNotification->source->GetID() == containerID) {
						trackSize_ = layoutNotification->rectLocal.Size()[axis_];
					}
				}
				return false;
			},
			Container::Build()
				.ID(containerID)
				.StyleClass("Transparent")
				.NotifyOnLayoutUpdate()
				.SizeMode(AxisMode{
					axis_ == Axis::Y ? AxisMode::Fixed : AxisMode::Expand,
					axis_ == Axis::Y ? AxisMode::Expand : AxisMode::Fixed,
				})
				.Size(float2{
					axis_ == Axis::Y ? scrollbarThickness : 0.f,
					axis_ == Axis::Y ? 0.f : scrollbarThickness,
				})
				.Child(Flexbox::Build()
					.ID("ScrollbarFlexbox")
					.Direction(axis_ == Axis::Y ? ContentDirection::Column : ContentDirection::Row)
					.Expand()
					.Children(std::move(children))	
					.New()		
				)
				.New()
		);
    }

	// Updates the scrollbar state and rebuilds
	// Receives thumb offset and thumb size normalized. I.e. in the range 0.0-1.0
	void Update(float offset, float ratio) {
		if(offset == offset_ && ratio == visibilityRatio_) {
			return;
		}
		visibilityRatio_ = ratio;
		offset_ = offset;
		MarkNeedsRebuild();
	}

private:

	void SetOffset(float newOffset) {
		if(newOffset + visibilityRatio_ > 1.f) {
			newOffset = 1.f - visibilityRatio_;
		} else if(newOffset < 0.f) {
			newOffset = 0.f;
		}
		if(newOffset == offset_) {
			return;
		}
		offset_ = newOffset;
		if(onScroll_) {
			onScroll_(offset_);
		}
		MarkNeedsRebuild();
	}

	void OnThumbDrag(float delta) {
		const auto deltaNorm = delta / trackSize_;
		const auto newOffset = offset_ + deltaNorm;
		SetOffset(newOffset);	
	}

	void OnTrackClick(bool isStart) {
		// Scroll one page to the start or end
		const auto delta = isStart ? -visibilityRatio_ : visibilityRatio_;
		const auto newOffset = offset_ + delta; 
		SetOffset(newOffset);
	}

	class Thumb: public WidgetState {
	public:

		Thumb(ScrollbarState* parent)
			: parent_(parent) 
		{}

		std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) override {
			return MouseRegion::Build()
				.OnMouseEnter([this]() {
					state_ = StateEnum::Hovered;
					MarkNeedsRebuild();
				})
				.OnMouseLeave([this]() {
					state_ = StateEnum::Normal;
					MarkNeedsRebuild();
				})
				.OnMouseDrag([this](const MouseDragEvent& e) {
					parent_->OnThumbDrag(e.mouseDelta[parent_->axis_]);	
				})
				.Child(Container::Build()
					.ID("ScrollbarThumb")
					.StyleClass("ScrollbarThumb")
					.BoxStyle(state_)
					.SizeExpanded()
					.New()
				)
				.New();
		}

	private:
		ScrollbarState* parent_ = nullptr;
		StringID        state_  = StateEnum::Normal;
	};

private:
	Axis                   axis_;
	float                  visibilityRatio_;
	// Normalized scroll offset 0.0 - 1.0
	float                  offset_;
	Thumb                  thumb_;
	float				   trackSize_;
	OnScrollbarChangedFunc onScroll_;
};


/*
* Containts scroll related state
* Manages scrolling and build a containers for a scrollable child
* The child should be shrinkable on the scroll axis
*/
class ScrollViewState: public WidgetState {
	WIDGET_CLASS(ScrollViewState, WidgetState)
public:

	static inline auto viewportID = StringID("ScrollViewViewport");
	static inline auto contentID = StringID("ScrollViewContent");

	ScrollViewState(bool enableHorizontal, bool enableVertical)
		: enableHorizontalScroll_(enableHorizontal)
		, enableVerticalScroll_(enableVertical) 
		, scrollbarVerticalState_(
			Axis::Y, 
			[this](float offset) { OnScrollbarChanged(Axis::Y, offset); }
		)
	{}

	std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&& child) override;

	void AddListener(Widget* listener) {
		if(std::ranges::find(listeners_, listener) == listeners_.end()) {
			listeners_.push_back(listener);
			ScrollChangedEvent e;
			e.offset = scrollOffset_;
			listener->OnEvent(&e);
		}
	}

	void RemoveListener(Widget* listener) {
		auto it = std::ranges::find(listeners_, listener);
		if(!listeners_.empty() && it != listeners_.end()) {
			listeners_.erase(it);
		}
	}

private:

	void OnViewportLayout(const LayoutNotification& e) {
		LOGF(Verbose, "ScrollView OnViewportLayout called. Vieport size: {}", e.rectLocal.Size());
		if(viewportSize_ != e.rectLocal.Size()) {
			viewportSize_ = e.rectLocal.Size();
			UpdateScrolling();
		}
	}
	
	void OnContentLayout(const LayoutNotification& e) {
		LOGF(Verbose, "ScrollView OnContentLayout called. Content size: {}", e.rectLocal.Size());
		if(contentSize_ != e.rectLocal.Size()) {
			contentSize_ = e.rectLocal.Size();
			UpdateScrolling();
		}
	}

	void UpdateScrolling() {
		auto axis = Axis::Y;
		const auto visibilityRatio = viewportSize_[axis] / contentSize_[axis];

		if(visibilityRatio < 1.f) {
			scrollOffsetMax_[axis] = math::Clamp(contentSize_[axis] - viewportSize_[axis], 0.f);
			scrollOffset_[axis] = math::Clamp(scrollOffset_[axis], 0.f, scrollOffsetMax_[axis]);
		} else {
			scrollOffsetMax_ = float2(0.f);
			scrollOffset_ = float2(0.f);
		}
		NotifyListeners();
	}

	bool OnMouseScroll(const MouseScrollEvent& e) {
		LOGF(Verbose, "ScrollView OnMouseScroll called. Scroll delta: {}", e.scrollDelta);
		auto axis = Axis::Y;
		auto delta = e.scrollDelta * 30.f;
		auto newOffset = scrollOffset_ - delta;

		if(newOffset[axis] > scrollOffsetMax_[axis] && scrollOffset_[axis] == scrollOffsetMax_[axis]) {
			return false;
		} else if(newOffset[axis] < 0.f && scrollOffset_[axis] == 0.f) {
			return false;
		}
		newOffset[axis] = math::Clamp(newOffset[axis], 0.f, scrollOffsetMax_[axis]);
		scrollOffset_ = newOffset;
		NotifyListeners();
		return true;
	}

	// Called from a scrollbar
	void OnScrollbarChanged(Axis axis, float offsetNorm) {
		auto offsetPx = offsetNorm * contentSize_[axis];
		auto newOffset = math::Clamp(offsetPx, 0.f, scrollOffsetMax_[axis]);
		if(newOffset == scrollOffset_[axis]) {
			return;
		}
		scrollOffset_[axis] = newOffset;
		NotifyListeners();
	}

	// Notify Scrolled widgets to update their position
	void NotifyListeners() {
		ScrollChangedEvent scrollEvent;
		scrollEvent.offset = scrollOffset_;
		for(auto& listener: listeners_) {
			listener->OnEvent(&scrollEvent);
		}
		const auto offsetNorm = scrollOffset_[Axis::Y] / contentSize_[Axis::Y];
		const auto visibilityRatio = viewportSize_[Axis::Y] / contentSize_[Axis::Y];
		scrollbarVerticalState_.Update(offsetNorm, visibilityRatio);
	}

private:
	bool                 enableHorizontalScroll_ = false;
	bool                 enableVerticalScroll_   = false;
	float2				 contentSize_;
	float2				 viewportSize_;
	float2               scrollOffset_;
	float2               scrollOffsetMax_;
	// For now Scrolled widgets
	std::vector<Widget*> listeners_;
	ScrollbarState		 scrollbarVerticalState_;
};



/*
* A layout container that wraps its child and 
* is notified when scroll position is changed
* This widget is actually modified on scroll
*/
class Scrolled: public SingleChildLayoutWidget {
	WIDGET_CLASS(Scrolled, SingleChildLayoutWidget)
public:

	static std::unique_ptr<Widget> New(ScrollViewState*          scrollState,
									   StringID                  id,
									   AxisMode					 axisMode,
									   std::unique_ptr<Widget>&& child) {
		auto out = std::unique_ptr<Scrolled>(new Scrolled(id, axisMode));
		scrollState->AddListener(out.get());
		out->SetFloatLayout();
		out->scrollState_ = scrollState;
		out->Parent(std::move(child));
		return out;
	}
	
	bool OnEvent(IEvent* event) override {
		if(auto* drawEvent = event->As<DrawEvent>()) {
			drawEvent->canvas->ClipRect(GetRect());
			return true;
		}
		// Received from the ScrollViewState when the scroll position is changed
		if(auto* scrollChangedEvent = event->As<ScrollChangedEvent>()) {
			// Positive offset corresponds to a negative coordinates because the 0 is at the top of the screen
			SetOrigin(-scrollChangedEvent->offset);
		}
		return Super::OnEvent(event);
	}
	
	~Scrolled() {
		scrollState_->RemoveListener(this);
		scrollState_ = nullptr;
	}

protected:
	Scrolled(StringID id, AxisMode axisMode)
		: Super("", axisMode, true, id) 
	{}

private:
	ScrollViewState* scrollState_;
};




inline std::unique_ptr<Widget> ScrollViewState::Build(std::unique_ptr<Widget>&& child) {
	return EventListener::New(
		[this](IEvent* e) {
			if(auto* layoutNotification = e->As<LayoutNotification>()) {
				if(layoutNotification->depth == 0) {
					if(layoutNotification->source->GetID() == viewportID) {
						this->OnViewportLayout(*layoutNotification);
						return true;
					} else if(layoutNotification->source->GetID() == contentID) {
						this->OnContentLayout(*layoutNotification);
						return true;
					}
				}
			}
			return false;
		},
		MouseRegion::Build()
			.OnMouseScroll([this](const MouseScrollEvent& e) {
				return OnMouseScroll(e);
			})
			.Child(Container::Build()
				.StyleClass("Transparent")
				.SizeExpanded()
				.Child(Flexbox::Build()
					.DirectionRow()
					.Expand()
					.Children(
						Container::Build()
						.ID(viewportID)
						.ClipContent(true)
						.StyleClass("Transparent")
						.SizeExpanded()
						.NotifyOnLayoutUpdate()
						.Child(Scrolled::New(
							this,
							contentID,
							AxisMode{
								enableHorizontalScroll_ ? AxisMode::Shrink : AxisMode::Expand,
								enableVerticalScroll_ ? AxisMode::Shrink : AxisMode::Expand
							},
							std::move(child))
						)
						.New(),
						StatefulWidget::New(&scrollbarVerticalState_)
					)
					.New()
				)					
				.New()
			)
			.New()
	);
}
}


