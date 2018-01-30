server.o: 
	g++ -std=c++0x -w   -o  server  server.cpp
clean:
	rm -rf server