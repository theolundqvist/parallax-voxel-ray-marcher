#pragma
#include "../util/Cell.hpp"
#include <vector>
#include <iostream>
#include <cmath>

namespace CA {
	static int caCellBoundary = Cell::getCellBoundary();
	static int caTotalCells = caCellBoundary * caCellBoundary * caCellBoundary;
	static enum drawMode {
		pos2RGB = 0,
		distance2RGB = 1,
		density2RGB = 2,
		hp2RGB = 3
	};

	static int convert3dIndexTo1d(int x, int y, int z) {
		return x + caCellBoundary * (y + caCellBoundary * z);
	}

	static int convert3dIndexTo1d(glm::vec3 p) {
		return convert3dIndexTo1d(p.x, p.y, p.z);
	}

	static bool isValidIndex(int x, int y, int z, glm::ivec3 offset) {
		if ((x + offset.x >= 0 && x + offset.x < caCellBoundary) &&
			(y + offset.y >= 0 && y + offset.y < caCellBoundary) &&
			(z + offset.z >= 0 && z + offset.z < caCellBoundary))
			return true;
		return false;
	}

	// create cells with cell boundary
	static std::vector<Cell> createCells(int cellBoundary) {
		std::vector<Cell> temp;
		// don't forget to set the cellBoundary if it's not same as inpur parameter
		if (caCellBoundary != cellBoundary) {
			Cell::setCellBoundary(cellBoundary);
		}
		temp.reserve(cellBoundary * cellBoundary * cellBoundary);

		for (int x = 0; x < cellBoundary; x++) {
			for (int y = 0; y < cellBoundary; y++) {
				for (int z = 0; z < cellBoundary; z++) {
					temp.push_back(Cell(x, y, z));
				}
			}
		}

		return temp;
	}

	static std::vector<glm::ivec3> getOffset(int offsetRangeStart, int offsetRangeEnd) {
		std::vector<glm::ivec3> offsets;
		for (int x = offsetRangeStart; x < offsetRangeEnd + 1; x++) {
			for (int y = offsetRangeStart; y < offsetRangeEnd + 1; y++) {
				for (int z = offsetRangeStart; z < offsetRangeEnd + 1; z++) {
					// remove the central point itself
					if (x != 0 || y != 0 || z != 0) {
						offsets.push_back(glm::ivec3(x, y, z));
					}
				}
			}
		}
		return offsets;
	}

	static int randomizeState(int state) {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, state - 1);
		return dis(gen);
	}

	static void randomizeCells(std::vector<Cell>& cells, glm::vec3 center, int width,
		int height, int depth, int state) {
		// reset all the cell in cells
		for (int i = 0; i < caTotalCells; i++) {
			// reset the hp and neighbours
			cells[i].reset();
		}

		// randomly give state to the particular region
		for (int x = center.x - width / 2; x < center.x + width / 2; x++) {
			for (int y = center.y - height / 2; y < center.y + height / 2; y++) {
				for (int z = center.z - depth / 2; z < center.z + depth / 2; z++) {
					cells[convert3dIndexTo1d(x, y, z)].randomizeState(state);
				}
			}
		}
	}

	static void updateNeighbour(std::vector<Cell>& cells, std::vector<glm::ivec3>& offset, int state, int totalOffset) {
		for (int x = 0; x < caCellBoundary; x++) {
			for (int y = 0; y < caCellBoundary; y++) {
				for (int z = 0; z < caCellBoundary; z++) {
					int neighbours = 0;
					int index = convert3dIndexTo1d(x, y, z);
					// reset the neighbours into 0
					cells[index].resetNeighbour();
					// for each voxel update its number of neighbours
					for (int i = 0; i < totalOffset; i++) {
						//if (isValidIndex(x, y, z, offset[i])) {
						//	int hp = cells[convert3dIndexTo1d(x + offset[i].x,
						//		y + offset[i].y,
						//		z + offset[i].z)].getHp();
						//	neighbours += (hp == (state-1));
						//}
						int sampleX = (x + offset[i].x + caCellBoundary) % caCellBoundary;
						int sampleY = (y + offset[i].y + caCellBoundary) % caCellBoundary;
						int sampleZ = (z + offset[i].z + caCellBoundary) % caCellBoundary;
						int hp = cells[convert3dIndexTo1d(sampleX, sampleY, sampleZ)].getHp();
						neighbours += (hp == (state-1));
						//neighbours += (hp != 0);
					}
					cells[index].setNeightbour(neighbours);
				}
			}
		}
	}

	static void updateCells(std::vector<Cell>& cells, bool* survival, bool* spawn, int state) {
		int totalOffset = 26;
		std::vector<glm::ivec3> offsets = getOffset(-1, 1);

		//for(auto offset : offsets) {
		//	std::cout << offset.x << " " << offset.y << " " << offset.z << std::endl;
		//}

		// update neighbours
		updateNeighbour(cells, offsets, state, totalOffset);

		// update state based on the neighbours
		for (int x = 0; x < caCellBoundary; x++) {
			for (int y = 0; y < caCellBoundary; y++) {
				for (int z = 0; z < caCellBoundary; z++) {
					int index = convert3dIndexTo1d(x, y, z);
					int hp = cells[index].getHp();
					int neighbour = cells[index].getNeighbour();
					//std::cout << neighbour << std::endl;
					// dying or start dying next frame
					//if ((hp < state-1 && hp > 0) || (hp == state - 1 && !survival[neighbour - 1]))

					// dying(may not have this state, for example state = 2
					if (hp < (state - 1) && hp > 0)
						cells[index].setHp(hp - 1);
					// start dying next frame
					else if (hp == (state - 1) && !survival[neighbour])
						cells[index].setHp(hp - 1);
					// spawn
					else if (hp == 0 && spawn[neighbour])
						cells[index].setHp(state - 1);
					else
						cells[index].setHp(hp);

					//hp =
					//	(hp == (state-1)) * (hp - 1 + survival[neighbour]) + // alive
					//	(hp == 0) * (spawn[neighbour] * (state - 1)) +  // dead
					//	(hp > 0 && hp < (state-1)) * (hp - 1); // dying

					//cells[index].setHp(hp);
				}
			}
		}
	}

	static void drawCells(std::vector<Cell>& cells, int drawMode) {

	}


	// caculate color
	// method 1: using pos

	// method 2: using distance from center

	// method 3: using state

	// method 4: using neighbour density
};
