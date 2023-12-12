#include "cellularAutomata.hpp"


void cellularAutomata::updateCAState(int state, glm::vec3 size, glm::vec3 randomStateSize, std::vector<glm::vec3> colors, int drawMode) {
	this->m_State = state;
	this->m_Size = size;
	randomizeCells(randomStateSize);
	this->colorPalette = colors;
	this->drawMode = drawMode;

	//for (int x = 0; x < m_Size.x; x++) {
	//	for (int y = 0; y < m_Size.y; y++) {
	//		for (int z = 0; z < m_Size.z; z++) {
	//			std::cout << m_Cells[x + m_Size.y * (y + m_Size.z * z)].getHp() << std::endl;
	//		}
	//	}
	//}


}

void cellularAutomata::CreateCells(int state, glm::vec3 size) {
	m_Cells.reserve(m_Size.x * m_Size.y * m_Size.z);

	for (int x = 0; x < m_Size.x; x++) {
		for (int y = 0; y < m_Size.y; y++) {
			for (int z = 0; z < m_Size.z; z++) {
				m_Cells.push_back(Cell(x, y, z));
			}
		}
	}
}

void cellularAutomata::resetCells() {
	int totalCells = m_Size.x * m_Size.y * m_Size.z;
	for (int i = 0; i < totalCells; i++) {
		m_Cells[i].reset();
	}
}

void cellularAutomata::randomizeCells(glm::vec3 randomStateSize) {
	// reset all the cells
	resetCells();
	// randomize cells in a certain region
	glm::vec3 center = m_Size / 2;
	for (int x = center.x - (randomStateSize.x / 2); x < (center.x + randomStateSize.x / 2); x++) {
		for (int y = center.y - (randomStateSize.y / 2); y < (center.y + randomStateSize.y / 2); y++) {
			for (int z = center.z - (randomStateSize.z / 2); z < (center.z + randomStateSize.z / 2); z++) {
				m_Cells[voxel_util::conv3dTo1d(x, y, z, m_Size)].randomizeState(m_State);
				//m_Cells[voxel_util::conv3dTo1d(x, y, z, m_Size)].updateHp(m_State-1);
			}
		}
	}
}

void cellularAutomata::updateNeighbors(std::vector<glm::ivec3>& offset) {
	for (int x = 0; x < m_Size.x; x++) {
		for (int y = 0; y < m_Size.y; y++) {
			for (int z = 0; z < m_Size.z; z++) {
				int neighbors = 0;
				int index = voxel_util::conv3dTo1d(x, y, z, m_Size);
				m_Cells[index].resetNeighbors();

				for (int i = 0; i < offset.size(); i++) {
					int sampleX = (x + offset[i].x + m_Size.x) % m_Size.x;
					int sampleY = (y + offset[i].y + m_Size.y) % m_Size.y;
					int sampleZ = (z + offset[i].z + m_Size.z) % m_Size.z;
					int hp = m_Cells[voxel_util::conv3dTo1d(sampleX, sampleY, sampleZ, m_Size)].getHp();
					neighbors += (hp == (m_State - 1));
				}
				m_Cells[index].updateNeighbors(neighbors);
			}
		}
	}
}

// actually neighbors mode is need: Moore or VN
void cellularAutomata::updateCells(std::vector<bool>& survival, std::vector<bool>& spawn) {
	// update neighbors of cells
	updateNeighbors(neighborOffset);

	// update hp based on new neighbors
	for (int x = 0; x < m_Size.x; x++) {
		for (int y = 0; y < m_Size.y; y++) {
			for (int z = 0; z < m_Size.z; z++) {
				int index = voxel_util::conv3dTo1d(x, y, z, m_Size);
				int hp = m_Cells[index].getHp();
				int neighbour = m_Cells[index].getNeighbors();
				
				// dying
				if (hp < (m_State - 1) && hp > 0)
					m_Cells[index].updateHp(hp - 1);
				// start dying next frame
				else if (hp == (m_State - 1) && !survival[neighbour])
					m_Cells[index].updateHp(hp - 1);
				// spawn
				else if (hp == 0 && spawn[neighbour])
					m_Cells[index].updateHp(m_State - 1);
				else
					m_Cells[index].updateHp(hp);
			}
		}
	}
}

int cellularAutomata::findColorIndex(glm::vec3 pos) {
	// may be can set index as a member of cell then we don't need to
	// calculate this so often
	int index = voxel_util::conv3dTo1d(pos.x,pos.y,pos.z,m_Size);
	glm::ivec2 specificRange;
	glm::ivec2 colorRange = glm::ivec2(0, 255);

	int colorIndex = 0;
	switch (drawMode)
	{
	case drawModes::hp2color:
		specificRange = glm::ivec2(0, m_State - 1);
		colorIndex = m_Cells[index].hp2colorIndex(specificRange, colorRange);
		break;
	case drawModes::dis2color:
		// lenth only accept floating point inputs
		specificRange = glm::vec2(0, glm::length(glm::vec3(m_Size)));
		colorIndex = m_Cells[index].dis2colorIndex(m_Size/2, specificRange, colorRange);
		break;
	case drawModes::pos2color:
		specificRange = glm::vec2(0, m_Size.x + m_Size.y + m_Size.z);
		colorIndex = m_Cells[index].pos2colorIndex(specificRange, colorRange);
		break;
	case drawModes::density2color:
		specificRange = glm::vec2(0, 26);
		colorIndex = m_Cells[index].density2colorIndex(specificRange, colorRange);
		break;
	default:
		break;
	}
	return colorIndex;
}

std::vector<CARule> cellularAutomata::CARules = {
	// pyroclastic
	{
		10, // state
		{ 0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },// survival
		{ 0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } // spawn
	},
	// 445
	{
		5, // state
		{ 0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },// survival
		{ 0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } // spawn
	},
	// 678
	{
		3, // state
		{ 0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },// survival
		{ 0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } // spawn
	},
	// amoeba
	{
		5, // state
		{ 0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },// survival
		{ 0,0,0,0,0,1,1,1,0,0,0,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0 } // spawn
	},
	// builder
	{
		10, // state
		{ 0,0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },// survival
		{ 0,0,0,0,1,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } // spawn
	},
	// cloud1
	{
		2, // state
		{ 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },// survival
		{ 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,0,0,0,0,0,0,0 } // spawn
	},
	// spike growth
	{
		4, // state
		{ 0,1,1,1,0,0,0,1,1,1,0,1,1,1,0,0,0,0,1,0,0,1,1,0,1,0,1 },// survival
		{ 0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1,1,1,1,0,1 } // spawn
	},
};

