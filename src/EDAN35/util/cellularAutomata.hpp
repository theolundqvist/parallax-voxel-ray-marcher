#pragma once
#include <vector>
#include <random>
#include "../util/Cell.hpp"
#include "../util/voxel_util.cpp"
#include "glm/glm.hpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"


typedef struct CARule {
	int state;
	std::vector<bool> survival;
	std::vector<bool> spawn;
} CARule; 


class cellularAutomata {
private:
	std::vector<Cell> m_Cells;
	glm::ivec3 m_Size;
	int m_State;

public:
	int drawMode;
	std::vector<glm::vec3> colorPalette;
	std::vector<glm::ivec3> neighborOffset;
	enum drawModes {
		hp2color = 1,
		dis2color = 2,
		pos2color = 3,
		density2color = 4
	};
    enum CA_rule {
        R_PYROCLASTIC,
        R_445,
        R_678,
        R_CLOUD1,
        R_SPIKE_GROWTH,
        NBR_CA_RULES
    };
	static std::vector<CARule> CARules;
	// c++ not allowed static enum in class
	//static enum drawModes;

	cellularAutomata(int state, glm::vec3 size, glm::vec3 randomStateSize, std::vector<glm::vec3> colors, int drawMode) :
					 m_State(state), m_Size(size), colorPalette(colors), drawMode(drawMode) {
		neighborOffset = voxel_util::generateOffset(-1, 1);
		CreateCells(state, size);
		randomizeCells(randomStateSize);
	}

	void updateCAState(int state, glm::vec3 size, glm::vec3 randomStateSize, std::vector<glm::vec3> colors, int drawMode);

	void CreateCells(int state, glm::vec3 size);
	void randomizeCells(glm::vec3 randomStateSize); // take center of a volume as default
	void updateNeighbors(std::vector<glm::ivec3>& offset);
	void updateCells(std::vector<bool>& survival, std::vector<bool>& spawn);
	int findColorIndex(glm::vec3 pos);
	void resetCells();

	glm::vec3 size() { return m_Size; }
	void changeSize(glm::vec3 size) { m_Size = size; }
	int getState() { return m_State; }
	void updateState(int state) { m_State = state; }


};
