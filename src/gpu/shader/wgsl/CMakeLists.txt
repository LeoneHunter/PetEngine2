
module(
    NAME
        wgsl
    SRCS
        parser.cpp
        program.cpp
        program_builder.cpp
        ast_printer.cpp
        program_alloc.cpp
    HDRS
        parser.h
        program.h
        program_builder.h
        program_alloc.h
        lexer.h
        token.h
        common.h
        ast_printer.h
        ast_node.h
        ast_attribute.h
        ast_expression.h
        ast_variable.h
        ast_function.h
    DEPS
        base
)

test(
    NAME
        wgsl_lexer
    SRCS
        lexer_test.cpp
    DEPS
        base
)

test(
    NAME
        wgsl_parser
    SRCS
        parser_test.cpp
    DEPS
        base
        wgsl
)

add_subdirectory(signature_parser)