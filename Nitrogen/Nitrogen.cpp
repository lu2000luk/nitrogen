#define RESTINIO_USE_BOOST_ASIO

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <restinio/all.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/popen.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/read.hpp>

namespace asio = boost::asio;
namespace bp = boost::process;
using restinio::request_handling_status_t;
using namespace std;

static string general_args = "--no-write-thumbnail --no-cookies --no-exec --no-playlist -N 5 --cache-dir \"./cache\" --no-write-subs --embed-subs --no-embed-thumbnail --no-sponsorblock --ffmpeg-location \"./ffmpeg/\" --no-embed-metadata --no-embed-chapters --no-embed-info-json --abort-on-error --no-batch-file --windows-filenames ";
static string print_output_args = "--quiet --no-warnings --newline --print \"before_dl:STEP|Downloading %(title)s\" --print \"after_move:SUCCESS|%(filepath)s\" --print \"after_move:SUCCESS|%(filepath)s\" --progress --progress-template \"download:PROGRESS|%(progress._percent)s\" ";
static string output_template_string = "-o \"{ID}.{EXT}\" -t {EXT}";

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

struct Query {
	string key;
	string value;
};

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

vector<Query> parse_query(string query) {
	vector<Query> queries;
	auto pairs = split_string(query, '&');
	for (const auto& pair : pairs) {
		auto kv = split_string(pair, '=');
		if (kv.size() == 2) {
			queries.push_back({ kv[0], kv[1] });
		}
	}
	return queries;
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

string template_output(string id, string ext) {
	string output = output_template_string;
	size_t pos = 0;
	while ((pos = output.find("{ID}", pos)) != string::npos) {
		output.replace(pos, 4, id);
		pos += id.length();
	}
	while ((pos = output.find("{EXT}", pos)) != string::npos) {
		output.replace(pos, 5, ext);
		pos += ext.length();
	}
	return output;
}

int main(int argc, char* argv[])
{
	std::vector<std::string> args(argv + 1, argv + argc);

	int port = 3070;
	string host = "localhost";

	bool download_ffmpeg = true;
	bool download_yt_dlp = true;
	
	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "-h" || args[i] == "--help") {
			std::cout << "--port <port> --host <host> [--no-download-ffmpeg] [--no-download-yt-dlp]\n";
			return 0;
		} else if (args[i] == "--port" && i + 1 < args.size()) {
			port = stoi(args[i + 1]);
		} else if (args[i] == "--host" && i + 1 < args.size()) {
			host = args[i + 1];
		} else if (args[i] == "--no-download-ffmpeg") {
			download_ffmpeg = false;
		} else if (args[i] == "--no-download-yt-dlp") {
			download_yt_dlp = false;
		}
	}

	if (download_ffmpeg) {
		ffmpeg_setup();
	}

	if (download_yt_dlp) {
		yt_dlp_setup();
	}

	cout << "USING ARGS: " << general_args << print_output_args << endl;

	asio::io_context ioctx;
	auto work_guard = asio::make_work_guard(ioctx);
	std::thread io_thread([&ioctx]() { ioctx.run(); });
	
	cout << "Starting server on http://" << host << ":" << port << "..." << endl;
	restinio::run(
		restinio::on_thread_pool(std::thread::hardware_concurrency())
		.port(port)
		.address(host)
		.request_handler([&ioctx](restinio::request_handle_t req) {
			auto path = req->header().path();
			auto method = req->header().method();

			cout << method << " " << path << endl;

			if (method != restinio::http_method_get()) {
				return req->create_response(restinio::status_method_not_allowed())
					.append_header_date_field()
					.done();
			}

			if (path != "/download") {
				return req->create_response(restinio::status_not_found())
					.append_header_date_field()
					.done();
			}

			auto query = req->header().query();
			auto queries = parse_query(string(query));
			
			asio::co_spawn(
				ioctx,
				[&ioctx, req]() -> asio::awaitable<void> {
					auto resp = req->create_response<restinio::chunked_output_t>();

					resp.append_header(restinio::http_field::server, "Nitrogen!")
						.append_header_date_field()
						.append_header(restinio::http_field::transfer_encoding, "chunked")
						.append_header("X-Content-Type-Options", "nosniff")
						.append_header(restinio::http_field::content_type, "text/plain; charset=utf-8");
					resp.flush();

					try {
						asio::readable_pipe pipe{ ioctx };

						cout << "Spawning process..." << endl;
						auto exe = bp::v2::environment::find_executable("ping");
						bp::v2::process process(
							ioctx,
							exe,
							{ "google.com" },  // vector<string>
							bp::v2::process_stdio{ {}, pipe, {} }  // stdin=default, stdout=pipe, stderr=default
						);

						std::vector<char> buffer(4096); // memory safety is for pussies

						for (;;) {
							auto [ec, n] = co_await pipe.async_read_some(
								asio::buffer(buffer),
								asio::as_tuple(asio::use_awaitable));

							if (n > 0) {
								resp.append_chunk(std::string(buffer.data(), n));
								cout << "Read " << n << " bytes from process output." << endl;
								resp.flush();
							}

							if (ec) {
								if (ec != asio::error::eof && ec != asio::error::broken_pipe)
									resp.append_chunk("ERROR|" + ec.message());
								break;
							}
						}

						co_await process.async_wait(asio::use_awaitable);
					} catch (const std::exception& e) {
						resp.append_chunk(std::string("ERROR|") + e.what());
						resp.flush();
					}
					resp.done();
					co_return;
				},
			asio::detached);

			return restinio::request_accepted();
		}));

	return 0;
}
