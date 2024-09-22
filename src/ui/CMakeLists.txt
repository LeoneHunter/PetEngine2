
module(
    NAME
        ui
    HDRS
        button.h
        containers.h
        focus.h
        framework.h
        popup.h
        scroll_view.h
        style.h
        tooltip.h
        ui.h
        widgets.h
    SRCS
        framework.cpp
        containers.cpp
        widgets.cpp
    DEPS
        base
        gfx_legacy
)

# test(
#     NAME
#         ui
#     SRCS
#         style_test.cpp
#     DEPS 
#         ui
# )