#pragma once
#include <glm/glm.hpp>

class Direction{
	public:
	 static int dad;
		static glm::vec3 up;
		static glm::vec3 down;

		static glm::vec3 front;
		static glm::vec3 back;

		static glm::vec3 right;
		static glm::vec3 left;
};
int Direction::dad = 0;
glm::vec3 Direction::up = glm::vec3(0,1,0);
glm::vec3 Direction::down = glm::vec3(0,-1,0);

glm::vec3 Direction::front = glm::vec3(0,0,1);
glm::vec3 Direction::back = glm::vec3(0,0,-1);

glm::vec3 Direction::right = glm::vec3(1,0,0);
glm::vec3 Direction::left = glm::vec3(-1,0,0);
