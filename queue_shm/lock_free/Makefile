target=test
obj=main.o que_shm.o
all:$(obj)
	g++ -m64 -g -c que_shm.cpp main.cpp
	g++ -m64 -g -o $(target) $(obj) -lrt

clean:
	rm $(target) $(obj)
