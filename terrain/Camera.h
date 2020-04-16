#pragma once

#include <glm/glm.hpp>

struct Transform
{
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;

	Transform() = default;
  Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) :
    position(position), rotation(rotation), scale(scale) {}
};
enum Movement {
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT,
  DOWN,
  UP
};

// Default camera values
const float YAW = glm::radians(90.f);
const float PITCH = glm::radians(0.f);
const float SPEED = 0.1f;
const float SENSITIVTY = 0.25f;
const float ZOOM = 45.0f;
// An abstract camera class that processes input and calculates the corresponding Eular Angles, Vectors and Matrices for use in OpenGL
class CCamera
{
public:
  // Camera Attributes
  Transform transform;
  glm::vec3 Front;
  glm::vec3 Up;
  glm::vec3 Right;
  glm::vec3 WorldUp;
  // Camera options
  float MovementSpeed = 10;
  float Zoom;
  float FOV = 45.0f;
  float Ratio = 16.0f / 9;
  float zNear = 0.1f;
  float zFar = 5000.f;

  enum class Mode
  {
    FPS,
    FLY
  }mode = Mode::FPS;

  // Constructor with vectors
  CCamera(glm::vec3 position = glm::vec3(0,10, -10), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);
  // Constructor with scalar values
  CCamera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

  // Returns the view matrix calculated using Eular Angles and the LookAt Matrix
  glm::mat4 getViewMatrix();
  glm::mat4 getProjectionMatrix();

  glm::vec3 getPosition();
  glm::vec3 getRotation();

  void setPosition(glm::vec3 pos);
  void setRotation(glm::vec3 ang);

  // Processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
  void ProcessKeyboard(Movement direction, float deltaTime);

  // Processes input received from a mouse input system. Expects the offset value in both the x and y direction.
  void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

  // Processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
  void ProcessMouseScroll(float yoffset);

  // Calculates the front vector from the Camera's (updated) Eular Angles
  void updateCameraVectors();
};
