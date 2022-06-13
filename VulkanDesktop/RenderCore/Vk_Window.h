#pragma once
#include <GLFW\glfw3.h>

class Vk_Window
{
public:
	Vk_Window(uint32_t aWidth, uint32_t aHeight);

	~Vk_Window();

private:
	void myInitWindow();

	void myLoop();

	void myClear();
};

