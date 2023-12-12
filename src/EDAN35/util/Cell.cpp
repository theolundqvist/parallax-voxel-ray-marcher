#include "Cell.hpp"

int Cell::randomizeState(int state) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, state - 1);
	return dis(gen);
}

int Cell::pos2colorIndex(glm::vec2 posRange, glm::vec2 colorRange) {
	float hpWeight = m_Hp > 0 ? 1.0f : 0.0f;
	float posWeight = m_Position.x + m_Position.y + m_Position.z;
	return std::round(voxel_util::remap(hpWeight * posWeight, posRange, colorRange));
}

int Cell::dis2colorIndex(glm::vec3 center, glm::vec2 disRange, glm::vec2 colorRange) {
	glm::vec3 c2p = m_Position - center;
	float hpWeight = m_Hp > 0 ? 1.0 : 0.0f;
	return std::round(voxel_util::remap(hpWeight * glm::length(c2p), disRange, colorRange));
}

int Cell::density2colorIndex(glm::vec2 densityRange, glm::vec2 colorRange) {
	float hpWeight = m_Hp > 0 ? 1.0 : 0.0f;
	return std::round(voxel_util::remap(hpWeight * m_Neighbors, densityRange, colorRange));
}

int Cell::hp2colorIndex(glm::vec2 hpRange, glm::vec2 colorRange) {
	return voxel_util::remap(m_Hp, hpRange, colorRange);
}
