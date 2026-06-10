prepare:
	conan install . --output-folder=out --build=missing

prepare-release:
	conan install . --output-folder=out --build=missing -s build_type=Release -s compiler.cppstd=20

prepare-debug:
	conan install . --output-folder=out --build=missing -s build_type=Debug -s compiler.cppstd=20

build-files-release:
	cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=out/build/Release/generators/conan_toolchain.cmake -G Ninja