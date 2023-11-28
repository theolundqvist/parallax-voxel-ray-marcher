prefix=EDAN35_Assignment


install: clean
	rm -rf CMakeFiles
	rm -rf CMakeCache.txt
	cmake -G "Unix Makefiles"	-S . -B build
	cmake --build build

install_xcode: clean
	cmake -G "Xcode" -S . -B xbuild
	cmake --build xbuild

2:
	cd build/src/EDAN35 && make $(prefix)2 && ./$(prefix)2
# 2:
# 	cd build/src/EDAF80 && make $(prefix)2 && ./$(prefix)2
project:
	cd build/src/EDAN35 && make EDAN35_Project && ./EDAN35_Project

p: project

clean:
	bash -c "shopt -s globstar && rm -rf **/CMakeFiles && rm -rf **/CMakeCache.txt && rm -rf cmake-build-* && rm -rf build"



