SRCS = main.cpp startScreen.cpp shader.cpp LoadShaders.cpp
PROG = water

GCC = g++
FLAGS = -O3 -Wall

LIBS = -lEGL -lGLESv2 -lfreeimage -lbcm_host -lvcos -lwiringPi -lconfig -L/opt/vc/lib
DIRS = -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux

SRCDIR = src
OBJDIR = obj

OBJS = $(patsubst %.cpp,obj/%.o,$(SRCS))

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@echo
	@echo ===== COMPILING SOURCE: $< =====
	@echo
	$(GCC) $(DIRS) $(FLAGS) -c -o $@ $<

all: $(OBJS)
	@echo
	@echo ===== BUILDING EXECUTABLE: $(PROG) =====
	@echo
	g++ $(OBJS) -o $(PROG) $(LIBS) $(FLAGS)
	@echo

clean:
	rm -f -r obj/*.o $(PROG)
