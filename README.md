# Nitrogen

Nitrogen is a video/audio downloader based on yt-dlp.

## Structure

- `nitrogen` C++ http server that handles downloading.
- `hydrogen` Web UI based on SvelteKit

## How to build

### Nitrogen - Windows

1. Install Just command runner from https://just.systems/
2. Install Conan package manager from https://conan.io/ (make sure to do its profile auto detection but change the C++ std to 20)
3. Install Visual Studio 2026 C++ workload
4. Clone the repository
5. Open a terminal in the repository and run `just prepare`
6. Open the folder in Visual Studio and build the CMake project

### Nitrogen - Linux

1. Install Just command runner from https://just.systems/
2. Install Conan package manager from https://conan.io/ (make sure to do its profile auto detection)
3. Install build-essential, cmake and ninja
4. Clone the repository
5. Open a terminal in the repository and run `just prepare`
6. Run `just build-files-release`
7. Go to ./build/ and run `ninja` to build the project

### Hydrogen

1. Install NodeJS from https://nodejs.org/
2. Clone the repository
3. Open a terminal in `hydrogen/`
4. Run `npm install` to install dependencies
5. Run `npm run dev` to start the development server
6. Open `http://localhost:5173` in your browser to access the web UI (Change the backend URL in `src/routes/+page.svelte` if you are self hosting the backend)

## Legal

The tool is intended for personal use only. The author is not responsible for any misuse of the tool, including but not limited to copyright infringement, violation of terms of service, or any other illegal activities. The tool is only meant for downloading content of which you own the rights to. Users are expected to comply with all applicable laws and regulations when using the tool. Requesting a service takedown can be done by simply opening an issue in the repository, I will personally review the request and take action.
