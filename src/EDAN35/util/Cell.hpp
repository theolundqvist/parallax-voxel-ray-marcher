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
	int pos2RGB(glm::vec2, glm::vec2);
	// use the distance between pos and center
	int distance2RGB(glm::vec3, glm::vec2, glm::vec2);
	// use the number of neighbours to calculate the color
	int density2RGB(glm::vec2, glm::vec2);
	// use state as a scale to make color from alive color to dead color
	int hp2RGB(glm::vec2, glm::vec2);
	
	inline void setNeightbour(int neighbours) { m_Neighbours = neighbours; }

	inline void setHp(int hp) { m_Hp = hp; }

	inline int getHp() const { return m_Hp; }
	
	inline int getNeighbour() const { return m_Neighbours; }

	inline void resetHp() {	m_Hp = 0; }

	inline void resetNeighbour() { m_Neighbours = 0; }

	inline void reset() {
		m_Hp = 0;
		m_Neighbours = 0;
	}

	inline static void setCellBoundary(int cellBoundary) { m_CellBoundary = cellBoundary; }
	inline static int getCellBoundary() { return m_CellBoundary; }

	inline static void setCellState(int state) { m_State = state; }
	inline static int getCellState() { return m_State; }

private:
	glm::vec3 m_Pos;
	int m_Hp = 0;
	static int m_CellBoundary;
	static int m_State;
	size_t m_Neighbours = 0;
};
