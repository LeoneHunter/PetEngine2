#=======================================================
# gfx module
#=======================================================
module(
    NAME
        gfx
    HDRS
        canvas.h
        common.h
        font_cache.h
        font.h
        shaper.h
    SRCS
        canvas.cpp
        shaper.cpp
        font.cpp
        font_cache.cpp
    DEPS
        gpu
        thirdparty::harfbuzz
        thirdparty::freetype
)

#=======================================================
# Canvas sample
# Shows a rectange on a window using gfx::Canvas
#=======================================================
executable(
    NAME
        canvas_sample
    SRCS
        canvas_sample_app.cpp
    DEPS
        base 
        gfx
        gfx_legacy
)