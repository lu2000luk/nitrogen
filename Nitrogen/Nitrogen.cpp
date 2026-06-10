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
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sstream>
#include <chrono>

namespace asio = boost::asio;
namespace bp = boost::process;
using restinio::request_handling_status_t;
using namespace std;
namespace fs = std::filesystem;

static vector<string> general_args = {
	"--no-write-thumbnail",
	"--no-cookies",
	"--no-exec",
	"--no-playlist",
	"-N", "5",
	"--cache-dir", "./cache",
	"--no-write-subs",
	"--embed-subs",
	"--no-embed-thumbnail",
	"--no-sponsorblock",
	"--ffmpeg-location", "./ffmpeg/",
	"--no-embed-metadata",
	"--no-embed-chapters",
	"--no-embed-info-json",
	"--abort-on-error",
	"--no-batch-file",
	"--windows-filenames",
};

static vector<string> print_output_args = {
	"--quiet",
	"--no-warnings",
	"--newline",
	"--print", "before_dl:STEP|Downloading %(title)s",
	"--print", "after_move:SUCCESS|%(filepath)s",
	"--print", "after_move:SUCCESS|%(filepath)s",
	"--progress",
	"--progress-template", "download:PROGRESS|%(progress._percent)s",
};

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

struct DownloadedFile {
	string id;
	int downloaded_at;
};

vector<DownloadedFile> downloaded_files;

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

int get_timestamp() {
	return static_cast<int>(std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1));
}

void dl_cleanup() {
	for (auto& file : downloaded_files) {
		if (get_timestamp() - file.downloaded_at > 3600) {
			std::filesystem::remove_all("dl/" + file.id);
		}
	}
	std::erase_if(downloaded_files, [](const DownloadedFile& file) {
		return get_timestamp() - file.downloaded_at > 3600;
	});
}


static vector<string> merge_args(initializer_list<vector<string>> arg_lists) {
	vector<string> result;
	for (const auto& args : arg_lists) {
		result.insert(result.end(), args.begin(), args.end());
	}
	return result;
}

static std::string base64_decode(const std::string& in) { // https://stackoverflow.com/a/34571089
	std::string out;

	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

	int val = 0, valb = -8;
	for (char c : in) {
		if (T[c] == -1) break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0) {
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

std::string processSuccess(const std::string& input) {
	if (input.rfind("SUCCESS", 0) != 0)
		return input;

	std::vector<std::string> pipeParts;
	std::stringstream ss(input);
	std::string token;
	while (std::getline(ss, token, '|'))
		pipeParts.push_back(token);

	if (pipeParts.size() < 2)
		return input;

	std::vector<std::string> slashParts;
	std::stringstream ss2(pipeParts[1]);
	while (std::getline(ss2, token, '\\'))
		slashParts.push_back(token);

	if (slashParts.empty())
		return input;

	return "SUCCESS|" + slashParts.back();
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

vector<string> template_output(string id, string ext) {
	return { "-o", "dl/" + id + "/%(autonumber)s." + ext, "-t", ext};
}

void schedule_cleanup(asio::steady_timer& timer) {
	timer.expires_after(std::chrono::minutes(10));
	timer.async_wait([&timer](auto ec) {
		if (!ec) {
			dl_cleanup();
			schedule_cleanup(timer);
		}
		});
}

// https://stackoverflow.com/a/24315631
string ReplaceAll(string str, const string& from, const string& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
	return str;
}

int main(int argc, char* argv[])
{
	std::vector<std::string> args(argv + 1, argv + argc);

	int port = 3070;
	string host = "localhost";
	bool use_local_yt_dlp = false;

	fs::path this_path = fs::current_path();

	bool download_ffmpeg = true;
	bool download_yt_dlp = true;
	
	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "-h" || args[i] == "--help") {
			std::cout << "--port <port> --host <host> [--no-download-ffmpeg] [--no-download-yt-dlp] [--use-local-yt-dlp]\n";
			return 0;
		} else if (args[i] == "--port" && i + 1 < args.size()) {
			port = stoi(args[i + 1]);
		} else if (args[i] == "--host" && i + 1 < args.size()) {
			host = args[i + 1];
		} else if (args[i] == "--no-download-ffmpeg") {
			download_ffmpeg = false;
		} else if (args[i] == "--no-download-yt-dlp") {
			download_yt_dlp = false;
		} else if (args[i] == "--use-local-yt-dlp") {
			use_local_yt_dlp = true;
		}
	}

	if (download_ffmpeg) {
		ffmpeg_setup();
	}

	if (download_yt_dlp) {
		yt_dlp_setup();
	}

	// cleanup
	std::filesystem::remove_all("./dl/");

	asio::io_context ioctx;
	auto work_guard = asio::make_work_guard(ioctx);
	std::thread io_thread([&ioctx]() { ioctx.run(); });

	asio::steady_timer cleanup_timer(ioctx);
	schedule_cleanup(cleanup_timer);
	
	cout << "Starting server on http://" << host << ":" << port << "..." << endl;
	restinio::run(
		restinio::on_thread_pool(std::thread::hardware_concurrency())
		.port(port)
		.address(host)
		.request_handler([&ioctx, this_path, use_local_yt_dlp](restinio::request_handle_t req) {
			auto path = req->header().path();
			auto method = req->header().method();

			cout << method << " " << path << endl;

			if (method != restinio::http_method_get()) {
				return req->create_response(restinio::status_method_not_allowed())
					.append_header_date_field()
					.append_header("Access-Control-Allow-Origin", "*")
					.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
					.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
					.append_header("Access-Control-Max-Age", "86400")
					.done();
			}

			if (path == "/") {
				return req->create_response(restinio::status_ok())
					.set_body("Nitrogen!")
					.append_header_date_field()
					.append_header("Access-Control-Allow-Origin", "*")
					.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
					.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
					.append_header("Access-Control-Max-Age", "86400")
					.done();
			}

			if (path == "/resolve") {
				auto q = restinio::parse_query(req->header().query());
				auto ext = q["ext"];

				if (ext != "mp3" && ext != "mp4") {
					return req->create_response(restinio::status_bad_request())
						.append_header("Access-Control-Allow-Origin", "*")
						.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
						.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
						.append_header("Access-Control-Max-Age", "86400")
						.set_body("Invalid format. Supported formats: mp3, mp4")
						.append_header_date_field()
						.done();
				}

				if (!q.has("id") || q["id"].empty()) {
					return req->create_response(restinio::status_bad_request())
						.append_header("Access-Control-Allow-Origin", "*")
						.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
						.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
						.append_header("Access-Control-Max-Age", "86400")
						.set_body("Missing required query parameter: id")
						.append_header_date_field()
						.done();
				}

				fs::path file_path = this_path / "dl" / string(q["id"]) / ("00001."+string(ext));

				cout << "Resolving file path: " << ReplaceAll(file_path.string(), "\\\\", "\\") << endl;
				
				if (!exists(file_path)) {
					return req->create_response(restinio::status_not_found())
						.set_body("File not found (if you are 100% sure the ID is correct the file may have been deleted or has multiple parts of which the first one was a temporary artifact of yt-dlp)")
						.append_header("Access-Control-Allow-Origin", "*")
						.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
						.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
						.append_header("Access-Control-Max-Age", "86400")
						.append_header_date_field()
						.done();
				}

				return req->create_response(restinio::status_ok())
					.set_body(restinio::sendfile(ReplaceAll(file_path.string(), "\\\\", "\\")))
					.append_header_date_field()
					.append_header(restinio::http_field::content_type, (ext == "mp3") ? "audio/mpeg" : "video/mp4")
					.append_header(restinio::http_field::content_disposition, "attachment; filename=\"" + split_string(string(q["id"]), "-"[0])[0] + "." + string(ext) + "\"")
					.append_header("Access-Control-Allow-Origin", "*")
					.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
					.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
					.append_header("Access-Control-Max-Age", "86400")
					.done();
			}

			if (path != "/download") {
				return req->create_response(restinio::status_not_found())
					.append_header("Access-Control-Allow-Origin", "*")
					.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
					.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
					.append_header("Access-Control-Max-Age", "86400")
					.append_header_date_field()
					.done();
			}

			auto query = req->header().query();
			auto queries = restinio::parse_query(query);

			if (!queries.has("u") || !queries.has("m") || queries["u"].empty() || queries["m"].empty()) {
				return req->create_response(restinio::status_bad_request())
					.append_header("Access-Control-Allow-Origin", "*")
					.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
					.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
					.append_header("Access-Control-Max-Age", "86400")
					.set_body("Missing required query parameters. Required: u (URL), m (format)")
					.append_header_date_field()
					.done();
			}

			if (queries["m"] != "3" && queries["m"] != "4") { // 3 -> mp3, 4 -> mp4
				return req->create_response(restinio::status_bad_request())
					.set_body("Invalid format. Supported formats: 3 (mp3), 4 (mp4)")
					.append_header("Access-Control-Allow-Origin", "*")
					.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
					.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
					.append_header("Access-Control-Max-Age", "86400")
					.append_header_date_field()
					.done();
			}
			
			asio::co_spawn(
				ioctx,
				[&ioctx, req, use_local_yt_dlp]() -> asio::awaitable<void> {
					auto resp = req->create_response<restinio::chunked_output_t>();

					resp.append_header(restinio::http_field::server, "Nitrogen!")
						.append_header_date_field()
						.append_header(restinio::http_field::transfer_encoding, "chunked")
						.append_header("X-Content-Type-Options", "nosniff")
						.append_header("Access-Control-Allow-Origin", "*")
						.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
						.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
						.append_header("Access-Control-Max-Age", "86400")
						.append_header(restinio::http_field::content_type, "text/event-stream; charset=utf-8")
						// Cloudflare / reverse-proxy streaming headers
						.append_header(restinio::http_field::cache_control, "no-cache, no-store, must-revalidate")
						.append_header(restinio::http_field::connection, "keep-alive")
						.append_header("X-Accel-Buffering", "no");
					resp.flush();

					try {
						asio::readable_pipe pipe{ ioctx };

						bool sent_id = false;

						fs::path exe;
						if (use_local_yt_dlp) {
							exe = bp::v2::environment::find_executable("yt-dlp");
							cout << "Using local (system) yt-dlp at: " << exe << endl;
						} else {
#if _WIN32
							exe = fs::current_path() / "yt-dlp.exe";
#else
							exe = fs::current_path() / "yt-dlp";
#endif
							cout << "Using downloaded yt-dlp at: " << exe << endl;
						}

						auto queries = restinio::parse_query(req->header().query());
						string url = escape_string(base64_decode(string(queries["u"])));
						string ext = (queries["m"] == "3") ? "mp3" : "mp4";
						boost::uuids::uuid uuid = boost::uuids::random_generator()(); // https://stackoverflow.com/a/3248017
						vector<string> output_template = template_output(boost::uuids::to_string(uuid), ext);

						vector<string> args = merge_args({ general_args, print_output_args, output_template, { url } });

						cout << "Executing command: " << exe << " " << [&args]() {
							string cmd;
							for (const auto& arg : args) {
								if (arg.find(' ') != string::npos) {
									cmd += "\"" + escape_string(arg) + "\" ";
								} else {
									cmd += arg + " ";
								}
							}
							return cmd;
							}() << endl;

						cout << "Spawning process..." << endl;
						bp::v2::process process(
							ioctx,
							exe,
							args,
							bp::v2::process_stdio{ {}, pipe, {} }  // stdin=default, stdout=pipe, stderr=default
						);

						std::vector<char> buffer(4096); // memory safety is for pussies

						for (;;) {
							auto [ec, n] = co_await pipe.async_read_some(
								asio::buffer(buffer),
								asio::as_tuple(asio::use_awaitable));

							if (n > 0) {
								if (!sent_id) {
									resp.append_chunk("ID|" + boost::uuids::to_string(uuid) + "\n");
									sent_id = true;
								}

								resp.append_chunk(processSuccess(std::string(buffer.data(), n)));
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
