all: compile

compile: 	
	g++ inter.cpp -o inter -lGLEW -lGL -Wall -lX11 -lXrandr -lXxf86vm -lrt -lm -pthread -lglfw -I$ATISTREAMSDKROOT/include -L$ATISTREAMSDKROOT/lib/x86  -lOpenCL
clean: 
	rm -rf *o inter
