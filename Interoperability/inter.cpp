#define GLEW_STATIC
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
//#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable
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
cl_kernel kernel_image;
cl_program black_background, red_paint, vertexShaderCL;
cl_mem mem, worldSpaceVertexBuffer, homogenousCoord;	
cl_mem projViewMatrixBuffer;	
cl_int err;
size_t global;
GLuint texture;
std::string inputBlackBackground = "black_background.ocl";
std::string inputShadingCL = "red_points.ocl";
std::string inputVertexShaderCL = "kernel_vertex_shader.ocl";

const char* inputDataFile = "escala.obj";

#define DATA_SIZE 1000000

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 1000;

float fov   =  45.0f;

// camera
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, -1500.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
bool obtainVertexs( const char * path, std::vector<float> &vertexs);
std::string getKernelCode(std::string);
void readCompileKernelProgram(std::string inputFileName, cl_program &prog);

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
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 1, &device_id, &num_of_devices);
	std::cout<<"GETTING DEVICES: "<<  getErrorString(err)<< std::endl;
	
	//Creating the context and the queue
	context = clCreateContext(props,1,&device_id,0,0,NULL);
	queue = clCreateCommandQueue(context,device_id,CL_QUEUE_PROFILING_ENABLE,NULL);
	
	readCompileKernelProgram(inputBlackBackground, black_background);
	readCompileKernelProgram(inputShadingCL, red_paint);
	readCompileKernelProgram(inputVertexShaderCL, vertexShaderCL);

//Create a Vertex Buffer & Reading inputData
	std::vector<float> v_vertexs;
	if(!obtainVertexs(inputDataFile,v_vertexs))
	{ 
		std::cout<<"Error reading the input data file"<<std::endl;
		return 1;
	}
	int nvertexs = v_vertexs.size();
	float* a_vertexs = &v_vertexs[0];

	worldSpaceVertexBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * nvertexs, NULL, &err);
	std::cout<<"CREATING BUFFER: "<<  getErrorString(err)<< std::endl;
	err = clEnqueueWriteBuffer(queue, worldSpaceVertexBuffer, CL_TRUE, 0, sizeof(float) * nvertexs, a_vertexs, 0, NULL, NULL);
	std::cout<<"WRITING INPUT DATA TO BUFFER: "<<  getErrorString(err)<< std::endl;
	clFinish(queue);


// 1.Create 2D texture for rendering (OpenGL) 
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
	//specify texture dimensions, format etc
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);

//2.Create the OpenCL image corresponding to the texture
    mem = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0,texture,&err);
    std::cout<<"TAKING FROM GL TEXTURE: "<<  getErrorString(err)<< std::endl;
//3.Acquire  the ownership via clEnqueueAcquireGLObjects
	glFinish();
	err = clEnqueueAcquireGLObjects(queue, 1,  &mem, 0, 0, NULL); 
	std::cout<<"ACQUIRE OWNERSHIP FROM GL: "<<  getErrorString(err)<< std::endl;
//4 Execute the OpenCL kernel that alters the image

	//4.1 Calculating viewMatrix 
	glm::mat4 view = glm::lookAt(cameraPos, cameraFront, cameraUp);

	//sqlite3_bind_blob(stmt, 0, viewMatrix, sizeof(viewMatrix), SQLITE_STATIC);
	//4.2 Calculating projectionMatrix
	glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		
	glm::mat4 projView = projection * view;
	float *projViewMatrix = glm::value_ptr(projView);
	
	
	//4.3 Creating Buffer for projViewMatrix
	projViewMatrixBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * 16, NULL, &err);
	std::cout<<"CREATING BUFFER PROJVIEWMATRIX: "<<  getErrorString(err)<< std::endl;
	err = clEnqueueWriteBuffer(queue, projViewMatrixBuffer, CL_TRUE, 0, sizeof(float) * 16, projViewMatrix, 0, NULL, NULL);
	std::cout<<"WRITING INPUT DATA TO BUFFER: "<<  getErrorString(err)<< std::endl;
	clFinish(queue);

	//4.5 Creating buffer for the output
	homogenousCoord = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * nvertexs, NULL, &err);
	std::cout<<"CREATING BUFFER Homogenous Coordinates: "<<  getErrorString(err)<< std::endl;

	kernel_image = clCreateKernel(vertexShaderCL, "vertex_shader", &err);
	std::cout<<"CREATING KERNEL VERTEX_SHADER: "<<  getErrorString(err)<< std::endl;
	err = clSetKernelArg(kernel_image, 0, sizeof(cl_mem), &projViewMatrixBuffer); 
	std::cout<<"Setting Kernel Argument projectionMatrix: "<<  getErrorString(err)<< std::endl;
	err = clSetKernelArg(kernel_image, 1, sizeof(cl_mem), &worldSpaceVertexBuffer);
	std::cout<<"Setting Kernel Argument inputBuffer: "<<  getErrorString(err)<< std::endl; 
	err = clSetKernelArg(kernel_image, 2, sizeof(cl_mem), &homogenousCoord);
	std::cout<<"Setting Kernel Argument inputBuffer: "<<  getErrorString(err)<< std::endl;  
	err = clSetKernelArg(kernel_image, 3, sizeof(int), &SCR_WIDTH); 
	std::cout<<"Setting Kernel Argument Screenwidht: "<<  getErrorString(err)<< std::endl;
	err = clSetKernelArg(kernel_image, 4, sizeof(int), &SCR_HEIGHT); 
	std::cout<<"Setting Kernel Argument screenheigh: "<<  getErrorString(err)<< std::endl;

	global = nvertexs/3;
	err = clEnqueueNDRangeKernel(queue, kernel_image, 1, NULL, &global, NULL, 0, NULL, NULL);
	std::cout<<"Enqueuing Range Kernel: "<<  getErrorString(err)<< std::endl;
	
	
	kernel_image = clCreateKernel(black_background, "black_background", &err);
	std::cout<<"CREATING KERNEL BLACK_BACKGROUND: "<<  getErrorString(err)<< std::endl;
	err = clSetKernelArg(kernel_image, 0, sizeof(cl_mem), &mem); 
	std::cout<<"Setting Kernel Argument: "<<  getErrorString(err)<< std::endl;
	
	size_t globalSizes[] = { (size_t)SCR_WIDTH,(size_t)SCR_HEIGHT};
	size_t globalSizesLocal[] = { 1, 1 };
	
	err = clEnqueueNDRangeKernel(queue, kernel_image, 2, NULL, globalSizes, globalSizesLocal, 0, NULL, NULL);  
	std::cout<<"Enqueuing Range Kernel: "<<  getErrorString(err)<< std::endl;
	clFinish(queue);
	
	
//Vertex shade phase

	//PAINTING RED THE VERTEXS
	kernel_image = clCreateKernel(red_paint, "red_points", &err);
	std::cout<<"CREATING KERNEL RED_POINTS: "<<  getErrorString(err)<< std::endl;
	err = clSetKernelArg(kernel_image, 0, sizeof(cl_mem), &mem); 
	std::cout<<"Setting Kernel Argument(texture): "<<  getErrorString(err)<< std::endl;
	err = clSetKernelArg(kernel_image, 1, sizeof(homogenousCoord), &homogenousCoord); 
	std::cout<<"Setting Kernel Argument(vertexs): "<<  getErrorString(err)<< std::endl;
	global = nvertexs/3;
	err = clEnqueueNDRangeKernel(queue, kernel_image, 1, NULL, &global, NULL, 0, NULL, NULL);  
	std::cout<<"Enqueuing Range Kernel: "<<  getErrorString(err)<< std::endl;
	
//5. Releasing the ownership via clEnqueueReleaseGLObjects
	clFinish(queue);
	err = clEnqueueReleaseGLObjects(queue, 1,  &mem, 0, 0, NULL); 
	std::cout<<"RELEASING ENQUEUE OBJECTS: "<<  getErrorString(err)<< std::endl;	
  
     // build and compile our shader zprogram
    // ------------------------------------
    Shader ourShader("inter.vs", "inter.fs");

  // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f, // top right
         1.0f, -1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f  // top left 
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
	    
		// per-frame time logic
		// --------------------


		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(1.0f, 1.0f,1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// bind textures on corresponding texture units
		glActiveTexture(texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		// render		
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

	clReleaseMemObject(mem);
	clReleaseProgram(black_background);
	clReleaseProgram(red_paint);
	clReleaseKernel(kernel_image);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

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

void readCompileKernelProgram(std::string inputFileName, cl_program &prog){
	
	std::string a = getKernelCode(inputFileName);
	const char *ProgramSource = a.c_str();
	
	prog = clCreateProgramWithSource(context,1,(const char **) &ProgramSource, NULL, &err);
	std::cout<<"GETTING PROGRAM "<< inputFileName<< ": "<<  getErrorString(err)<< std::endl;
	// compile the program
	err = clBuildProgram(prog, 0, NULL, NULL, NULL, NULL);
	std::cout<<"BUILDING PROGRAM " <<inputFileName<<": "<<  getErrorString(err)<< std::endl;
}

std::string getKernelCode(std::string inputFilename){
	std::ifstream inputFile(inputFilename);
	std::string result="";
	if(inputFile)
	{
		std::string line;
		while(std::getline(inputFile, line))
		{          
			result += line;
		}
	}
	else
	{
		std::cout << "File '" << inputFilename << "' does not exist." <<  std::endl;
	}
	return result;

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


