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
// https://coolors.co/palette/8ecae6-219ebc-023047-ffb703-fb8500
std::vector<glm::vec3> colorPalette::CAColorsBlue2Orange = {
        glm::vec3(214, 40, 40),
        glm::vec3(142, 202, 230),
        glm::vec3(33, 158, 188),
        glm::vec3(255, 183, 3),
        glm::vec3(251, 133, 0),
};
// https://coolors.co/palette/264653-2a9d8f-e9c46a-f4a261-e76f51
std::vector<glm::vec3> colorPalette::CAColorsGreen2Orange = {
        glm::vec3(38,70,83),
        glm::vec3(42,157,143),
        glm::vec3(233, 196, 106),
        glm::vec3(244, 162, 97),
        glm::vec3(231, 111, 81),
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
