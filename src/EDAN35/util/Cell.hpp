#pragma once
#include <iostream>
#include <vector>
#include <random>
#include "glm/glm.hpp"
#include "../util/voxel_util.cpp"

class Cell {
private:
	glm::vec3 m_Position;
	int m_Hp = 0;
	int m_Neighbors = 0;

public:
	Cell(float x, float y, float z) : m_Position(glm::vec3(x,y,z)) {}
	Cell(glm::vec3 pos) : m_Position(pos) {}

	int randomizeState(int state);
	int pos2colorIndex(glm::vec2 posRange, glm::vec2 colorRange);
	int dis2colorIndex(glm::vec3 center, glm::vec2 desRange, glm::vec2 colorRange);
	int density2colorIndex(glm::vec2 densityRange, glm::vec2 colorRange);
	int hp2colorIndex(glm::vec2 hpRange, glm::vec2 colorRange);

	int getHp() { return m_Hp; }
	void updateHp(int hp) { m_Hp = hp; }
	int getNeighbors() { return m_Neighbors; }
	void updateNeighbors(int neighbors) { m_Neighbors = neighbors; }
	void resetNeighbors() { m_Neighbors = 0; }
	void reset() { m_Hp = 0; m_Neighbors = 0; }
};
