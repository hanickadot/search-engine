#ifndef CRAWLER_STRIP_TAGS_HPP
#define CRAWLER_STRIP_TAGS_HPP

#include <span>
#include <string>
#include <string_view>

namespace crawler {

std::string_view strip_tags(std::string_view input, std::span<char> output) noexcept;

inline std::string strip_tags(std::string && mutable_input_output) noexcept {
	const auto result = strip_tags(mutable_input_output, mutable_input_output);
	// changed output was written into the string, so I can just resize it based on the size
	mutable_input_output.resize(result.size());
	return std::move(mutable_input_output);
}

} // namespace crawler

#endif
