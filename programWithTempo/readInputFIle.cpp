#define GLEW_STATIC
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include "CL/cl.h"
#include "CL/cl_gl.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glx.h>

#include <iostream>
#include <stdio.h>
#include <vector>
#include "shader.h"
#include "errors.h"
cl_context context;
cl_command_queue queue;
cl_uint num_of_platforms=0;
cl_platform_id platform_id;
cl_device_id device_id;
cl_uint num_of_devices=0;
cl_mem mem;
cl_int err;
// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
bool obtainVertexs( const char * path, std::vector<float> &vertexs);
	
int main(int argc, char* argv[]){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Interoperability", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
   // glfwSetCursorPosCallback(window, mouse_callback);
   // glfwSetScrollCallback(window, scroll_callback);
   
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE; 
    // Initialize GLEW to setup the OpenGL Function pointers
    if (glewInit() != GLEW_OK)
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    } 
	if (clGetPlatformIDs(1, &platform_id, &num_of_platforms)!= CL_SUCCESS)
	{
		int a = num_of_platforms;
		printf("Number of platforms: %d\n",a);
		printf("Unable to get platform_id\n");
		return 1;
	}
	
	
	//Additional  attributes to OpenCL context creation 
	// Create CL context properties, add WGL context & handle to DC 
	cl_context_properties props[] =
	{ 
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id,  //OpenCL platform 
		CL_GL_CONTEXT_KHR,   (cl_context_properties)glXGetCurrentContext(),  //OpenGL context
		CL_GLX_DISPLAY_KHR,     (cl_context_properties)glXGetCurrentDisplay() ,0  ////HDC used to create the OpenGL context
	};  


	// try to get a supported GPU device
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, &num_of_devices);
	std::cout<<"GETTING DEVICES: "<<  getErrorString(err)<< std::endl;

	//Creating the context and the queue
	context = clCreateContext(props,1,&device_id,0,0,NULL);
	queue = clCreateCommandQueue(context,device_id,CL_QUEUE_PROFILING_ENABLE,NULL);
	
//Create a Vertex Buffer & Reading inputData
	std::vector<float> vertexs;
	if(!obtainVertexs(argv[1],vertexs))
	{ 
		std::cout<<"Error reading the input data file"<<std::endl;
		return 1;
	}
	int nvertexs = vertexs.size();
	float* a_vertexs = &vertexs[0];
	
	std::cout<<"TAMANO : " <<nvertexs << std::endl;
	cl_mem vertexBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * nvertexs, NULL, &err);
	std::cout<<"CREATING BUFFER: "<<  getErrorString(err)<< std::endl;


	err = clEnqueueWriteBuffer(queue, vertexBuffer, CL_TRUE, 0, sizeof(float) * nvertexs, a_vertexs, 0, NULL, NULL);
	std::cout<<"WRITING INPUT DATA TO BUFFER: "<<  getErrorString(err)<< std::endl;

}


bool obtainVertexs( const char * path, std::vector<float> &vertexs){
	FILE * file = fopen(path, "r");
	if(file == NULL) return false;
	while(1){
		char lineHeader[128];
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF) break;
		else if ( strcmp( lineHeader, "v" ) == 0 ){
			float x,y,z;
			fscanf(file, "%f %f %f\n", &x, &y, &z); 
			vertexs.push_back(x);
			vertexs.push_back(y);
			vertexs.push_back(z);

		}
		else return false;
	}
	return true;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

