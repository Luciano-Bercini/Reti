BIN = ../bin

Project: Discovery Peer

Discovery: Discovery.o Utilities.o Wrapper.o
	gcc discovery.o utilities.o wrapper.o -o $(BIN)/discovery -l pthread
	
Peer: Peer.o
	gcc peer.o utilities.o wrapper.o -o $(BIN)/peer -l pthread

Discovery.o: discovery.c discovery.h
	gcc -c discovery.c

Peer.o: peer.c peer.h
	gcc -c peer.c

Utilities.o: utilities.c utilities.h
	gcc -c utilities.c

Wrapper.o: wrapper.c wrapper.h
	gcc -c wrapper.c

clean:
	rm *.o $(BIN)/Discovery $(BIN)/Peer
	 
