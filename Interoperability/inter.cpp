#define GLEW_STATIC
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <stdio.h>
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

cl_context context;
cl_command_queue queue;
cl_uint num_of_platforms=0;
cl_platform_id platform_id;
cl_device_id device_id;
cl_uint num_of_devices=0;
cl_kernel kernel_image;
cl_program program;
cl_mem mem;	
cl_int err;
size_t global;
GLuint texture;


const char *ProgramSource = 
"__kernel void copy(__global float *mem)\n"\
"{\n"\
"  size_t id = get_global_id(0);\n"\
"  mem[id] = mem[id];\n"\
"}\n";


int tex_height=2, tex_width=2;  //Now it is 0, but this variable will be determined by openCL size data.


#define DATA_SIZE 4
float inputData[DATA_SIZE]={65535.0f, 65535.0f, 65535.0f, 65535.0f}; //simulation of the data
float results[DATA_SIZE]={0};

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float texCoords[] = {
    0.0f, 0.0f,  // lower-left corner  
    0.0f, 1.0f,  // top-left corner
    1.0f, 0.0f,   // lower-right corner
    1.1f, 1.1f   // tope-right corner
};



void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
//void mouse_callback(GLFWwindow* window, double xpos, double ypos);

int main(){
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
	if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, &num_of_devices) != CL_SUCCESS)
	{
		printf("Unable to get device_id\n");
		return 1;
	}

	//Creating the context and the queue
	context = clCreateContext(props,1,&device_id,0,0,NULL);
	queue = clCreateCommandQueue(context,device_id,CL_QUEUE_PROFILING_ENABLE,NULL);

// 1.Create 2D texture (OpenGL) 
	//generate the texture ID 
	glGenTextures(1, &texture); 
	//binding the texture and setting the
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

//2.Create the OpenCL image corresponding to the texture
	//mem = clCreateImage(context, CL_MEM_READ_WRITE, ,,CL_MEM_OBJECT_IMAGE2D,&	);
	mem = clCreateFromGLTexture(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0,texture,&err);
//3.Acquire  the ownership via clEnqueueAcquireGLObjects
	glFinish();
	clEnqueueAcquireGLObjects(queue, 1,  &mem, 0, 0, NULL); 
//4 Execute the OpenCL kernel that alters the image
	//load data into the input buffer
	size_t size3DOrig[3] = {0, 0, 0};
	size_t size3D[3] = {DATA_SIZE, DATA_SIZE, 1};
	clEnqueueWriteImage(queue, mem, CL_TRUE, size3DOrig, size3D,0 , 0, inputData, 0, NULL, NULL);
	
	kernel_image = clCreateKernel(program, "copy", &err);
	clSetKernelArg(kernel_image, 1, sizeof(cl_mem), &mem); 
	global=DATA_SIZE; 
	clEnqueueNDRangeKernel(queue, kernel_image, 1, NULL, &global, NULL, 0, NULL, NULL); 

	
//5. Releasing the ownership via clEnqueueReleaseGLObjects
	clFinish(queue);
	clEnqueueReleaseGLObjects(queue, 1,  &mem, 0, 0, NULL); 
	
	clEnqueueReadImage(queue, mem, CL_TRUE, size3DOrig, size3D,0 , 0, results, 0, NULL, NULL);

	// print the results
	std::cout << "mem (values): ";
	for(int i=0;i<DATA_SIZE; i++)
	{
		std::cout << results[i] << " ";
	}	
	std::cout << std::endl;
	clReleaseMemObject(mem);
	clReleaseProgram(program);
	clReleaseKernel(kernel_image);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
	
	
     // build and compile our shader zprogram
    // ------------------------------------
    Shader ourShader("inter.vs", "inter.fs");

   // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        // positions          // colors           // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left 
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    // -------------------------------------------------------------------------------------------
    ourShader.use(); // don't forget to activate/use the shader before setting uniforms!
    // either set it manually like so:
    glUniform1i(glGetUniformLocation(ourShader.ID, "texture1"), 0);
     // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // bind textures on corresponding texture units
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // render container
        ourShader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();

}


void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

 
