ifndef BINDIR
$(error Run top level make)
endif

LIBS    := -lssl -lpthread -lcrypto -lnghttp2

OBJDIR  := $(BUILDDIR)
CSRC    := http_parser_ref.c
CXXSRC  := Hosting.cpp Http.cpp HttpParser.cpp Listener.cpp MappedFile.cpp Tls.cpp common.cpp main.cpp
OBJ     := $(patsubst %.c,$(OBJDIR)/%.o,$(CSRC)) $(patsubst %.cpp,$(OBJDIR)/%.o,$(CXXSRC))


all: $(BINDIR)/myne_server

clean:
	@echo Cleaning
	@rm -rf *.o .depend $(OBJDIR) obj dobj $(BINDIR)/myne_server

.depend: $(CSRC) $(CXXSRC)
	@$(CXX) $(CPPFLAGS) -MM $^>./.depend;

.PHONY: all clean



$(BINDIR)/myne_server: $(OBJ)
	@mkdir -p $(BINDIR)
	@$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "LD -> $@"



$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) -c -o $@ $<


include .depend
