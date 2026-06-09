#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <restinio/all.hpp>

using namespace std;

static string general_args = "--no-write-thumbnail --no-cookies --no-exec --no-playlist -N 5 --cache-dir \"./cache\" --no-write-subs --embed-subs --no-embed-thumbnail --no-sponsorblock --ffmpeg-location \"./ffmpeg/\" --no-embed-metadata --no-embed-chapters --no-embed-info-json --abort-on-error --no-batch-file --windows-filenames ";
static string print_output_args = "--quiet --no-warnings --newline --print \"before_dl:STEP|Downloading %(title)s\" --print \"after_move:SUCCESS|%(filepath)s\" --print \"after_move:SUCCESS|%(filepath)s\" --progress --progress-template \"download:PROGRESS|%(progress._percent)s\" ";
static string output_template = "-o \"{ID}.{EXT}\" -t {EXT}";

#if _WIN32
static string ffmpeg_tar_url = "https://huggingface.co/buckets/lu2000luk/portable/resolve/ffmpeg_win.tar?download=true";
#else
static string ffmpeg_tar_url = "https://huggingface.co/buckets/lu2000luk/portable/resolve/ffmpeg_linux.tar?download=true";
#endif


#if _WIN32
static string yt_dlp_url = "https://github.com/yt-dlp/yt-dlp/releases/download/2026.03.17/yt-dlp.exe";
#else
static string yt_dlp_url = "https://github.com/yt-dlp/yt-dlp/releases/download/2026.03.17/yt-dlp";
#endif

string escape_string(string input) {
	string escaped;
	for (char c : input) {
		if (c == '\\' || c == '\"') {
			escaped += '\\';
		}
		escaped += c;
	}
	return escaped;
}

bool exists(const std::string& name) { // https://stackoverflow.com/a/12774387
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

vector<string> split_string(string str, char delimiter) {
	vector<string> tokens;
	string token;
	for (char c : str) {
		if (c == delimiter) {
			if (!token.empty()) {
				tokens.push_back(token);
				token.clear();
			}
		} else {
			token += c;
		}
	}
	if (!token.empty()) {
		tokens.push_back(token);
	}
	return tokens;
}

void ffmpeg_setup() {
#if _WIN32
	if (!exists("./ffmpeg/ffmpeg.exe")) {
#else
	if (!exists("./ffmpeg/ffmpeg")) {
#endif
		cout << "Downloading ffmpeg..." << endl;
#if _WIN32
		string command = "curl.exe -L \"" + ffmpeg_tar_url + "\" -o ffmpeg.tar";
#else
		string command = "curl -L \"" + ffmpeg_tar_url + "\" -o ffmpeg.tar";
#endif
		system(command.c_str());

		command = "mkdir ffmpeg";
		system(command.c_str());

		command = "tar -xf ffmpeg.tar -C ./ffmpeg/";
		system(command.c_str());

#if _WIN32
		command = "del ffmpeg.tar";
#else
		command = "rm ffmpeg.tar";
#endif
		system(command.c_str());

	}
}

void yt_dlp_setup() {
#if _WIN32
	if (!exists("./yt-dlp.exe")) {	
#else
	if (!exists("./yt-dlp")) {
#endif
		cout << "Downloading yt-dlp..." << endl;
#if _WIN32
		string command = "curl.exe -L \"" + yt_dlp_url + "\" -o yt-dlp.exe";
#else
		string command = "curl -L \"" + yt_dlp_url + "\" -o yt-dlp";
#endif
		system(command.c_str());
	}
}

int main()
{
	ffmpeg_setup();
	yt_dlp_setup();

	cout << "USING ARGS: " << general_args << print_output_args << endl;
	return 0;
}
