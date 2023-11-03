prefix=EDAF80_Assignment

install:
	cmake -G "Unix Makefiles"	-S . -B build
	cmake --build build

1:
	cd build/src/EDAF80 && make $(prefix)1 && ./$(prefix)1
2:
	cd build/src/EDAF80 && make $(prefix)2 && ./$(prefix)2
3:
	cd build/src/EDAF80 && make $(prefix)3 && ./$(prefix)3
4:
	cd build/src/EDAF80 && make $(prefix)4 && ./$(prefix)4
5:
	cd build/src/EDAF80 && make $(prefix)5 && ./$(prefix)5
proj:
	cd build/src/EDAN35 && make EDAN35_Project && ./EDAN35_Project

