#include "camera/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include "core/Logger.h"

Camera::Camera(const glm::vec3& position, const glm::vec3& up, float yaw, float pitch)
	: m_position(position)
	, m_worldUp(up)
	, m_yaw(yaw)
	, m_pitch(pitch)
{
	updateVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::setMode(CameraMode newMode)
{
	if (newMode == CameraMode::Orbit && m_mode != CameraMode::Orbit) {
		m_focusPoint = m_position + m_front * m_orbitDistance;
		glm::vec3 diff = m_position - m_focusPoint;
		m_orbitDistance = glm::length(diff);
		m_orbitYaw = glm::degrees(atan2(diff.z, diff.x));
		float horizontalDist = glm::length(glm::vec3(diff.x, 0.0f, diff.z));
		m_orbitPitch = glm::degrees(atan2(diff.y, horizontalDist));
	}

	if (newMode == CameraMode::Fly && m_mode == CameraMode::Orbit) {
		m_yaw = glm::degrees(atan2(m_front.z, m_front.x));
		m_pitch = glm::degrees(asin(glm::clamp(m_front.y, -1.0f, 1.0f)));
		updateVectors();
	}

	m_mode = newMode;
}

void Camera::cycleMode()
{
	switch (m_mode) {
		case CameraMode::Fly:    setMode(CameraMode::Orbit);  break;
		case CameraMode::Orbit:  setMode(CameraMode::Frozen); break;
		case CameraMode::Frozen: setMode(CameraMode::Fly);    break;
	}
}

const char* Camera::getModeName() const
{
	switch (m_mode) {
		case CameraMode::Fly:    return "FLY";
		case CameraMode::Orbit:  return "ORBIT";
		case CameraMode::Frozen: return "FROZEN";
	}
	return "UNKNOWN";
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
	if (m_mode != CameraMode::Fly) return;

	float velocity = m_movementSpeed * deltaTime;
	if (m_sprinting) velocity *= kSprintMultiplier;

	switch (direction) {
		case CameraMovement::Forward:  m_position += m_front  * velocity; break;
		case CameraMovement::Backward: m_position -= m_front  * velocity; break;
		case CameraMovement::Left:     m_position -= m_right  * velocity; break;
		case CameraMovement::Right:    m_position += m_right  * velocity; break;
		case CameraMovement::Up:       m_position += m_worldUp * velocity; break;
		case CameraMovement::Down:     m_position -= m_worldUp * velocity; break;
	}
}

void Camera::processMouseMovement(float xoffset, float yoffset)
{
	if (m_mode == CameraMode::Frozen) return;

	if (m_mode == CameraMode::Orbit) {
		m_orbitYaw += xoffset * m_mouseSensitivity;
		m_orbitPitch += yoffset * m_mouseSensitivity;
		m_orbitPitch = glm::clamp(m_orbitPitch, -89.0f, 89.0f);
		updateOrbitPosition();
		return;
	}

	m_yaw += xoffset * m_mouseSensitivity;
	m_pitch += yoffset * m_mouseSensitivity;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

	updateVectors();
}

void Camera::processMouseScroll(float yoffset)
{
	if (m_mode == CameraMode::Frozen) return;

	if (m_mode == CameraMode::Orbit) {
		m_orbitDistance = glm::clamp(m_orbitDistance - yoffset * 0.5f, 0.5f, 50.0f);
		updateOrbitPosition();
		return;
	}

	m_movementSpeed = glm::clamp(m_movementSpeed + yoffset * 0.5f, 0.5f, 50.0f);
}

void Camera::logPosition() const
{
	LOG_DEBUG << "Position: " << m_position.x << " " << m_position.y << " " << m_position.z;
	LOG_DEBUG << "Front: " << m_front.x << " " << m_front.y << " " << m_front.z;
	LOG_DEBUG << "Yaw: " << m_yaw << " Pitch: " << m_pitch;
}

void Camera::updateVectors()
{
	glm::vec3 front;
	front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	front.y = sin(glm::radians(m_pitch));
	front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front = glm::normalize(front);
	m_right = glm::normalize(glm::cross(m_front, m_worldUp));
	m_up = glm::normalize(glm::cross(m_right, m_front));
}

void Camera::updateOrbitPosition()
{
	float radYaw = glm::radians(m_orbitYaw);
	float radPitch = glm::radians(m_orbitPitch);

	m_position.x = m_focusPoint.x + m_orbitDistance * cos(radPitch) * cos(radYaw);
	m_position.y = m_focusPoint.y + m_orbitDistance * sin(radPitch);
	m_position.z = m_focusPoint.z + m_orbitDistance * cos(radPitch) * sin(radYaw);

	m_front = glm::normalize(m_focusPoint - m_position);
	m_right = glm::normalize(glm::cross(m_front, m_worldUp));
	m_up = glm::normalize(glm::cross(m_right, m_front));
}
