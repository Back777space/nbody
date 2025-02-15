#pragma once

#include "include/glm/glm.hpp"
#include "include/glm/gtc/matrix_transform.hpp"
#include "include/glm/gtc/type_ptr.hpp"
#include "include/glm/gtx/string_cast.hpp"

class Camera {
    private:
        float sensitivity = 0.10f;
        float speed = 140.0f;

        bool firstMouse = true;

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); //  normalized up-direction

        glm::vec3 movingDirection = {0.f, 0.f, 0.f};
        glm::vec3 direction = {0.0, 0.0, 0.0}; // normalized direction the camera is pointing to
        glm::vec3 position; // position of camera

        float yaw = 0.f;   // degrees around the y-axis
        float pitch = 0.f; // degrees around the x-axis

        // screen coordinates
        float lastX = 0.0f;
        float lastY = 0.0f;

        void setDirection(float yaw, float pitch) {
            glm::vec3 directionTemp;
            directionTemp.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            directionTemp.y = sin(glm::radians(pitch));
            directionTemp.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            this->direction = glm::normalize(directionTemp);
        }
        
        void incrementPosition(glm::vec3 deltaPos) {
            position += deltaPos;
        }


    public:
        enum class InputEvent {
            FORWARDS,
            BACKWARDS,
            LEFT,
            RIGHT,
            UP,
            DOWN
        };

        Camera(
            glm::vec3 position,
            float yaw = -90.0f,
            float pitch = 0.0f
        ) : position{position} {
            this->setDirection(yaw, pitch);
        }

        glm::mat4 getView() const {
            return glm::lookAt(position, position + direction, up);
        }

        void setDirectionByMouse(float xPos, float yPos) {
            // i hate this
            if (firstMouse) {
                lastX = xPos;
                lastY = yPos;
                firstMouse = false;
            }

            float xOffset = xPos - lastX;
            float yOffset = lastY - yPos;
            lastX = xPos;
            lastY = yPos;
            yaw += xOffset * sensitivity;
            pitch += yOffset * sensitivity;
        
            if (pitch > 89.0f) 
                pitch = 89.0f;
            if (pitch < -89.0f) 
                pitch = -89.0f;
            setDirection(yaw, pitch);
        }

        glm::vec3 getPosition() {
            return this->position;
        }

        void update(float dt) {
            if (movingDirection.x + movingDirection.y + movingDirection.z != 0.0) {
                glm::vec3 directionNormalized = glm::normalize(movingDirection);
                incrementPosition(directionNormalized * speed * dt);
                movingDirection = glm::vec3{0.0f, 0.0f, 0.0f};
            }
        }

        void handleKeyInput(InputEvent event) {
            float x = this->direction.x;
            float z = this->direction.z;
            glm::vec3 left = glm::cross(this->up, this->direction);
            switch (event) {
                case InputEvent::FORWARDS:
                    movingDirection += glm::vec3{x, 0.f, z};
                    break;
                case InputEvent::BACKWARDS:
                    movingDirection -= glm::vec3{x, 0.f, z};
                    break;
                case InputEvent::LEFT:
                    movingDirection += glm::vec3{left.x, 0.f, left.z};
                    break;
                case InputEvent::RIGHT:
                    movingDirection -= glm::vec3{left.x, 0.f, left.z};
                    break;
                case InputEvent::UP:
                    movingDirection += up;
                    break;
                case InputEvent::DOWN:
                    movingDirection -= up;
                    break;
            }
        }
};