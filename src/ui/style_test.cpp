#include "style.h"
#include "thirdparty/doctest/doctest.h"

TEST_CASE("[Theme] basic") {
	using namespace ui;

	auto theme = std::make_unique<Theme>();
	auto& class1 = theme->Add("Style1");
	CHECK_EQ(class1.name_, "Style1");

	auto& style1 = class1.Add<BoxStyle>("BoxStyle1");
	style1.FillColor("#dddddd");
	style1.Borders(2);
	CHECK_EQ(style1.name, "BoxStyle1");

	auto* found1 = theme->Find("Style1");
	CHECK(found1);
	auto* foundStyle1 = found1->Find<BoxStyle>("BoxStyle1");
	CHECK(foundStyle1);
	CHECK(foundStyle1 == &style1);

	auto& class3 = theme->Add("Style1");
	CHECK(&class3 == &class1);
	auto& style3 = class3.Add<BoxStyle>("BoxStyle1");
	CHECK(&style3 == &style1);
}

TEST_CASE("[Theme] style copy") {
	using namespace ui;

	auto theme = std::make_unique<Theme>();
	auto& class1 = theme->Add("Style1");

	auto& style1 = class1.Add<BoxStyle>("BoxStyle1");
	style1.FillColor("#dddddd");
	style1.Borders(2);

	auto& style2 = class1.Add<BoxStyle>("BoxStyle2", "BoxStyle1");
	CHECK(style2.backgroundColor == Color::FromHex("#dddddd"));
	CHECK(style2.borders.Bottom == style1.borders.Bottom);
	CHECK(style2.borders.Top == style1.borders.Top);
}

TEST_CASE("[Theme] style inheritance") {
	using namespace ui;

	auto theme = std::make_unique<Theme>();
	auto& class1 = theme->Add("Style1");

	auto& style1 = class1.Add<BoxStyle>("BoxStyle1");
	style1.FillColor("#dddddd");
	style1.Borders(2);

	auto& class2 = theme->Add("Style2", "Style1");
	auto* styleFound = class2.Find<BoxStyle>("BoxStyle1");
	CHECK(styleFound);
	CHECK(styleFound == &style1);
	CHECK(styleFound->backgroundColor == style1.backgroundColor);
}