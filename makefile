

all:
	gcc vm.c -o vm
	./vm

compile:
	gcc vm.c -o vm

clean:
	rm -f vm