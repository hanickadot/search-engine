#ifndef CRAWLER_STRIP_TAGS_HPP
#define CRAWLER_STRIP_TAGS_HPP

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>

namespace crawler {

std::string_view convert_to_plain_text(std::string_view input, std::span<char> output);
std::string_view convert_to_plain_text(std::string_view input, std::span<char> output, std::function<void(size_t, std::string_view)> target);

std::string convert_to_plain_text(std::string && mutable_input_output);
std::string convert_to_plain_text(std::string && mutable_input_output, std::function<void(size_t, std::string_view)> target);

struct id_and_text {
	std::string id;
	std::string text;
};

std::vector<id_and_text> convert_to_plain_text_by_nearest_anchor(std::string_view input);

} // namespace crawler

#endif
