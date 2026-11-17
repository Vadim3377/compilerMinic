CXX = clang++ -std=c++17
LLVM_CONFIG ?= llvm-config

# Flags for compiling each .cpp file, and for linking them into mccomp.
CXXFLAGS = -g -O3 `$(LLVM_CONFIG) --cppflags` \
           -Wno-unused-function -Wno-unknown-warning-option -fno-exceptions -fno-rtti
LDFLAGS = `$(LLVM_CONFIG) --ldflags --system-libs --libs all`

OBJS = mccomp.o lexer.o ast.o parser.o codegen.o

.PHONY: clean

mccomp: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o mccomp

# Each object is rebuilt when its own source or any header changes.
%.o: %.cpp lexer.h ast.h parser.h codegen.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf mccomp output.ll $(OBJS)
