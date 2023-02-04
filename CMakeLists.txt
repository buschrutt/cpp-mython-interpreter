cmake_minimum_required(VERSION 3.22)
project(cpp-mython-interpreter)

set(CMAKE_CXX_STANDARD 17)

add_executable(mython-interpreter main.cpp lexer.cpp lexer.h lexer_test_open.cpp test_runner_p.h
        statement_test.cpp statement.h statement.cpp runtime_test.cpp runtime.h runtime.cpp
        parse_test.cpp parse.h parse.cpp)