#include <atomic>
#include <co_curl/co_curl.hpp>
#include <co_curl/format.hpp>
#include <co_curl/url.hpp>
#include <crawler/index.hpp>
#include <crawler/strip-tags.hpp>
#include <ctre.hpp>
#include <iostream>
#include <numeric>
#include <ranges>
#include <set>
#include <string>
#include <csignal>

static std::atomic<bool> stop_flag{false};

struct content_and_mime {
	std::string content;
	std::optional<std::string> mime;
};

struct url_and_path {
	std::string url;
	std::string host;
	std::string path;

	friend constexpr bool operator==(const url_and_path & lhs, const url_and_path & rhs) noexcept {
		return lhs.url == rhs.url;
	}
	friend constexpr auto operator<=>(const url_and_path & lhs, const url_and_path & rhs) noexcept {
		return lhs.url <=> rhs.url;
	}
};

static auto get_url_and_path(const std::string & original_url) -> std::optional<url_and_path> {
	auto url = co_curl::url{original_url.c_str()}.remove_fragment().remove_query();
	auto opt_url = url.get();
	auto opt_host = url.host();
	auto opt_path = url.path();

	if (!opt_url || !opt_host || !opt_path) {
		return std::nullopt;
	}

	return url_and_path{.url = std::move(*opt_url), .host = std::move(*opt_host), .path = std::move(*opt_path)};
}

auto normalize_and_filter_links(auto && range_of_links, const std::string & original_url) {
	const auto normalize = [&](std::optional<std::string_view> in) -> std::optional<url_and_path> {
		if (!in) {
			return std::nullopt;
		}

		auto url = co_curl::url{original_url.c_str(), std::string(*in).c_str()}.remove_fragment().remove_query(); // in case of problems remove the forced scheme

		auto opt_url = url.get();
		auto opt_host = url.host();
		auto opt_path = url.path();

		if (!opt_url || !opt_host || !opt_path) {
			return std::nullopt;
		}

		if (std::string_view(original_url).starts_with("https://")) {
			if (std::string_view(*opt_url).starts_with("http://")) {
				url.set_scheme("https");
				opt_url = url.get();
				opt_host = url.host();
			}
		}

		return url_and_path{.url = std::move(*opt_url), .host = std::move(*opt_host), .path = std::move(*opt_path)};
	};

	constexpr auto nonempty = [](const std::optional<url_and_path> & in) {
		return in.has_value();
	};

	constexpr auto unwrap = [](std::optional<url_and_path> && in) {
		return std::move(*in);
	};

	return std::ranges::transform_view(std::move(range_of_links), std::move(normalize)) | std::views::filter(nonempty) | std::views::transform(unwrap);
}

auto extract_hrefs_from_html(std::string_view content, const std::string & original_url) {
	constexpr auto search_all_links = ctre::search_all<R"/((?:(?:href|src|data-src|data-original)=(?:"(?<dg>[^"]*+)"|'(?<sg>[^']*+)'| (?<space>[^ ]*+))))/">;

	constexpr auto remove_quotes = std::views::transform([](auto && in) -> std::string_view {
		if (auto content = in.template get<"dg">()) {
			return content;
		}

		if (auto content = in.template get<"sg">()) {
			return content;
		}

		if (auto content = in.template get<"space">()) {
			return content;
		}

		std::unreachable();
	});

	return normalize_and_filter_links(search_all_links(content) | remove_quotes, original_url);
}

auto extract_urls_from_doxygen_menu(std::string_view content, const std::string & original_url) {
	constexpr auto split_by_url = ctre::split<R"/(,url:")/">;

	constexpr auto remove_quotes = std::views::transform([](std::string_view in) -> std::optional<std::string> {
		auto it = in.begin();
		const auto end = in.end();

		std::ostringstream buffer;

		while (it != end) {
			if (*it == '"') {
				return buffer.str();
			} else if (*it == '\\') {
				++it;
				if (it != end) {
					// I know, but there won't be any new lines in urls probably unicode characters
					buffer << *it++;
				}
			} else {
				buffer << *it++;
			}
		}

		return std::nullopt;
	});

	return normalize_and_filter_links(split_by_url(content) | std::views::drop(1) | remove_quotes, original_url);
}

struct url_content {
	url_and_path info;
	std::string content;

	friend constexpr bool operator==(const url_content & lhs, const url_content & rhs) noexcept {
		return lhs.info == rhs.info;
	}
	friend constexpr auto operator<=>(const url_content & lhs, const url_content & rhs) noexcept {
		return lhs.info <=> rhs.info;
	}
};

std::string_view extension_by_mime(std::optional<std::string_view> mime) {
	using namespace std::string_view_literals;

	if (!mime.has_value()) {
		return ".bin"sv;
	}

	if (mime->starts_with("text/html")) {
		return ".html"sv;
	}

	return ".bin"sv;
}

bool known_extension(std::string_view path) noexcept {
	if (path.ends_with(".html") || path.ends_with(".htm")) {
		return true;
	} else if (path.ends_with(".jpg") || path.ends_with(".jpeg")) {
		return true;
	} else if (path.ends_with(".js")) {
		return true;
	} else if (path.ends_with(".css")) {
		return true;
	} else if (path.ends_with(".txt")) {
		return true;
	} else if (path.ends_with(".svg")) {
		return true;
	} else if (path.ends_with(".json")) {
		return true;
	} else if (path.ends_with(".gif")) {
		return true;
	} else if (path.ends_with(".png")) {
		return true;
	}
	return false;
}

bool blocked_extensions(std::string_view url) noexcept {
	if (url.ends_with(".gif") || url.ends_with(".png") || url.ends_with(".jpg") || url.ends_with(".jpeg") || url.ends_with(".svg") || url.ends_with(".ico")) {
		return true;
	} else if (url.ends_with(".pdf") || url.ends_with(".bin") || url.ends_with(".dat") || url.ends_with(".doc") || url.ends_with(".zip")) {
		// we can't index pdf yet
		return true;
	} else if (url.ends_with(".js") || url.ends_with(".css")) {
		return true;
	}

	return false;
}

template <size_t N = 3> auto fetch_recursive(crawler::index_t<N> & index, std::string requested_url, auto & check_link, auto & add_link, std::string referer, int attempts = 10) -> co_curl::promise<void> {
	if (!requested_url.ends_with("menudata.js")) {
		// menudata.js is doxygen generated menu
		if (blocked_extensions(requested_url)) {
			std::cerr << "skipped: " << requested_url << "\n";
			co_return;
		}
	}

	auto handle = co_curl::easy_handle{requested_url};

	std::string output;

	// handle.verbose();
	handle.follow_location();
	handle.write_into(output);
	// handle.connection_timeout(std::chrono::seconds{3});
	// handle.low_speed_timeout(64, std::chrono::seconds{1});

	for (;;) {
		auto r = co_await handle.perform();

		if (!r) {
			output.clear();

			if (attempts-- > 0) {
				// TODO co_await before trying again
				continue;
			}

			std::cerr << "failed to download: " << requested_url << " (error = " << r << ")\n";
			co_return;
		}

		if (handle.get_response_code() != co_curl::http_2XX) {
			std::cerr << "HTTP " << handle.get_response_code() << ": " << requested_url << " (referer = " << referer << ")\n";
			co_return;
		}

		break;
	}

	auto mime = handle.get_content_type();
	auto final_url = std::string{handle.url()};
	auto info = get_url_and_path(final_url);

	if (!info) {
		std::cerr << "can't get URL info for: " << final_url << "\n";
		co_return;
	}

	if (!check_link(*info)) {
		std::cerr << "post download disallowed file: " << final_url << "\n";
		co_return;
	}

	if (requested_url != final_url) {
		std::cout << "redirected: " << requested_url << " -> " << final_url << "\n";
	}

	if (mime == "text/html" || final_url.ends_with(".htm") || final_url.ends_with(".html")) {
		for (auto && url: extract_hrefs_from_html(output, final_url)) {
			add_link(*info, std::move(url));
		}
	} else if (mime == "application/javascript" && final_url.ends_with("/menudata.js")) {
		std::cout << "found doxygen menudata.js\n";
		for (auto && url: extract_urls_from_doxygen_menu(output, final_url)) {
			std::cout << url.url << "\n";
			add_link(*info, std::move(url));
		}
		co_return;
	}

	if (!info->path.empty() && info->path.back() == '/') {
		info->path.append("index").append(extension_by_mime(mime));
	} else if (!known_extension(info->path)) {
		info->path.append(extension_by_mime(mime));
	}

	bool convert = false;

	if (!mime.has_value()) {
		convert = true;

	} else if (mime->starts_with("text/html")) {
		convert = true;

	} else if (mime->starts_with("text/plain")) {
		// do nothing
	} else {
		std::cerr << "ignored: " << final_url << " (mime = " << *mime << ")\n";
		co_return;
	}

	auto & doc = index.insert_document(info->url);
	auto builder = crawler::ngram_builder_t<N>{};

	const auto start = std::chrono::high_resolution_clock::now();

	if (convert) {
		// crawle thru everything
		const auto add_section = [&](size_t pos, std::string_view target) {
			if (target.starts_with("mw-")) {
				return;
			}
			if (target == "contentSub" || target == "siteSub" || target == "content") {
				return;
			}
			doc.add_target(crawler::position_t{static_cast<uint32_t>(pos)}, std::string{target});
		};

		output = crawler::convert_to_plain_text(std::move(output), add_section);
	}

	for (char c: output) {
		if (builder.push(static_cast<char8_t>(c))) {
			index.insert_ngram(builder, doc);
		}
	}

	const auto end = std::chrono::high_resolution_clock::now();

	const auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	std::cout << info->url << " (" << dur.count() << "ms)\n";

	co_return;
}

struct url_and_referer {
	std::string url;
	std::string referer;

	friend constexpr bool operator==(const url_and_referer & lhs, const url_and_referer & rhs) noexcept {
		return lhs.url == rhs.url;
	}
	friend constexpr auto operator<=>(const url_and_referer & lhs, const url_and_referer & rhs) noexcept {
		return lhs.url <=> rhs.url;
	}
};

template <size_t N = 3> auto download_everything(std::set<std::string> urls, auto & allow) -> co_curl::promise<crawler::index_t<N>> {
	std::set<url_and_referer> urls_to_download{};
	std::set<url_and_referer> touched_urls{};

	crawler::index_t<N> index{};

	auto add_link = [&](std::string url, std::string referer) {
		urls_to_download.emplace(std::move(url), std::move(referer));
	};

	auto add_link_from_info = [&](const url_and_path & previous, url_and_path info) {
		if (previous.host != info.host) {
			return;
		}
		if (!allow(info)) {
			return;
		}
		add_link(std::move(info.url), previous.url);
	};

	for (const auto & first_url: urls) {
		add_link(first_url, ""); // start here!
	}

	auto queue = std::queue<co_curl::promise<void>>{};

	for (;;) {
		// if we have something to download...
		int throttle = 10;
		while (!urls_to_download.empty() && ((--throttle) > 0)) {
			// remove it from wait set
			auto first = urls_to_download.extract(urls_to_download.begin());
			// and mark it as to be download
			auto [position, inserted, node] = touched_urls.insert(std::move(first));
			// if it's a unique element there, actually download it
			if (inserted) {
				queue.push(fetch_recursive(index, position->url, allow, add_link_from_info, position->referer));
			}
		}

		if (!queue.empty()) {
			co_await std::move(queue.front());
			queue.pop();
		}

		if (urls_to_download.empty() && queue.empty()) {
			break;
		}
	}

	co_return std::move(index);
};

[[maybe_unused]] constexpr auto cppreference = [](const auto & path) -> bool {
	if (!path.starts_with("/w/")) {
		return false;
	}
	// if (path.starts_with("/wiki_old/")) {
	//	return false;
	// }
	// if (path.starts_with("/mwiki/")) {
	//	return false;
	// }
	// if (path.starts_with("/tmpw/")) {
	//	return false;
	// }

	if (ctre::starts_with<"/w/[A-Z][a-z0-9_]*+:">(path)) {
		return false;
	}
	return true;
};

[[maybe_unused]] constexpr auto llvm_docs = [](const auto & path) -> bool {
	if (path.starts_with("/doxygen/")) {
		return true;
	}

	if (!path.starts_with("/docs/")) {
		return false;
	}

	if (ctre::starts_with<"/docs/_(sources|images|static)/">(path)) {
		return false;
	}

	if (path.starts_with("/docs/doxygen/")) {
		return false;
	}

	return true;
};

[[maybe_unused]] constexpr auto cpp_draft = []([[maybe_unused]] const auto & path) -> bool {
	if (!path.starts_with("/c++draft/")) {
		std::cerr << "blocked: " << path << "\n";
		return false;
	}

	return true;
};

[[maybe_unused]] constexpr auto based_on_server = [](const url_and_path & info) -> bool {
	if (info.host == "llvm.org") {
		return llvm_docs(info.path);
	} else if (info.host == "en.cppreference.com") {
		return cppreference(info.path);
	} else if (info.host == "eel.is") {
		return cpp_draft(info.path);
	} else {
		return true;
	}
};

std::set<std::string> args_to_set_of_string(int argc, char ** argv) {
	std::set<std::string> urls;
	for (int i = 1; i != argc; ++i) {
		urls.emplace(std::string{argv[i]});
	}
	return urls;
}

int main(int argc, char ** argv) {
	signal(
		SIGINT, +[](int) {
		std::cerr << "requesting stop...\n";
		stop_flag = true;
		signal(SIGINT, SIG_DFL);
	});

	co_curl::get_scheduler().waiting.curl.max_total_connections(6);

	auto index = download_everything<3>(args_to_set_of_string(argc, argv), based_on_server).get();

	std::cout << "indexed documents = " << index.documents.size() << "\n";
	std::cout << "unique ngrams = " << index.leaves.size() << "\n";
	const size_t total_count = std::accumulate(index.leaves.begin(), index.leaves.end(), size_t{0}, [](size_t lhs, const auto & rhs) {
		return lhs + rhs.second.unsorted_data.size();
	});

	const size_t total_targets = std::accumulate(index.documents.begin(), index.documents.end(), size_t{0}, [](size_t lhs, const auto & rhs) {
		return lhs + rhs.position_to_target.size();
	});
	std::cout << "targets = " << total_targets << "\n";
	std::cout << "total ngrams = " << total_count << "\n";
	std::cout << "saving...\n";

	index.save_into("web/index/");

	std::cout << "done.\n";
}
