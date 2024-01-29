#pragma once
#include "Runtime/Core/Core.h"
#include "Runtime/Core/Util.h"
#include "Style.h"

namespace UI {

	class Widget;

	using Point = Vec2<float>;

	class Drawable {
		DEFINE_ROOT_CLASS_META(Drawable)
	public:
		virtual ~Drawable() = default;
	};

	class DrawableBox: public Drawable {
		DEFINE_CLASS_META(DrawableBox, Drawable)
	public:

		BoxStyle*	m_Style;
		Point		m_OffsetLocal;
		float2		m_Size;
	};

	class DrawableText: public Drawable {
		DEFINE_CLASS_META(DrawableText, Drawable)
	public:

		TextStyle*			m_Style;
		Point				m_OffsetLocal;
		float2				m_Size;
		std::string_view	m_TextView;
	};

	/*
	* Container for drawables representing a Widget
	* When widget state changes it should invalidate and update it's RenderObject
	*/
	class RenderObject {
	public:

		RenderObject(Widget* inOwner, Point inWidgetOriginGlobal);

		// Draws rectangle with borders and rounding using a BoxStyle 
		void PushBox(Point inOffset, float2 inSize, const std::string& inStyleSelector);

		// Draws the view of a string. Should be invalidated when string changes
		void PushText(std::string_view inTextView, Point inOffset, float2 inSize, const std::string& inStyleSelector);

	private:
		Widget* m_Widget;
		Point	m_OriginGlobal;
	};


	class RenderTree {
	public:





	};

}