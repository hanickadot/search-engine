#include "strip-tags.hpp"
#include <iostream>
#include <optional>
#include <cassert>

template <typename T = std::string_view> struct reader_t {
	using type = T;
	typename T::iterator it;
	typename T::iterator end;

	explicit constexpr reader_t(type in) noexcept: it{std::ranges::begin(in)}, end{std::ranges::end(in)} { }
};

template <typename T = std::span<char>> struct writer_t {
	using type = T;
	typename T::iterator it;
	typename T::iterator end;

	explicit constexpr writer_t(type out) noexcept: it{std::ranges::begin(out)}, end{std::ranges::end(out)} { }
};

struct reader_writer_t {
	using reader_type = reader_t<>;
	using writer_type = writer_t<>;

	reader_type reader;
	writer_type writer;

	constexpr reader_writer_t(typename reader_type::type input, typename writer_type::type output) noexcept: reader{input}, writer{output} { }
};

bool skip_comment(std::string_view::iterator & it, const std::string_view::iterator end) noexcept {
	// sanity check
	if (it == end || *it != '!') {
		return false;
	}
	++it;
	if (it == end || *it != '-') {
		return false;
	}
	++it;
	if (it == end || *it != '-') {
		return false;
	}
	++it;
	// content of comment

	enum comment_state_t {
		normal,
		one,
		two
	} comment_state{normal};

	while (it != end) {
		char c = *it++;

		if (comment_state == normal) {
			if (c == '-') {
				comment_state = one;
				continue;
			}
		} else if (comment_state == one) {
			if (c == '-') {
				comment_state = two;
				continue;
			}
		} else if (comment_state == two) {
			if (c == '-') {
				continue;
			} else if (c == '>') {
				return true;
			}
		}

		comment_state = normal;
	}
	return false;
}

constexpr auto is_tag_name_char(char c) {
	// TODO make it table
	if (c >= '0' && c <= '9') {
		return true;
	} else if (c >= 'a' && c <= 'z') {
		return true;
	} else if (c >= 'A' && c <= 'Z') {
		return true;
	} else if (c == '-') {
		return true;
	}
	return false;
};

void skip_spaces(std::string_view::iterator & it, const std::string_view::iterator end) {
	// skip spaces
	while (it != end && *it == ' ') {
		++it;
	}
}

bool skip_ending_tag(std::string_view::iterator & it, const std::string_view::iterator end, std::optional<std::string_view> expected_tag = std::nullopt) noexcept {
	// sanity check
	if (it == end && *it != '/') {
		return false;
	}

	++it;

	const auto begin_tag_name = it;
	while (it != end && is_tag_name_char(*it)) {
		++it;
	}
	const auto end_tag_name = it;
	const auto tag_name = std::string_view{&*begin_tag_name, static_cast<size_t>(end_tag_name - begin_tag_name)};

	skip_spaces(it, end);

	if (it == end || *it != '>') {
		return false;
	}

	++it;

	if (expected_tag.has_value()) {
		// std::cout << "[" << *expected_tag << " VS " << tag_name << "]";
		return *expected_tag == tag_name;
	} else {
		return true;
	}
}

bool ignore_tag_content(std::string_view name) {
	// TODO case-insensitive
	if (name == "script") {
		return true;
	} else if (name == "style") {
		return true;
	} else if (name == "img") {
		return true;
	} else {
		return false;
	}
}

bool skip_tag_or_comment_inner(std::string_view::iterator & it, const std::string_view::iterator end) noexcept {
	if (it == end || *it != '<') {
		return false;
	}

	++it;

	// comment
	if (it == end) {
		return false;
	}

	if (*it == '!') {
		// lookahead
		auto tmp = it;
		++tmp;
		if (tmp != end && *tmp == '-') {
			return skip_comment(it, end);
		}
	}

	// ending tag
	if (*it == '/') {
		return skip_ending_tag(it, end);
	}

	// read tag name
	const auto begin_tag_name = it;

	// skip tag name
	while (it != end && is_tag_name_char(*it)) {
		++it;
	}

	const auto end_tag_name = it;
	const auto tag_name = std::string_view{&*begin_tag_name, static_cast<size_t>(end_tag_name - begin_tag_name)};

	// read attributes...
	constexpr auto is_attribute_name_char = [](char c) {
		// TODO table
		if (c == '\0') {
			return false;
		} else if (c == '"') {
			return false;
		} else if (c == '\'') {
			return false;
		} else if (c == '>') {
			return false;
		} else if (c == '/') {
			return false;
		} else if (c == '=') {
			return false;
		} else {
			return true;
		}
	};

	constexpr auto is_unqouted_value_char = [&](char c) {
		// TODO make it table
		if (c == '"') {
			return false;
		} else if (c == '\'') {
			return false;
		} else if (c == '=') {
			return false;
		} else if (c == '<') {
			return false;
		} else if (c == '>') {
			return false;
		} else if (c == '`') {
			return false;
		} else {
			return true;
		}
	};

	const auto skip_attribute_value = [&] {
		if (it == end) {
			return false;
		}

		const char c = *it;
		++it;

		if (c == '"' || c == '\'') {
			// unquoted
			while (it != end && *it != c) {
				++it;
			}
			// final quote
			if (it == end || *it != c) {
				return false;
			}
			++it;
			return true;
		} else {
			// unquoted
			if (it != end && is_unqouted_value_char(*it)) {
				while (it != end && is_unqouted_value_char(*it)) {
					++it;
				}
				return true;
			} else {
				return false;
			}
		}
	};

	for (;;) {
		skip_spaces(it, end);

		if (it == end) {
			return false;
		}

		if (is_attribute_name_char(*it)) {
			++it;
			// skip attribute name
			while (it != end && is_attribute_name_char(*it)) {
				++it;
			}

			if (it != end && *it == '=') {
				++it;
				if (!skip_attribute_value()) {
					return false;
				}
			}
		} else if (*it == '/') {
			// self-closing tag
			++it;
			if (it != end && *it == '>') {
				++it;
				return true;
			} else {
				return false;
			}
		} else if (*it == '>') {
			// opening tag
			++it;
			if (ignore_tag_content(tag_name)) {
				while (it != end) {
					if (*it == '<') {
						++it;
						if (it != end && *it == '/') {
							// std::cout << "[ending tag?]";
							if (skip_ending_tag(it, end, tag_name)) {
								return true;
							}
						}
					} else {
						++it;
					}
				}
			}
			return true;
		} else {
			return false;
		}
	}
	(void)begin_tag_name;
	(void)end_tag_name;
}

bool skip_tag_or_comment(std::string_view::iterator & it, const std::string_view::iterator end) noexcept {
	auto mine = it;
	const bool result = skip_tag_or_comment_inner(mine, end);

	if (result) {
		it = mine;
	}

	return result;
}

constexpr bool is_digit(char c) noexcept {
	return c >= '0' && c <= '9';
}

constexpr bool is_alpha(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr auto convert_hexdec_digit(char c) noexcept -> std::optional<unsigned> {
	if (c >= '0' && c <= '9') {
		return static_cast<unsigned>(c - '0');
	} else if (c >= 'a' && c <= 'f') {
		return static_cast<unsigned>(c - 'a') + 10u;
	} else if (c >= 'A' && c <= 'F') {
		return static_cast<unsigned>(c - 'A') + 10u;
	} else {
		return std::nullopt;
	}
}

auto read_entity_inner(std::string_view::iterator & it, const std::string_view::iterator end) noexcept -> std::optional<char32_t> {
	if (it == end || *it != '&') {
		return std::nullopt;
	}
	++it;

	if (it == end) {
		return std::nullopt;
	}

	char32_t code = 0;

	if (*it == '#') {
		++it;
		if (it != end && *it == 'x') {
			++it;
			while (it != end) {
				if (const auto value = convert_hexdec_digit(*it)) {
					++it;
					code = (code * 16u) + *value;
				} else {
					break;
				}
			}
		} else {
			while (it != end && is_digit(*it)) {
				code = (code * 10u) + static_cast<unsigned>((*it++) - '0');
			}
		}

	} else if (is_alpha(*it)) {
		[[maybe_unused]] const auto name_start = it;
		++it;
		while (it != end && (is_alpha(*it) || is_digit(*it))) {
			++it;
		}
		[[maybe_unused]] const auto name_end = it;
		const auto name = std::string_view(&*(name_start), static_cast<size_t>(std::distance(name_start, name_end)));

		if (name == "lt") {
			code = '<';
		} else if (name == "gt") {
			code = '>';
		} else if (name == "nbsp" || name == "ensp" || name == "emsp") {
			code = ' ';
		} else if (name == "amp") {
			code = '&';
		} else {
			return std::nullopt;
		}
	}

	if (it == end || *it != ';') {
		return std::nullopt;
	}

	++it;

	return code;
}

auto read_entity(std::string_view::iterator & it, const std::string_view::iterator end) noexcept -> std::optional<char32_t> {
	auto mine = it;
	auto result = read_entity_inner(mine, end);

	if (result) {
		it = mine;
	}

	return result;
}

std::string_view crawler::strip_tags(std::string_view input, std::span<char> output) noexcept {
	// remove <script...>...</script>
	// remove <style...>...</style>
	// other tags only remove <X>[content]</X> and keep content
	// also remove <X/>
	// remove content of all attributes

	auto it = input.begin();
	const auto end = input.end();

	auto out = output.begin();
	[[maybe_unused]] const auto oend = output.end();

	assert(input.size() <= output.size());

	auto write_character = [&, previous_space = false](char32_t c) mutable {
		assert(out != oend);

		// TODO process unicode properly
		if (c == 0x200b) {
			return;
			//} else if (c == '“' || c == '”') {
			//	c = '"';
			//} else if (c == '“' || c == '”') {
			//	c = '"';
		} else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (previous_space) {
				return;
			}
			c = ' ';
			previous_space = true;
		} else {
			previous_space = false;
		}

		// TODO write unicode
		*out = static_cast<char>(c);
		++out;
	};

	while (it != end) {
		const char c = *it;
		if (c == '<' && skip_tag_or_comment(it, end)) {
			assert(out != oend);
			// write_character(' ');
			continue;
		} else if (c == '&') {
			if (auto entity = read_entity(it, end)) {
				write_character(*entity);
				continue;
			}
		}

		// TODO read unicode properly
		write_character(static_cast<char32_t>(static_cast<unsigned char>(c)));
		++it;
	}

	return std::string_view(output.data(), (size_t)std::distance(output.begin(), out));
}
