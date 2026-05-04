#pragma once

#include <glm/glm.hpp>

enum class CameraMode {
	Fly,
	Orbit,
	Frozen
};

enum class CameraMovement {
	Forward,
	Backward,
	Left,
	Right,
	Up,
	Down
};

class Camera {
public:
	Camera(const glm::vec3& position = glm::vec3(0.0f),
		const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
		float yaw = -90.0f, float pitch = 0.0f);

	glm::mat4 getViewMatrix() const;

	// Mode
	void setMode(CameraMode newMode);
	void cycleMode();
	CameraMode getMode() const { return m_mode; }
	const char* getModeName() const;

	// Input processing
	void processKeyboard(CameraMovement direction, float deltaTime);
	void processMouseMovement(float xoffset, float yoffset);
	void processMouseScroll(float yoffset);

	// State
	void setSprinting(bool sprinting) { m_sprinting = sprinting; }

	const glm::vec3& getPosition() const { return m_position; }
	const glm::vec3& getFront() const { return m_front; }
	const glm::vec3& getUp() const { return m_up; }
	float getZoom() const { return m_zoom; }
	float getYaw() const { return m_yaw; }
	float getPitch() const { return m_pitch; }
	float getMovementSpeed() const { return m_movementSpeed; }

	void logPosition() const;

private:
	void updateVectors();
	void updateOrbitPosition();

	// Fly mode state
	glm::vec3 m_position;
	glm::vec3 m_front{0.0f, 0.0f, -1.0f};
	glm::vec3 m_up;
	glm::vec3 m_right;
	glm::vec3 m_worldUp;
	float m_yaw;
	float m_pitch;
	float m_movementSpeed = 5.0f;
	float m_mouseSensitivity = 0.1f;
	float m_zoom = 45.0f;
	bool m_sprinting = false;

	// Orbit mode state
	glm::vec3 m_focusPoint{0.0f, 0.5f, 0.0f};
	float m_orbitDistance = 3.0f;
	float m_orbitYaw = 0.0f;
	float m_orbitPitch = 20.0f;

	CameraMode m_mode = CameraMode::Fly;

	static constexpr float kSprintMultiplier = 3.0f;
};
