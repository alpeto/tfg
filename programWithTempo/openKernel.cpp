#include <stdio.h>
#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
int main()
{   
	const char *kernel_code;
	std::string inputFilename = "kernel_inter.ocl";
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
	kernel_code = result.c_str();
	std::cout <<kernel_code<<std::endl;
	return 0;
}
