#pragma once

#define GLFW_INCLUDE_NONE
#include <glfw/glfw3.h>

class Camera;

class Input {
public:
	Input(GLFWwindow* window, Camera& camera);

	void update(float deltaTime);
	void onMouseMove(double xpos, double ypos);
	void onMouseScroll(double xoffset, double yoffset);

private:
	GLFWwindow* m_window;
	Camera& m_camera;

	// Mouse tracking
	bool m_firstMouse = true;
	float m_lastX = 0.0f;
	float m_lastY = 0.0f;

	// Toggle key state
	bool m_tabPressed = false;
	bool m_pPressed = false;
};
