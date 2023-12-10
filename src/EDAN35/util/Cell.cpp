#pragma
#include "Cell.hpp"
#include "../util/voxel_util.cpp"

// init static value 
// 其他地方include了这个文件，所以m_CellBoundary被多重定义了
// need to change with the main function
int Cell::m_CellBoundary = 30;
int Cell::m_State = 0;

void Cell::randomizeState(int state){
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, state - 1);
	m_Hp = dis(gen);
}

int Cell::pos2RGB(glm::vec2 oldRange, glm::vec2 newRange) {
	// remap 0-3 * tex_size to 0-colorpalette.size()-1
	float posWeight = m_Pos.x + m_Pos.y + m_Pos.z;
	float weight = m_Hp > 0 ? 1.0f : 0.0f;
	if (posWeight == 0)
		return std::round(voxel_util::remap(weight, oldRange, newRange));
	else {
		//std::cout << std::round(voxel_util::remap(m_Hp * (m_Pos.x + m_Pos.y + m_Pos.z), oldRange, newRange)) << std::endl;
		return std::round(voxel_util::remap(weight * (m_Pos.x + m_Pos.y + m_Pos.z), oldRange, newRange));
	}
}

int Cell::distance2RGB(glm::vec3 center, glm::vec2 oldRange, glm::vec2 newRange) {
	// usually sum of each element of c2p should range from 0 - 1.5*cellBoundary
	glm::vec3 c2p = m_Pos - center;
	float weight = m_Hp > 0 ? 1.0 : 0.0f;
	return std::round(voxel_util::remap(weight * glm::length(c2p), oldRange, newRange));
}

int Cell::density2RGB(glm::vec2 oldRange, glm::vec2 newRange) {
	// neighbours should from 0-26
	float weight = m_Hp > 0 ? 1.0 : 0.0f;
	return std::round(voxel_util::remap(weight * m_Neighbours, oldRange, newRange));
}

int Cell::hp2RGB(glm::vec2 oldRange, glm::vec2 newRange) {
	return voxel_util::remap(m_Hp, oldRange, newRange);
}


