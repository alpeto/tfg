#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <stdio.h>
#include "CL/cl.h"
#include "CL/cl_gl.h"
#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

cl_context context;
cl_command_queue queue;
cl_uint num_of_platforms=0;
cl_platform_id platform_id;
cl_device_id device_id;
cl_uint num_of_devices=0;
cl_mem mem;	
size_t global;
GLuint texture;


int bufferSize = 0; //Now it is 0, but this variable will be determined by openCL size data.
int tex_height=0, tex_width=0;  //Now it is 0, but this variable will be determined by openCL size data.
std::vector< glm::vec3 > data; //Simulation of the data

int main(){
	
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
	if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_of_devices) != CL_SUCCESS)
	{
		printf("Unable to get device_id\n");
		return 1;
	}


	//Creating the context and the queue
	context = clCreateContext(props,1,&device_id,0,0,NULL);
	queue = clCreateCommandQueue(context,device_id,CL_QUEUE_PROFILING_ENABLE,NULL);


// Create 2D texture (OpenGL) 

	//generate the texture ID 
	glGenTextures(1, &texture); 

	//bdinding the texture and setting the
	glBindTexture(GL_TEXTURE_2D, texture);

	//regular sampler params
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	//need to set GL_NEAREST  
	//(and not GL_NEAREST_MIPMAP_* which results in  CL_INVALID_GL_OBJECTlater)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 

	//specify texture dimensions, format etc  (tiene tex_width & tex_height, maybe they shouldnt)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);


	//Create the OpenGL Pixel-Buffer
	GLuint pbo; 
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_ARRAY_BUFFER, pbo); 
	//specifying the buffer size, this will have to be done when we know the size of the data!!
	glBufferData(GL_ARRAY_BUFFER, bufferSize, &data[0], GL_STATIC_DRAW);

//Create the OpenCL buffer corresponding to the Pixel-Buffer-Object
	mem = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, pbo,  NULL); 


// Acquire  the ownership via clEnqueueAcquireGLObjects/
	glFinish();
	clEnqueueAcquireGLObjects(queue, 1,  &mem, 0, 0, NULL); 

//Modification of the openCLbuffer ??


//Releasing the ownership via clEnqueueReleaseGLObjects
	clFinish(queue);
	clEnqueueReleaseGLObjects(queue, 1,  &mem, 0, 0, NULL);


//Finally,stream data from the PBO to the texture is required
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

}



 
