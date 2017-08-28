all: compile execute

try_read: compile_read execute_read
compile_read: 
	g++ readInputFIle.cpp -o readInput -lGLEW -lGL -Wall -lX11 -lXrandr -lXxf86vm -lrt -lm -pthread -lglfw -I$ATISTREAMSDKROOT/include -L$ATISTREAMSDKROOT/lib/x86 -lOpenCL
compile: 	
	g++ inter.cpp -o inter -lGLEW -lGL -Wall -lX11 -lXrandr -lXxf86vm -lrt -lm -pthread -lglfw -I$ATISTREAMSDKROOT/include -L$ATISTREAMSDKROOT/lib/x86 -lOpenCL
execute_read:
	./readInput kit.obj
execute: 
	./inter
clean: 
	rm -rf *o inter
