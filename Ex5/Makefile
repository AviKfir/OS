default: all

all: container sockets

container: container.cpp
	g++ -Wall container.cpp -o container

sockets: sockets.cpp
	g++ -Wall sockets.cpp -o sockets

tar:
	tar -cvf ex5.tar README container.cpp sockets.cpp Makefile
