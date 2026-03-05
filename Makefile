CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I src/
SRCS     = src/lexer.cpp src/parser.cpp src/integrator.cpp
TARGET   = calc
REPL     = repl

all: $(TARGET) $(REPL)

$(TARGET): $(SRCS) src/main.cpp src/ast.hpp src/ast_ext.hpp
	$(CXX) $(CXXFLAGS) $(SRCS) src/main.cpp -o $@

$(REPL): $(SRCS) src/main_repl.cpp src/ast.hpp src/ast_ext.hpp
	$(CXX) $(CXXFLAGS) $(SRCS) src/main_repl.cpp -o $@ -lreadline

clean:
	rm -f $(TARGET) $(REPL)

.PHONY: all clean
