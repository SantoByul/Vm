

all:
	gcc vm.c -o vm
	./vm adressess.txt lru fifo

compile:
	gcc vm.c -o vm

clean:
	rm -f vm correct.txt