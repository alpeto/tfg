CC=gcc
CFLAGS: -lGLEW -lGL -lX11 -lXrandr -lXxf86vm -lrt -lm -pthread -lglfw

drawModelmake: drawModel.cpp plane.obj kit.obj
	$(CC) -o drawModel drawModel.o $(CFLAGS)
     
     
     # $(CC) -o hellomake hellomake.o hellofunc.o -I.
     #g++ drawModel.cpp -o drawModel -lGLEW -lGL -lX11 -lXrandr -lXxf86vm -lrt -lm -pthread -lglfw
     
clean:
# rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
