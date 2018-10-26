# g++ -c -x c++ /root/projects/Myne/src/server/Hosting.cpp -g1 -o "/root/projects/Myne/src/server/obj/MyneServer-Release/Hosting.o" -Wall -Wswitch -W"no-deprecated-declarations" -W"empty-body" -Wconversion -W"return-type" -Wparentheses -W"no-format" -Wuninitialized -W"unreachable-code" -W"unused-function" -W"unused-value" -W"unused-variable" -O3 -fno-strict-aliasing -fomit-frame-pointer -DNDEBUG -fthreadsafe-statics -fexceptions -frtti -std=c++17
# g++ -o "/root/projects/Myne/src/server/bin/Release/MyneServer.out" -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack "/root/projects/Myne/src/server/obj/MyneServer-Release/Hosting.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/Http.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/HttpParser.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/http_parser_ref.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/Listener.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/main.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/MappedFile.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/Tls.o" -lssl -lpthread -lcrypto


CFLAGS    := -Wall -O3 -fno-strict-aliasing -fomit-frame-pointer -std=c99 -ggdb3
CXXFLAGS  := -Wall -O3 -fno-strict-aliasing -fomit-frame-pointer -fthreadsafe-statics -fexceptions -frtti -std=c++17 -ggdb3
LIBS      := -lssl -lpthread -lcrypto -lnghttp2
LDFLAGS   := -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack

OBJDIR  = obj
BINDIR  = ../../bin
CSRC    = http_parser_ref.c
CXXSRC  = Hosting.cpp Http.cpp HttpParser.cpp Listener.cpp MappedFile.cpp Tls.cpp common.cpp
ALL_SRC = $(CSRC) $(CXXSRC) main.cpp
OBJ     = $(patsubst %.c,$(OBJDIR)/%.o,$(CSRC)) $(patsubst %.cpp,$(OBJDIR)/%.o,$(CXXSRC))







all: $(BINDIR)/myne_server

clean:
	@rm -rf *.o myne_server .depend
	@echo Cleaning

.depend: $(ALL_SRC)
	@$(CXX) $(CPPFLAGS) -MM $^>./.depend;

.PHONY: all clean



$(BINDIR)/myne_server: $(OBJ) $(OBJDIR)/main.o
	@mkdir -p $(BINDIR)
	@$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "LD -> $@"



$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/%.o: %.c $(OBJDIR)
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo "CC  $<"

$(OBJDIR)/%.o: %.cpp $(OBJDIR)
	@$(CXX) $(CXXFLAGS) -c -o $@ $<
	@echo "CXX $<"


include .depend
