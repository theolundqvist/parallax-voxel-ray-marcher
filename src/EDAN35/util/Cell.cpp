#pragma
#include "Cell.hpp"

// init static value 
// 其他地方include了这个文件，所以m_CellBoundary被多重定义了
// need to change with the main function
int Cell::m_CellBoundary = 100;

void Cell::randomizeState(int state){
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, state - 1);
	m_Hp = dis(gen);
}

void Cell::pos2RGB() {
	// remap 0-tex_size to 0-255
	glm::vec2 oldRange = glm::vec2(0, m_CellBoundary);
	glm::vec2 newRange = glm::vec2(0, 255);
	m_Color = glm::vec3(remap(m_Pos.x, oldRange, newRange),
						remap(m_Pos.y, oldRange, newRange),
						remap(m_Pos.z, oldRange, newRange));
}

void Cell::distance2RGB(glm::vec3 center) {
	glm::vec3 c2p = m_Pos - center;
	glm::vec2 oldRange = glm::vec2(0, m_CellBoundary/2);
	glm::vec2 newRange = glm::vec2(0, 255);
	m_Color = glm::vec3(remap(c2p.x, oldRange, newRange),
						remap(c2p.y, oldRange, newRange),
						remap(c2p.z, oldRange, newRange));
}

void Cell::density2RGB() {
}

void Cell::hp2RGB(glm::vec3 colorAlive, glm::vec3 colorDead) {
	m_Color = lerp(m_Hp, colorAlive, colorDead);
}

float Cell::remap(float x, glm::vec2 oldRange, glm::vec2 newRange) {
	return (((x - oldRange.x) * (newRange.y - newRange.x)) / (oldRange.y - oldRange.x)) + newRange.x;
}

glm::vec3 Cell::lerp(float x, glm::vec3 colorOne, glm::vec3 colorTwo)
{
	return glm::vec3(lerp(x, colorOne.x, colorTwo.x), 
					 lerp(x, colorOne.y, colorTwo.y),
					 lerp(x, colorOne.z, colorTwo.z));
}

float Cell::lerp(float scale, float x, float y)
{
	return (scale * (y - x) + x);
}

glm::vec3 lerp(float scale, glm::vec3 colorOne, glm::vec3 colorTwo) {
	return glm::vec3(0.0f);
}
