prepare:
	conan install . --output-folder=out --build=missing

prepare-release:
	conan install . --output-folder=out --build=missing -s build_type=Release -s compiler.cppstd=20

prepare-debug:
	conan install . --output-folder=out --build=missing -s build_type=Debug -s compiler.cppstd=20