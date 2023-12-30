#include <atomic>
#include <co_curl/co_curl.hpp>
#include <co_curl/format.hpp>
#include <co_curl/url.hpp>
#include <crawler/strip-tags.hpp>
#include <ctre.hpp>
#include <iostream>
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

auto extract_hrefs(std::string_view content, const std::string & original_url) {
	constexpr auto remove_quotes = [](const ctre::capture_groups auto & groups) -> std::string_view {
		return groups.template get<1>();
	};

	const auto normalize = [&original_url](std::string_view in) -> std::optional<url_and_path> {
		auto url = co_curl::url{original_url.c_str(), std::string(in).c_str()}.remove_fragment().remove_query();
		auto opt_url = url.get();
		auto opt_host = url.host();
		auto opt_path = url.path();

		if (!opt_url || !opt_host || !opt_path) {
			return std::nullopt;
		}

		return url_and_path{.url = std::move(*opt_url), .host = std::move(*opt_host), .path = std::move(*opt_path)};
	};

	constexpr auto nonempty = [](const std::optional<url_and_path> & in) {
		return in.has_value();
	};

	constexpr auto unwrap = [](std::optional<url_and_path> && in) {
		return std::move(*in);
	};

	return ctre::split<R"/((?:href|src|data-src|data-original)=(?:["']([^"']*+)["']))/">(content) | std::views::transform(remove_quotes) | std::views::transform(std::move(normalize)) | std::views::filter(nonempty) | std::views::transform(unwrap);
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
	} else if (path.ends_with(".md")) {
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

auto fetch_recursive(std::string requested_url, auto & check_link, auto & add_link, std::string referer, int attempts = 10) -> co_curl::promise<std::optional<url_content>> {
	auto handle = co_curl::easy_handle{requested_url};

	std::string output;

	// handle.verbose();
	handle.follow_location();
	handle.write_into(output);
	handle.connection_timeout(std::chrono::seconds{2});
	handle.low_speed_timeout(100, std::chrono::seconds{1});

	for (;;) {
		auto r = co_await handle.perform();

		if (!r) {
			output.clear();

			if (attempts-- > 0) {
				continue;
			}

			std::cerr << "failed to download: " << requested_url << " (error = " << r << ")\n";
			co_return std::nullopt;
		}

		if (handle.get_response_code() != co_curl::http_2XX) {
			std::cerr << "HTTP " << handle.get_response_code() << ": " << requested_url << " (referer = " << referer << ")\n";
			co_return std::nullopt;
		}

		break;
	}

	auto mime = handle.get_content_type();
	auto final_url = std::string{handle.url()};
	auto info = get_url_and_path(final_url);

	if (!info) {
		std::cerr << "can't get URL info for: " << final_url << "\n";
		co_return std::nullopt;
	}

	if (!check_link(*info)) {
		std::cerr << "post download disallowed file: " << final_url << "\n";
		co_return std::nullopt;
	}

	if (requested_url != final_url) {
		std::cout << "redirected: " << requested_url << " -> " << final_url << "\n";
	} else {
		std::cout << "downloaded: " << final_url << "\n";
	}

	for (auto && url: extract_hrefs(output, final_url)) {
		add_link(*info, std::move(url));
	}

	if (!info->path.empty() && info->path.back() == '/') {
		info->path.append("index").append(extension_by_mime(mime));
	} else if (!known_extension(info->path)) {
		info->path.append(extension_by_mime(mime));
	}

	co_return url_content{.info = std::move(*info), .content = crawler::convert_to_plain_text(std::move(output))};
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

auto download_everything(std::string first_url, auto & allow) -> co_curl::promise<std::set<url_content>> {
	std::set<url_and_referer> urls_to_download{};
	std::set<url_and_referer> touched_urls{};
	std::set<url_content> results{};

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

	add_link(first_url, ""); // start here!

	auto queue = std::queue<co_curl::promise<std::optional<url_content>>>{};

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
				queue.push(fetch_recursive(position->url, allow, add_link_from_info, position->referer));
			}
		}

		if (!queue.empty()) {
			auto result = co_await std::move(queue.front());

			if (result.has_value()) {
				results.emplace(std::move(*result));
			}

			queue.pop();
		}

		if (urls_to_download.empty() && queue.empty()) {
			break;
		}
	}

	co_return std::move(results);
};

constexpr auto allow_everything = [](const auto &) {
	return true;
};

auto download_everything(std::string first_url) -> co_curl::promise<std::set<url_content>> {
	return download_everything(std::move(first_url), allow_everything);
}

[[maybe_unused]] constexpr auto cppreference = [](const auto & path) -> bool {
	if (path.starts_with("/wiki_old/")) {
		return false;
	}
	if (path.starts_with("/mwiki/")) {
		return false;
	}
	if (path.starts_with("/tmpw/")) {
		return false;
	}
	if (ctre::starts_with<"/w/[A-Z][a-z0-9_]*+:">(path)) {
		return false;
	}
	return true;
};

[[maybe_unused]] constexpr auto llvm_docs = [](const auto & path) -> bool {
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

int main(int argc, char ** argv) {
	if (argc < 2) {
		return 1;
	}

	signal(
		SIGINT, +[](int) {
		std::cerr << "requesting stop...\n";
		stop_flag = true;
		signal(SIGINT, SIG_DFL);
	});

	const std::string url = argv[1];
	auto files = download_everything(url, based_on_server).get();

	std::cout << "files count = " << files.size() << "\n";

	size_t total_size = 0;
	for (const auto & item: files) {
		total_size += item.content.size();
	}

	std::cout << "downloaded: " << co_curl::data_amount{total_size} << "\n";
}
