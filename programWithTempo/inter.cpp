#define GLEW_STATIC
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include "CL/cl.h"
#include "CL/cl_gl.h"

#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glx.h>

#include <iostream>
#include <stdio.h>
#include <vector>
#include <time.h>
#include "shader.h"
#include "errors.h"

GLuint64 startTime, stopTime;
unsigned int queryID[2];

cl_context context;
cl_command_queue queue;
cl_uint num_of_platforms=0;
cl_platform_id platform_id;
cl_device_id device_id;
cl_uint num_of_devices=0;
cl_kernel kernel_vs, kernel_dt, kernel_fm, kernel_fs, kernel_bb;
cl_program black_background, fragMerging, vertexShaderCL, fragShader, initializeDepthBuffer;
cl_mem vertex_image, speedBuffer, colorBuffer, worldSpaceVertexBuffer, homogenousCoord, depthBuffer, particleSizeBuffer;	
cl_mem projViewMatrixBuffer;	
cl_int err;
size_t global;
GLuint texture;
std::string inputBlackBackground = "black_background.ocl";
std::string inputFragShader = "fragShader.ocl";
std::string inputFragMerging = "fragMerging.ocl";
std::string inputVertexShaderCL = "kernel_vertex_shader.ocl";
std::string inputInitializeDepthBuffer = "initializeDepthBuffer.ocl";

const char* inputDataFile = "givenDataAndSizeParticles.csv";

std::vector<float> v_vertexs;
std::vector<float> speed;
std::vector<float> particleS;

#define DATA_SIZE 1000000

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 1000;

bool firstMouse = true;
float yaw   = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch =  0.0f;
float lastX =  800.0f / 2.0;
float lastY =  600.0 / 2.0;
float fov   =  45.0f;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;


// camera
glm::vec3 cameraPos   = glm::vec3(-10.0f, 0.0f, 100.0f);
glm::vec3 cameraFront = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
bool obtainVertexs( const char * path, std::vector<float> &vertexs,std::vector<float> &speed, std::vector<float> &particleS);
std::string getKernelCode(std::string);
void readCompileKernelProgram(std::string inputFileName, cl_program &prog);
void createBuffers(int ncoord, int nvertex);
void initializeKernels();

int main(int argc, char* argv[]){
    //Create a Vertex Buffer & Reading inputData	
	if(!obtainVertexs(inputDataFile,v_vertexs,speed, particleS))
	{ 
		std::cout<<"Error reading the input data file"<<std::endl;
		return 1;
	}
	float ncoord = v_vertexs.size();
	float nvertex = ncoord / 3;

	float* a_vertexs = &v_vertexs[0];
	float* a_speed = &speed[0];
	float* a_particles = &particleS[0];
    
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH*4, SCR_HEIGHT*4, "Interoperability", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
   
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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
	checkError(err,"GETTING DEVICES: ");

	
	//Creating the context and the queue
	context = clCreateContext(props,1,&device_id,0,0,NULL);
	queue = clCreateCommandQueue(context,device_id,CL_QUEUE_PROFILING_ENABLE,NULL);
	
	readCompileKernelProgram(inputBlackBackground, black_background);
	readCompileKernelProgram(inputVertexShaderCL, vertexShaderCL);
	readCompileKernelProgram(inputFragShader, fragShader);
	readCompileKernelProgram(inputInitializeDepthBuffer, initializeDepthBuffer);
	readCompileKernelProgram(inputFragMerging, fragMerging);


	createBuffers(ncoord,nvertex);
	
	err = clEnqueueWriteBuffer(queue, speedBuffer, CL_TRUE, 0, sizeof(float) * (nvertex), a_speed, 0, NULL, NULL);
	checkError(err,"Writing input data to buffer speedbuffer");
	clFinish(queue);
	
	err = clEnqueueWriteBuffer(queue, particleSizeBuffer, CL_TRUE, 0, sizeof(float) * (nvertex), a_particles, 0, NULL, NULL);
	checkError(err,"Writing input data to buffer particleSizeBuffer");
	clFinish(queue);
	
	err = clEnqueueWriteBuffer(queue, worldSpaceVertexBuffer, CL_TRUE, 0, sizeof(float) * ncoord, a_vertexs, 0, NULL, NULL);
	checkError(err,"Writing input data to buffer worldSpaceVertexBuffer:");
	clFinish(queue);

	unsigned int VBO, VAO, EBO;

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
	//2.Create the OpenCL image corresponding to the texture & depthBuffer
	vertex_image = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0,texture,&err);
	checkError(err,"Taking from GL Texture:");
		

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
	//  unsigned int VBO, VAO, EBO;
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
	//ourShader.use(); // don't forget to activate/use the shader before setting uniforms!
	// either set it manually like so:
	glUniform1i(glGetUniformLocation(ourShader.ID, "texture1"), 0);

	initializeKernels();
     // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
	    glGenQueries(2, queryID);
	    glQueryCounter(queryID[0], GL_TIMESTAMP);
	      //GLint64 timer;
	    //glBeginQuery();
		
		// per-frame time logic
		// --------------------
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
	
		//3.Acquire  the ownership via clEnqueueAcquireGLObjects
		glFinish();
		err = clEnqueueAcquireGLObjects(queue, 1,  &vertex_image, 0, 0, NULL); 
		checkError(err,"Acquire Ownership from GL:");
		
		//4 Execute the OpenCL kernel that alters the image

		//4.1 Calculating viewMatrix 
		glm::mat4 view = glm::lookAt(cameraPos, cameraFront, cameraUp);

		//sqlite3_bind_blob(stmt, 0, viewMatrix, sizeof(viewMatrix), SQLITE_STATIC);
		//4.2 Calculating projectionMatrix
		glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
			
		glm::mat4 projView = projection * view;
		float *projViewMatrix = glm::value_ptr(projView);

		//4.3 Writing Buffer for projViewMatrix
		err = clEnqueueWriteBuffer(queue, projViewMatrixBuffer, CL_TRUE, 0, sizeof(float) * 16, projViewMatrix, 0, NULL, NULL);
		checkError(err,"Writing input data to projViewMatrixBuffer: ");
		clFinish(queue);

		//4.5 Transforming coordinates
		global = nvertex;
		err = clEnqueueNDRangeKernel(queue, kernel_vs, 1, NULL, &global, NULL, 0, NULL, NULL);
		checkError(err,"Enqueing Range Kernel in Vertex Shader");

		//Painting the background black
		size_t globalSizes[] = { (size_t)SCR_WIDTH,(size_t)SCR_HEIGHT};
		size_t globalSizesLocal[] = { 1, 1 };

		err = clEnqueueNDRangeKernel(queue, kernel_bb, 2, NULL, globalSizes, globalSizesLocal, 0, NULL, NULL);  
		checkError(err,"Enqueing Range Kernel BlackBackground:");
		clFinish(queue);

		//InitializeDepthBuffer
		global = SCR_HEIGHT * SCR_WIDTH;
		err = clEnqueueNDRangeKernel(queue, kernel_dt, 1, NULL, &global, NULL, 0, NULL, NULL);
		checkError(err,"Enqueuing Range Kernel Initialize depthbuffer:");

		//FragmentShader
		global = nvertex;
		err = clEnqueueNDRangeKernel(queue, kernel_fs, 1, NULL, &global, NULL, 0, NULL, NULL);
		checkError(err,"Enqueuing rangeKernel FragmentShader: ");

		//Fragment merging
		global = nvertex;
		err = clEnqueueNDRangeKernel(queue, kernel_fm, 1, NULL, &global, NULL, 0, NULL, NULL);  
		checkError(err,"Enqueing Range Kernel FragmentMerging");

		//5. Releasing the ownership via clEnqueueReleaseGLObjects
		clFinish(queue);
		err = clEnqueueReleaseGLObjects(queue, 1,  &vertex_image, 0, 0, NULL); 
		checkError(err,"Releasing Enqueuing objects: ");
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
	
		ourShader.use();

		// render		
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
		glQueryCounter(queryID[1], GL_TIMESTAMP);
		// wait until the results are available
		int stopTimerAvailable = 0;
		while (!stopTimerAvailable) {
		glGetQueryObjectiv(queryID[1], 
							 GL_QUERY_RESULT_AVAILABLE, 
							 &stopTimerAvailable);
		}

		// get query results
		glGetQueryObjectui64v(queryID[0], GL_QUERY_RESULT, &startTime);
		glGetQueryObjectui64v(queryID[1], GL_QUERY_RESULT, &stopTime);

		printf("Time spent: %f ms\n", (stopTime - startTime) / 1000000.0);
    
    }

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();

	clReleaseMemObject(vertex_image);
	clReleaseProgram(black_background);
	clReleaseProgram(fragMerging);
	clReleaseKernel(kernel_dt);
	clReleaseKernel(kernel_fm);
	clReleaseKernel(kernel_vs);
	clReleaseKernel(kernel_bb);
	clReleaseKernel(kernel_fs);

	clReleaseCommandQueue(queue);
	clReleaseContext(context);

}


void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 10 * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
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
	checkError(err,"Getting program " + inputFileName);
	
	// compile the program
	err = clBuildProgram(prog, 0, NULL, NULL, NULL, NULL);
	checkError(err,"Building program " + inputFileName);
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
bool obtainVertexs( const char * path, std::vector<float> &vertexs, std::vector<float> &speed, std::vector<float> &particleS){
	float maxx = 12749;
	float minx = 5250;
	float maxy = 20749;
	float miny = 13250;
	float maxd = 0.00591643;
	float mind = 6.72733E-11;
	float maxz = 66749;
	float minz = 59250;
	
	FILE * file = fopen(path, "r");
	if(file == NULL) return false;
	while(1){
		float x,y,z,s,siz;
		int res = fscanf(file, "%f %f %f %f %f\n", &x, &y, &z, &s, &siz);	
		if (res == EOF) break;
		else{
			x = (SCR_WIDTH / (maxx-minx)) * (x - minx);
			y = (SCR_HEIGHT / (maxy-miny)) * (y - miny);
			z = (100 / (maxz-minz)) * (z - minz);
			s =  (100 / (maxd-mind)) * (s	 - mind) * 100000;
			vertexs.push_back(x);
			vertexs.push_back(y);
			vertexs.push_back(z);
			speed.push_back(s);
			particleS.push_back(siz);
			//std::cout<<"VERTEX : "<< x << " "<< y << " "<< z <<" "<< s<<std::endl;
		}
	}
	return true;
}

void createBuffers(int ncoord, int nvertex){
	speedBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * nvertex, NULL, &err);
	checkError(err,"Creating Buffer speedBuffer");
	clFinish(queue);
	
	colorBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * ncoord, NULL, &err);
	checkError(err,"Creating buffer colorBuffer");
	
	worldSpaceVertexBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * ncoord, NULL, &err);
	checkError(err,"Creating buffer worldSpaceVertexBuffer");
	
	depthBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * SCR_HEIGHT * SCR_WIDTH, NULL, &err);
	checkError(err,"Creating buffer DepthBuffer: ");
	
	homogenousCoord = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * ncoord, NULL, &err);
	checkError(err,"Creating buffer Homogenous Coordinates: ");
	
	projViewMatrixBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * 16, NULL, &err);
	checkError(err,"Creating buffer projViewMatrixBuffer: ");
	
	particleSizeBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * nvertex, NULL, &err);
	checkError(err,"Creating buffer particleSize: ");
	
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f; // change this value to your liking
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (fov >= 1.0f && fov <= 45.0f)
        fov -= yoffset;
    if (fov <= 1.0f)
        fov = 1.0f;
    if (fov >= 45.0f)
        fov = 45.0f;
}


void initializeKernels(){
	
	//Kernel BlackBackground
	kernel_bb = clCreateKernel(black_background, "black_background", &err);
	checkError(err,"Creating kernel Black_background:");
	err = clSetKernelArg(kernel_bb, 0, sizeof(cl_mem), &vertex_image); 
	checkError(err,"Setting Kernel Argument of black_background:");
	
	//Kernel Vertex Shader
	kernel_vs = clCreateKernel(vertexShaderCL, "vertex_shader", &err);
	checkError(err,"Creating Kernel Vertex Shader");
	err = clSetKernelArg(kernel_vs, 0, sizeof(cl_mem), &projViewMatrixBuffer); 
	checkError(err,"Setting Kernel Argument (projectionMatrix) in Vertex Shader");
	err = clSetKernelArg(kernel_vs, 1, sizeof(cl_mem), &worldSpaceVertexBuffer);
	checkError(err,"Setting Kernel Argument (worldSpaceVertexBuffer) in Vertex Shader");
	err = clSetKernelArg(kernel_vs, 2, sizeof(cl_mem), &homogenousCoord);
	checkError(err,"Setting Kernel Argument (homogenousCoord) in Vertex Shader");  
	err = clSetKernelArg(kernel_vs, 3, sizeof(int), &SCR_WIDTH); 
	checkError(err,"Setting Kernel Argument (ScreenWidth) in Vertex Shader");
	err = clSetKernelArg(kernel_vs, 4, sizeof(int), &SCR_HEIGHT); 
	checkError(err,"Setting Kernel Argument (ScreenHeight) in Vertex Shader");
	
	//Kernel DepthTest
	kernel_dt = clCreateKernel(initializeDepthBuffer, "initializeDepthBuffer", &err);
	checkError(err,"Creating kernel InitializeDepthBuffer: ");
	err = clSetKernelArg(kernel_dt, 0, sizeof(depthBuffer), &depthBuffer); 
	checkError(err,"Setting Kernel Argument(depthBuffer) initialize depthbuffer:");
	
	//Kernel Fragment Shader
	kernel_fs = clCreateKernel(fragShader, "fragShader", &err);
	checkError(err,"CREATING KERNEL FragShader: ");
	err = clSetKernelArg(kernel_fs, 0, sizeof(speedBuffer), &speedBuffer); 
	checkError(err,"Setting Kernel Argument(speedBuffer):");
	err = clSetKernelArg(kernel_fs, 1, sizeof(colorBuffer), &colorBuffer); 
	checkError(err,"Setting Kernel Argument(colorBuffer) in FragmentShader: ");
	
	//Kernel Fragment Merging
	kernel_fm = clCreateKernel(fragMerging, "fragMerging", &err);
	checkError(err,"Creating Kernel FragMerging:");
	err = clSetKernelArg(kernel_fm, 0, sizeof(cl_mem), &vertex_image); 
	checkError(err,"Setting Kernel Argument (texture) in FragmentMerging");
	err = clSetKernelArg(kernel_fm, 1, sizeof(homogenousCoord), &homogenousCoord); 
	checkError(err,"Setting Kernel Argument (vertexs) in FragmentMerging");	
	err = clSetKernelArg(kernel_fm, 2, sizeof(colorBuffer), &colorBuffer); 
	checkError(err,"Setting Kernel Argument (colorBuffer) in FragmentMerging");
	err = clSetKernelArg(kernel_fm, 3, sizeof(depthBuffer), &depthBuffer); 
	checkError(err,"Setting Kernel Argument (depthBuffer) in FragmentMerging");
	err = clSetKernelArg(kernel_fm, 4, sizeof(SCR_WIDTH), &SCR_WIDTH); 
	checkError(err,"Setting Kernel Argument Screen width in FragmentMerging");
	err = clSetKernelArg(kernel_fm, 5, sizeof(SCR_HEIGHT), &SCR_HEIGHT); 
	checkError(err,"Setting Kernel Argument Screen height in FragmentMerging");
	err = clSetKernelArg(kernel_fm, 6, sizeof(particleSizeBuffer), &particleSizeBuffer); 
	checkError(err,"Setting Kernel Argument (particleSizeBuffer) in FragmentMerging");
}

