#include "colorPalette.hpp"

std::vector<glm::vec3> colorPalette::CAColorsRed2Green = {
		glm::vec3(0.0f, 0.0f, 0.0f),
		// light red
		glm::vec3(255, 153, 153),
		// light orange
		glm::vec3(255,204,153),
		// light yellow
		glm::vec3(255,255,153),
		// green
		glm::vec3(204, 255,153),
		// light green
		glm::vec3(153, 255, 153),
};

std::vector<glm::vec3> colorPalette::CAColorsBlue2Pink = {
	glm::vec3(153,255,255),
	// light red
	glm::vec3(153,204,255),
	// light orange
	glm::vec3(153,153,255),
	// light yellow
	glm::vec3(204,153,255),
	// light green
	glm::vec3(255,153,255),
	// light blue
	glm::vec3(255,204,229),
};

std::vector<glm::vec3> colorPalette::terrainDefaultColors = {
	glm::vec3(0.0f, 0.0f, 0.0f),
	// water1
	glm::vec3(0.0f, 128, 255),
	// water2
	glm::vec3(51, 153, 255),
	// dirt1
	glm::vec3(240,230,140),
	// dirt2
	glm::vec3(238, 232,170),
	// grass1
	glm::vec3(0,100,0),
	// grass2
	glm::vec3(0,128,0),
	// stone1
	glm::vec3(60,40,40),
	// stone2
	glm::vec3(70,50,50),
	// snow
	glm::vec3(255,255,255),
};
