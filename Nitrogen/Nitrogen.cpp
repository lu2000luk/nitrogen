#include <iostream>
#include <fstream>
#include <string>

using namespace std;

static string general_args = "--no-write-thumbnail --no-cookies --no-playlist -N 5 --cache-dir \"./cache\" --no-write-subs --embed-subs --no-embed-thumbnail --no-embed-metadata --no-embed-chapters --no-embed-info-json --abort-on-error --no-batch-file --windows-filenames ";
static string print_output_args = "--quiet --no-warnings --newline --print \"before_dl:STEP:Downloading %(title)s\" --print \"after_move:STEP:Saved to % (filepath)s\" --print \"after_move:SUCCESS:true\" --progress --progress-template \"download:PROGRESS: % (progress._percent_str)s\" ";

static string output_template = "-o \"{}.%(ext)s\"";


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

int main()
{
	cout << "Hydrogen!" << endl;
	return 0;
}
