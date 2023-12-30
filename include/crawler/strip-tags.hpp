#ifndef CRAWLER_STRIP_TAGS_HPP
#define CRAWLER_STRIP_TAGS_HPP

#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>

namespace crawler {

std::string_view convert_to_plain_text(std::string_view input, std::span<char> output) noexcept;
std::string convert_to_plain_text(std::string && mutable_input_output) noexcept;

struct id_and_text {
	std::string id;
	std::string text;
};

std::vector<id_and_text> convert_to_plain_text_by_nearest_anchor(std::string_view input);

} // namespace crawler

#endif
