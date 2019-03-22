PROGS=srfdump
LIBS=srfiobits.o srfio.o srfiofile.o
CXXFLAGS=-std=c++11 -Wall -Wextra -pedantic

all: $(PROGS)

clean:
	rm -f *.o $(PROGS)

.cpp.o:
	g++ $(CXXFLAGS) -c -o $@ $<

srfdump: srfdump.o $(LIBS)
	g++ $(CXXFLAGS) -o $@ $< $(LIBS)

