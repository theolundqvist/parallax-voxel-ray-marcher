#pragma once
#include <vector>
#include <ctime>
#include <random>
#include <iostream>
#include "glm/glm.hpp"

class Cell {
public:
	Cell(int x, int y, int z) : m_Pos(glm::vec3(x, y, z)) {}

	Cell(glm::vec3 p) : m_Pos(p) {}

	void randomizeState(int state);

	// draw color
	void pos2RGB();
	// use the distance between pos and center
	void distance2RGB(glm::vec3 center);
	// use the number of neighbours to calculate the color
	void density2RGB();
	// use state as a scale to make color from alive color to dead color
	void hp2RGB(glm::vec3, glm::vec3);
	float remap(float, glm::vec2, glm::vec2);
	glm::vec3 lerp(float, glm::vec3, glm::vec3);
	float lerp(float, float, float);

	inline void setNeightbour(int neighbours) { m_Neighbours = neighbours; }

	inline void setHp(int hp) { m_Hp = hp; }

	inline int getHp() const { return m_Hp; }
	
	inline int getNeighbour() const { return m_Neighbours; }

	inline glm::vec3 getColor() const { return m_Color; }

	inline static int getCellBoundary() { return m_CellBoundary; }

	inline void resetHp() {	m_Hp = 0; }

	inline void resetNeighbour() { m_Neighbours = 0; }

	inline void reset() {
		m_Hp = 0;
		m_Neighbours = 0;
	}

	inline static void setCellBoundary(int cellBoundary) { m_CellBoundary = cellBoundary; }


private:
	glm::vec3 m_Pos;
	glm::vec3 m_Color;
	int m_ColorIndex = 0;
	int m_Hp = 0;
	static int m_CellBoundary;
	size_t m_Neighbours = 0;
};
