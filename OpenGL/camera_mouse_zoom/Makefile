all: compile execute_kit

execute_kit:
	./camera_mouse_zoom "/home/albert/Universidad/TFG/OpenGL/drawModel/kit.obj"
execute_fertility:
	./camera_mouse_zoom "/home/albert/Universidad/TFG/OpenGL/drawModel/fertility.obj"
	
execute_plane:
	./camera_mouse_zoom "/home/albert/Universidad/TFG/OpenGL/drawModel/plane.obj"

compile: 	
	g++ camera_mouse_zoom.cpp -o camera_mouse_zoom -lGLEW -lGL -lX11 -lXrandr -lXxf86vm -lrt -lm -pthread -lglfw
clean: 
	rm -rf *o camera_mouse_zoom
