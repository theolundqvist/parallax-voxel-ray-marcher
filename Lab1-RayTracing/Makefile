
install:
	cmake -G "Unix Makefiles"	-S . -B build
	cmake --build build

run:
	cd build/ && make raytracer && ./raytracer && open out.png
