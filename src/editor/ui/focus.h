#pragma once
#include "widgets.h"
#include "containers.h"

namespace ui {

class Focused;

using FocusChangedFunc = std::function<void(bool)>;

/*
* Stores in the user state object
* A Focused manages this node state
* Should not be deleted before corresponding Focused
*/
class FocusNode {
public:
    friend class Focused;

    FocusNode(const FocusChangedFunc& callback = {}) 
        : onFocusChanged_(callback)
    {}

    void RequestFocus() {
        Assertf(widgets_.size() == 1, 
                "There should be only one Focused when requesting focus.");
        Application::Get()->RequestFocus(this);
    }

    Focused* GetWidget() {
        Assertf(widgets_.size() == 1, 
                "There should be only one Focused when requesting focus.");
        return widgets_.back();
    }

private:
    void AddWidget(Focused* focused) {
        auto it = std::ranges::find(widgets_, focused);
        if(it == widgets_.end()) {
            widgets_.push_back(focused);
        }
    }

    void RemoveWidget(Focused* focused) {
        auto it = std::ranges::find(widgets_, focused);
        if(it != widgets_.end()) {
            widgets_.erase(it);
        }
    }

private:
    bool focused_ = false;
    FocusChangedFunc onFocusChanged_;
    // We will have multiple Focused during build
    std::vector<Focused*> widgets_;
};


/*
* This widget references the node stored in the user state object
* When the managed FocusNode is actually focused, the ancestor KeyboardListeners
*   will be able to receive the keyboard events
*/
class Focused: public SingleChildWidget {
    WIDGET_CLASS(Focused, SingleChildWidget)
public:

    static auto New(FocusNode* focusNode, std::unique_ptr<Widget>&& child) {
        auto out = std::unique_ptr<Focused>(new Focused(focusNode));
        focusNode->AddWidget(out.get());
        out->Parent(std::move(child));
        return out;
    }
    
    ~Focused() {
        node_->RemoveWidget(this);
        node_ = nullptr;
    }

    // Called by Application when the actual focus state has changed
    void OnFocusChanged(bool focused) {
        node_->focused_ = focused;
        if(node_->onFocusChanged_) {
            node_->onFocusChanged_(focused);
        }
    }

    // Do not replace 'this' during rebuild if we reference the same FocusNode
	bool UpdateWith(const Widget* newWidget) override {
		if(auto* w = newWidget->As<Focused>()) {
            if(w->node_ == node_) {
    			CopyConfiguration(*w);
			    return true;
            }
		}
		return false;
	}

protected:
    Focused(FocusNode* node)
        : node_(node) 
    {}

private:
    FocusNode* node_ = nullptr;
};


/*
* Receives the keyboard events if any descendant FocusNode is focused
*/
class KeyboardListener: public SingleChildWidget {
    WIDGET_CLASS(KeyboardListener, SingleChildWidget)
public:



private:
};

}