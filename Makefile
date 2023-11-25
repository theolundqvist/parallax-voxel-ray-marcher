prefix=EDAN35_Assignment

install:
	cmake -G "Unix Makefiles"	-S . -B build
	cmake --build build

2:
	cd build/src/EDAN35 && make $(prefix)2 && ./$(prefix)2
# 2:
# 	cd build/src/EDAF80 && make $(prefix)2 && ./$(prefix)2
project:
	cd build/src/EDAN35 && make EDAN35_Project && ./EDAN35_Project

p: project

