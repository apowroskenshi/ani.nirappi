#include <cstdlib>

#define GLFW_INCLUDE_NONE
#include <glfw/glfw3.h>
#include <glad/glad.h>

#include "renderer/Scene.h"
#include "camera/Input.h"
#include "core/Logger.h"

namespace {

constexpr int kDefaultWidth  = 2560;
constexpr int kDefaultHeight = 1440;
constexpr const char* kWindowTitle = "Ani Nirappi Renderer";

struct AppContext {
	Scene* scene;
	Input* input;
};

void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
	ctx->input->onMouseMove(xpos, ypos);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
	ctx->input->onMouseScroll(xoffset, yoffset);
}

} // namespace

int main()
{
	// Initialize logging
	static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
	plog::init(plog::debug, &consoleAppender);

	// Initialize GLFW
	if (!glfwInit()) {
		LOG_ERROR << "Failed to initialize GLFW";
		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Create window
	Scene scene;
	scene.window = glfwCreateWindow(kDefaultWidth, kDefaultHeight, kWindowTitle, nullptr, nullptr);

	if (!scene.window) {
		LOG_ERROR << "Failed to create GLFW window";
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(scene.window);

	// Load OpenGL function pointers
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LOG_ERROR << "Failed to initialize GLAD";
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glEnable(GL_DEPTH_TEST);

	// Create input handler and set callbacks BEFORE scene.Init()
	// so ImGui can chain to them when it installs its own callbacks
	Input input(scene.window, scene.camera);
	AppContext ctx{ &scene, &input };
	glfwSetWindowUserPointer(scene.window, &ctx);
	glfwSetCursorPosCallback(scene.window, mouseCallback);
	glfwSetScrollCallback(scene.window, scrollCallback);

	scene.init();

	// Main loop
	float lastFrame = 0.0f;

	while (!glfwWindowShouldClose(scene.window)) {
		const float currentFrame = static_cast<float>(glfwGetTime());
		const float deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		glfwPollEvents();
		input.update(deltaTime);
		scene.update(deltaTime);
		scene.drawScene();
		glfwSwapBuffers(scene.window);
	}

	glfwTerminate();

	return EXIT_SUCCESS;
}
