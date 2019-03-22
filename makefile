PROGS=srfdump
LIBS=srfiobits.o srfio.o srfiofile.o

all: $(PROGS)

clean:
	rm -f *.o $(PROGS)

.cpp.o:
	g++ -std=c++11 -c -o $@ $<

srfdump: srfdump.o $(LIBS)
	g++ -std=c++11 -o $@ $<

