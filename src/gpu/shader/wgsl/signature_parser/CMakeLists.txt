
executable(
    NAME
        wgsl_signature_parser
    SRCS
        parser_main.cpp
    HDRS
        parser.h
    DEPS
        base
        wgsl
)