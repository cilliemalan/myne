# g++ -c -x c++ /root/projects/Myne/src/server/Hosting.cpp -g1 -o "/root/projects/Myne/src/server/obj/MyneServer-Release/Hosting.o" -Wall -Wswitch -W"no-deprecated-declarations" -W"empty-body" -Wconversion -W"return-type" -Wparentheses -W"no-format" -Wuninitialized -W"unreachable-code" -W"unused-function" -W"unused-value" -W"unused-variable" -O3 -fno-strict-aliasing -fomit-frame-pointer -DNDEBUG -fthreadsafe-statics -fexceptions -frtti -std=c++17
# g++ -o "/root/projects/Myne/src/server/bin/Release/MyneServer.out" -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack "/root/projects/Myne/src/server/obj/MyneServer-Release/Hosting.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/Http.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/HttpParser.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/http_parser_ref.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/Listener.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/main.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/MappedFile.o" "/root/projects/Myne/src/server/obj/MyneServer-Release/Tls.o" -lssl -lpthread -lcrypto


CFLAGS    := -Wall -O3 -fno-strict-aliasing -fomit-frame-pointer -fthreadsafe-statics -std=c99
CXXFLAGS  := -Wall -O3 -fno-strict-aliasing -fomit-frame-pointer -fthreadsafe-statics -fexceptions -frtti -std=c++17
LIBS      := -lssl -lpthread -lcrypto
LDFLAGS   := -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack

CSRC    = http_parser_ref.c
CXXSRC  = Hosting.cpp Http.cpp HttpParser.cpp Listener.cpp MappedFile.cpp Tls.cpp main.cpp
SRC     = $(CSRC) $(CXXSRC)
OBJ     = $(patsubst %.c,%.o,$(CSRC)) $(patsubst %.cpp,%.o,$(CXXSRC))
BIN     = myne_server







all: $(BIN)

clean:
	rm -rfv obj/
	
.depend: $(SRC)
	$(CXX) $(CPPFLAGS) -MM $^>./.depend;

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: all clean .depend





%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

include .depend
