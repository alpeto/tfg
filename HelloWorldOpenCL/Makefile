all: compile execute

execute:
	./hello_world

compile: 	
	gcc -o hello_world -I$ATISTREAMSDKROOT/include -L$ATISTREAMSDKROOT/lib/x86 hello_world.c -lOpenCL
clean: 
	rm -rf *o hello_world
