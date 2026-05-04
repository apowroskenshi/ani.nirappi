#include "camera/Input.h"
#include "camera/Camera.h"

#include <imgui.h>

Input::Input(GLFWwindow* window, Camera& camera)
	: m_window(window)
	, m_camera(camera)
{
}

void Input::update(float deltaTime)
{
	if (ImGui::GetIO().WantCaptureKeyboard) return;

	if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(m_window, true);

	// Sprint
	m_camera.setSprinting(glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

	// Movement
	if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
		m_camera.processKeyboard(CameraMovement::Forward, deltaTime);
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
		m_camera.processKeyboard(CameraMovement::Backward, deltaTime);
	if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
		m_camera.processKeyboard(CameraMovement::Left, deltaTime);
	if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
		m_camera.processKeyboard(CameraMovement::Right, deltaTime);
	if (glfwGetKey(m_window, GLFW_KEY_E) == GLFW_PRESS)
		m_camera.processKeyboard(CameraMovement::Up, deltaTime);
	if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS)
		m_camera.processKeyboard(CameraMovement::Down, deltaTime);

	// Cycle camera mode (TAB - toggle)
	if (glfwGetKey(m_window, GLFW_KEY_TAB) == GLFW_PRESS && !m_tabPressed) {
		m_camera.cycleMode();
		if (m_camera.getMode() == CameraMode::Frozen)
			glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		else
			glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		m_tabPressed = true;
	}
	else if (glfwGetKey(m_window, GLFW_KEY_TAB) == GLFW_RELEASE) {
		m_tabPressed = false;
	}

	// Log camera position (P - toggle)
	if (glfwGetKey(m_window, GLFW_KEY_P) == GLFW_PRESS && !m_pPressed) {
		m_camera.logPosition();
		m_pPressed = true;
	}
	else if (glfwGetKey(m_window, GLFW_KEY_P) == GLFW_RELEASE) {
		m_pPressed = false;
	}
}

void Input::onMouseMove(double xpos, double ypos)
{
	if (ImGui::GetIO().WantCaptureMouse) return;

	float x = static_cast<float>(xpos);
	float y = static_cast<float>(ypos);

	if (m_firstMouse) {
		m_lastX = x;
		m_lastY = y;
		m_firstMouse = false;
	}

	float xoffset = x - m_lastX;
	float yoffset = m_lastY - y;

	m_lastX = x;
	m_lastY = y;

	m_camera.processMouseMovement(xoffset, yoffset);
}

void Input::onMouseScroll(double xoffset, double yoffset)
{
	if (ImGui::GetIO().WantCaptureMouse) return;

	m_camera.processMouseScroll(static_cast<float>(yoffset));
}
