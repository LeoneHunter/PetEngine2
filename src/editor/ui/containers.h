#pragma once
#include "widgets.h"

namespace UI {

class ContainerBuilder;

/*
 * A layout widget that contains a single child
 * Draws a simple rect
 * TODO: rework drawing and style changes
 */
class Container: public SingleChildLayoutWidget {
	WIDGET_CLASS(Container, SingleChildLayoutWidget)
public:

	static ContainerBuilder Build();

	bool OnEvent(IEvent* event) override {
		if(auto* drawEvent = event->As<DrawEvent>()) {
			drawEvent->canvas->DrawBox(GetRect(), style_->FindOrDefault<BoxStyle>(boxStyleName_));
			if(bClip_) {
				drawEvent->canvas->ClipRect(GetRect());
			}
			return true;
		}
		return Super::OnEvent(event);
	}
	
	void DebugSerialize(PropertyArchive& archive) override {
		Super::DebugSerialize(archive);
		archive.PushProperty("Clip", bClip_);
		archive.PushStringProperty("StyleClass", style_ ? *style_->name_ : "null");
		archive.PushStringProperty("BoxStyleSelector", *boxStyleName_);
	}

	// TODO: maybe change on rebuild
	// Sets style selector for box style used for drawing
	void SetBoxStyleName(StringID name) {
		boxStyleName_ = name;
	}

protected:
	Container(StringID styleName, bool notifyOnLayout = false)
		: Super(styleName, axisModeShrink, notifyOnLayout) 
	{}
	friend class ContainerBuilder;

private:
	bool              bClip_ = false;
	const StyleClass* style_;
	StringID          boxStyleName_;
};

class ContainerBuilder {
public:
	auto& ID(StringID inID) { id = inID; return *this; }
	auto& SizeFixed(float2 inSize) { bSizeFixed = true; axisMode = axisModeFixed; size = inSize; return *this; }	
	auto& SizeMode(AxisMode inMode) { axisMode = inMode; return *this;  }
	auto& Size(float2 inSize) { size = inSize; return *this; }
	auto& SizeExpanded() { axisMode = axisModeExpand; bSizeFixed = false; return *this; }	
	auto& PositionFloat(Point inPos) { pos = inPos; bPosFloat = true; return *this; }
	auto& NotifyOnLayoutUpdate() { bNotifyOnLayout = true; return *this; }
	auto& BoxStyle(StringID inStyleName) { boxStyleName = inStyleName; return *this; }
	auto& StyleClass(StringID inStyleName) { styleClass = inStyleName; return *this; }
	auto& ClipContent(bool bClip = true) { bClipContent = bClip; return *this; }
	auto& Child(std::unique_ptr<Widget>&& inChild) { child = std::move(inChild); return *this; }

	std::unique_ptr<Container> New() {
		std::unique_ptr<Container> out(new Container(styleClass, bNotifyOnLayout));
		out->SetID(id);
		out->SetSize(size);
		out->bClip_ = bClipContent;
		out->boxStyleName_ = boxStyleName;

		if(bSizeFixed) {
			out->SetAxisMode(axisModeFixed);
		} else {
			out->SetAxisMode(axisMode);
		}
		if(bPosFloat) {
			out->SetFloatLayout(true);
			out->SetOrigin(pos);
		} 
		out->style_ = Application::Get()->GetTheme()->Find(styleClass);
		if(child) {
			out->Parent(std::move(child));
		}
		return out;
	}

private:
	u8                      bSizeFixed : 1   = false;
	u8                      bPosFloat : 1    = false;
	u8                      bClipContent : 1 = false;
	u8						bNotifyOnLayout:1 = false;
	float2                  size;
	Point                   pos;
	AxisMode                axisMode = axisModeShrink;
	StringID                styleClass;
	StringID                boxStyleName;
	StringID                id;
	std::unique_ptr<Widget> child;
};

inline ContainerBuilder Container::Build() { return {}; }





/*
* Simple wrapper that provides flexFactor for parent Flexbox
* Ignored by other widgets
*/
class Flexible: public SingleChildWidget {
	DEFINE_CLASS_META(Flexible, SingleChildWidget)
public:

	static auto New(float flexFactor, std::unique_ptr<Widget>&& child) {
		return std::make_unique<Flexible>(flexFactor, std::move(child));
	}

	Flexible(float flexFactor, std::unique_ptr<Widget>&& widget) 
		: Super("")
		, flexFactor_(flexFactor) {
		Parent(std::move(widget));
	}

	void DebugSerialize(PropertyArchive& archive) override {
		Super::DebugSerialize(archive);
		archive.PushProperty("FlexFactor", flexFactor_);
	}

	float GetFlexFactor() const { return flexFactor_; }

private:
	float flexFactor_;
};




enum class ContentDirection {
	Column,
	Row
};

enum class JustifyContent {
	Start,
	End,
	Center,
	SpaceBetween,
	SpaceAround
};

enum class OverflowPolicy {
	Clip,        // Just clip without scrolling
	Wrap,        // Wrap children on another line
	ShrinkWrap,  // Do not decrease containter size below content size
				 // Could be used with Scrolled widget to scroll content on overflow
};
class FlexboxBuilder;

/*
 * Vertical or horizontal flex container
 * Similar to Flutter and CSS
 * Default size behavior is shrink to fit children but can be set
 */
class Flexbox: public MultiChildLayoutWidget {
	WIDGET_CLASS(Flexbox, MultiChildLayoutWidget)
public:
	friend class FlexboxBuilder;

	static FlexboxBuilder Build();

	float2 OnLayout(const LayoutConstraints& event) override;
	void DebugSerialize(PropertyArchive& archive) override;

	ContentDirection GetDirection() const { return direction_; }

private:
	void LayOutChildren(const LayoutConstraints& event);

private:
	ContentDirection direction_;
	JustifyContent   justifyContent_;
	Alignment        alignment_;
	OverflowPolicy   overflowPolicy_;
};

DEFINE_ENUM_TOSTRING_2(ContentDirection, Column, Row)
DEFINE_ENUM_TOSTRING_5(JustifyContent, Start, End, Center, SpaceBetween, SpaceAround)
DEFINE_ENUM_TOSTRING_2(OverflowPolicy, Clip, Wrap)


class FlexboxBuilder {
public:
	FlexboxBuilder& ID(const std::string& inID) { id = inID; return *this; }
	FlexboxBuilder& Style(const std::string& inStyle) { style = inStyle; return *this; }
	// Direction of the main axis
	FlexboxBuilder& Direction(ContentDirection inDirection) { direction = inDirection; return *this; }
	FlexboxBuilder& DirectionRow() { direction = ContentDirection::Row; return *this; }
	FlexboxBuilder& DirectionColumn() { direction = ContentDirection::Column; return *this; }
	// Distribution of children on the main axis
	FlexboxBuilder& JustifyContent(UI::JustifyContent inJustify) { justifyContent = inJustify; return *this; }
	// Alignment on the cross axis
	FlexboxBuilder& Alignment(UI::Alignment inAlignment) { alignment = inAlignment; return *this; }
	FlexboxBuilder& AlignCenter() { alignment = Alignment::Center; return *this; }
	FlexboxBuilder& AlignStart() { alignment = Alignment::Start; return *this; }
	FlexboxBuilder& AlignEnd() { alignment = Alignment::End; return *this; }
	// What to do when children don't fit into container
	FlexboxBuilder& OverflowPolicy(OverflowPolicy inPolicy) { overflowPolicy = inPolicy; return *this; }
	FlexboxBuilder& ExpandMainAxis(bool bExpand = true) { expandMainAxis = bExpand; return *this; }
	FlexboxBuilder& ExpandCrossAxis(bool bExpand = true) { expandCrossAxis = bExpand; return *this; }
	FlexboxBuilder& Expand() { expandCrossAxis = true; expandMainAxis  = true; return *this; }

	FlexboxBuilder& Children(std::vector<std::unique_ptr<Widget>>&& inChildren) { 
		for(auto& child: inChildren) {
			children.push_back(std::move(child));
		}
		return *this; 
	}
	
	template<typename ...T> 
		requires (std::derived_from<T, Widget> && ...)
	FlexboxBuilder& Children(std::unique_ptr<T>&& ... child) { 
		(children.push_back(std::move(child)), ...);
		return *this; 
	}

	// Finalizes creation
	std::unique_ptr<Flexbox> New();

private:
	static const inline StringID defaultStyleName = "Flexbox";

	std::string          id;
	StringID             style           = defaultStyleName;
	UI::ContentDirection direction       = ContentDirection::Row;
	UI::JustifyContent   justifyContent  = JustifyContent::Start;
	UI::Alignment        alignment       = Alignment::Start;
	UI::OverflowPolicy   overflowPolicy  = OverflowPolicy::Clip;
	bool                 expandMainAxis  = true;
	bool                 expandCrossAxis = false;

	std::vector<std::unique_ptr<Widget>> children;
};
inline FlexboxBuilder Flexbox::Build() { return {}; }



/*
* Aligns a child inside the parent if parent is bigger that child
*/
class Aligned: public SingleChildLayoutWidget {
	WIDGET_CLASS(Aligned, SingleChildLayoutWidget)
public:

	static auto New(Alignment horizontal, Alignment vertical, std::unique_ptr<Widget>&& child) {
		return std::make_unique<Aligned>(horizontal, vertical, std::move(child));
	}

	Aligned(Alignment horizontal, Alignment vertical, std::unique_ptr<Widget>&& widget) 
		: Super("", axisModeExpand) 
		, horizontal_(horizontal)
		, vertical_(vertical) {
		Parent(std::move(widget));
		SetAxisMode(axisModeFixed);
	}

	float2 OnLayout(const LayoutConstraints& event) override {
		auto* parent = FindParentOfClass<LayoutWidget>();
		auto parentAxisMode = parent ? parent->GetAxisMode() : axisModeExpand;
		SetOrigin(event.rect.TL() + GetLayoutStyle()->margins.TL());

		for(auto axis: axes2D) {
			if(parentAxisMode[axis] == AxisMode::Expand || parentAxisMode[axis] == AxisMode::Fixed) {
				SetSize(axis, event.rect.Size()[axis]);
			}
		}
		if(auto* child = FindChildOfClass<LayoutWidget>()) {
			const auto childSize = child->OnLayout(event);
			
			for(auto axis: axes2D) {
				if(parentAxisMode[axis] == AxisMode::Shrink) {
					SetSize(axis, childSize[axis]);
				}
			}
			Align(child);
			child->OnPostLayout();
		}
		return GetOuterSize();
	}
	
	void DebugSerialize(PropertyArchive& archive) override {
		Super::DebugSerialize(archive);
		archive.PushProperty(
			"AlignmentHorizontal", 
			horizontal_ == Alignment::Start 
				? "Start"
				: horizontal_ == Alignment::Center 
					? "Center"
					: "End");
		archive.PushProperty(
			"AlignmentVertical", 
			vertical_ == Alignment::Start 
				? "Start"
				: vertical_ == Alignment::Center 
					? "Center"
					: "End");
	}

private:

	void Align(LayoutWidget* child) {
		const auto innerSize      = GetSize();
		const auto childMargins   = child->GetLayoutStyle()->margins;
		const auto childOuterSize = child->GetSize() + childMargins.Size();
		Point      childPos;

		for(auto axis: axes2D) {
			auto alignment = axis == Axis::Y ? vertical_ : horizontal_;

			if(alignment == Alignment::Center) {
				childPos[axis] = (innerSize[axis] - childOuterSize[axis]) * 0.5f;
			} else if(alignment == Alignment::End) {
				childPos[axis] = innerSize[axis] - childOuterSize[axis];
			}
		}
		child->SetOrigin(childPos + childMargins.TL());
	}

private:
	Alignment horizontal_;
	Alignment vertical_;
};


}