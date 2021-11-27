BIN = ../bin

Project: Discovery Peer

Discovery: Discovery.o Utilities.o Wrapper.o
	gcc Discovery.o Utilities.o Wrapper.o -o $(BIN)/Discovery -l pthread
	
Peer:	Peer.o
	gcc Peer.o Utilities.o Wrapper.o -o $(BIN)/Peer -l pthread

Discovery.o: Discovery.c
	gcc -c Discovery.c

Peer.o: Peer.c
	gcc -c Peer.c

Utilities.o: Utilities.c Utilities.h
	gcc -c Utilities.c

Wrapper.o: Wrapper.c Wrapper.h
	gcc -c Wrapper.c

clean:
	rm *.o $(BIN)/Discovery $(BIN)/Peer
	 
