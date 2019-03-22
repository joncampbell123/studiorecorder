PROGS=srfdump
LIBS=

all: $(PROGS)

clean:
	rm -f *.o $(PROGS)

srfdump.o: srfdump.cpp
	g++ -std=c++11 -c -o $@ $<

srfdump: srfdump.o $(LIBS)
	g++ -std=c++11 -o $@ $<

