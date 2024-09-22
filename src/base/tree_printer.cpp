#include "tree_printer.h"

JsonTreePrinter::JsonTreePrinter() : depth_(0) {
    Write("{").NewLine().PushDepth();
}

void JsonTreePrinter::ReserveBuffer(uint32_t size) {
    buffer_.reserve(size);
}

TreePrinter& JsonTreePrinter::OpenObject(const std::string& name) {
    WriteIndent().Write("\"", name, "\": {").NewLine().PushDepth();
    return *this;
}

TreePrinter& JsonTreePrinter::CloseObject() {
    PopTrailComma().PopDepth().WriteIndent().Write("},").NewLine();
    return *this;
}

TreePrinter& JsonTreePrinter::OpenArray(const std::string& name) {
    ++arrayDepth_;
    WriteIndent().Write("\"", name, "\": [").NewLine().PushDepth();
    return *this;
}

TreePrinter& JsonTreePrinter::CloseArray() {
    DASSERT(arrayDepth_);
    PopTrailComma().PopDepth().WriteIndent().Write("],").NewLine();
    return *this;
}

TreePrinter& JsonTreePrinter::PushImpl(const std::string& name,
                                       const std::string& value) {
    WriteIndent().Write("\"", name, "\": \"", value, "\",").NewLine();
    return *this;
}

TreePrinter& JsonTreePrinter::PushElementImpl(const std::string& value) {
    DASSERT(arrayDepth_);
    WriteIndent().Write("\"", value, "\",").NewLine();
    return *this;
}

std::string JsonTreePrinter::Finalize() {
    PopTrailComma().PopDepth().Write("}");
    DASSERT(depth_ == 0 && arrayDepth_ == 0);
    return std::move(buffer_);
}

JsonTreePrinter& JsonTreePrinter::WriteIndent() {
    if (depth_) {
        for (auto i = depth_ * indentSize_; i; --i) {
            buffer_.append(" ");
        }
    }
    return *this;
}

JsonTreePrinter& JsonTreePrinter::NewLine() {
    buffer_.append("\n");
    return *this;
}

JsonTreePrinter& JsonTreePrinter::PushDepth() {
    ++depth_;
    return *this;
}

JsonTreePrinter& JsonTreePrinter::PopDepth() {
    DASSERT(depth_);
    --depth_;
    return *this;
}

JsonTreePrinter& JsonTreePrinter::PopTrailComma() {
    if(buffer_.ends_with(",\n")) {
        buffer_.pop_back();
        buffer_.pop_back();
        buffer_ += "\n";
    }
    return *this;
}
