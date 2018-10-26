


export BINDIR    := $(abspath ./bin)
export CFLAGS    := -Wall -O3 -std=c99 -fstack-protector-all -mfunction-return=thunk -mindirect-branch=thunk -fPIE -DNDEBUG -D_NDEBUG
export CXXFLAGS  := -Wall -O3 -std=c++17 -fstack-protector-all -mfunction-return=thunk -mindirect-branch=thunk -fPIE -DNDEBUG -D_NDEBUG
export LDFLAGS   := -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,nodlopen -Wl,-z,nodump -pie
export BUILDDIR  := obj

all: myne_server

debug: CFLAGS   := -Wall -O0 -std=c99 -ggdb3 -DDEBUG -D_DEBUG
debug: CXXFLAGS := -Wall -O0 -std=c++17 -ggdb3 -DDEBUG -D_DEBUG
debug: LDFLAGS  :=
debug: BUILDDIR := dobj
debug: myne_server

clean:
	@$(MAKE) -C src/server clean




myne_server:
	@$(MAKE) -C src/server