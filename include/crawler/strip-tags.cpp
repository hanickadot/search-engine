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

struct tag_t {
	bool opening;
	bool closing;
	std::string_view name;
};

auto skip_ending_tag(std::string_view::iterator & it, const std::string_view::iterator end) noexcept -> std::optional<tag_t> {
	// sanity check
	if (it == end && *it != '/') {
		return std::nullopt;
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
		return std::nullopt;
	}

	++it;

	return tag_t{.opening = false, .closing = true, .name = tag_name};
}

bool ignore_tag_content(std::string_view name) {
	// TODO case-insensitive
	if (name == "script") {
		return true;
	} else if (name == "style") {
		return true;
	} else if (name == "svg") {
		return true;
	} else if (name == "img") {
		return true;
	} else {
		return false;
	}
}

template <typename Attribute> auto skip_tag_or_comment_inner(std::string_view::iterator & it, const std::string_view::iterator end, Attribute && attr) noexcept -> std::optional<tag_t> {
	if (it == end || *it != '<') {
		return std::nullopt;
	}

	++it;

	// comment
	if (it == end) {
		return std::nullopt;
	}

	if (*it == '!') {
		// lookahead
		auto tmp = it;
		++tmp;
		if (tmp != end && *tmp == '-') {
			if (skip_comment(it, end)) {
				return tag_t{.opening = true, .closing = true, .name = "comment"};
			} else {
				return std::nullopt;
			}
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

	const auto get_attribute_value = [&]() -> std::optional<std::string_view> {
		if (it == end) {
			return std::nullopt;
		}

		const char c = *it;
		++it;

		if (c == '"' || c == '\'') {
			// unquoted
			const auto value_begin = it;
			while (it != end && *it != c) {
				++it;
			}
			const auto value_end = it;
			// final quote
			if (it == end || *it != c) {
				return std::nullopt;
			}
			++it;
			return std::string_view{&*value_begin, static_cast<size_t>(std::distance(value_begin, value_end))};
		} else {
			// unquoted
			const auto value_begin = it;
			if (it != end && is_unqouted_value_char(*it)) {
				while (it != end && is_unqouted_value_char(*it)) {
					++it;
				}
				const auto value_end = it;
				return std::string_view{&*value_begin, static_cast<size_t>(std::distance(value_begin, value_end))};
			} else {
				return std::nullopt;
			}
		}
	};

	for (;;) {
		skip_spaces(it, end);

		if (it == end) {
			return std::nullopt;
		}

		const auto attribute_start = it;
		if (is_attribute_name_char(*it)) {
			++it;
			// skip attribute name
			while (it != end && is_attribute_name_char(*it)) {
				++it;
			}
			const auto attribute_end = it;
			const auto attribute_name = std::string_view(&*attribute_start, static_cast<size_t>(std::distance(attribute_start, attribute_end)));

			if (it != end && *it == '=') {
				++it;
				if (auto attribute_value = get_attribute_value()) {
					attr(tag_name, attribute_name, *attribute_value);
				} else {
					return std::nullopt;
				}
			}
		} else if (*it == '/') {
			// self-closing tag
			++it;
			if (it != end && *it == '>') {
				++it;
				return tag_t{.opening = true, .closing = true, .name = tag_name};
			} else {
				return std::nullopt;
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
							const auto closing_tag = skip_ending_tag(it, end);
							if (closing_tag && closing_tag->name == tag_name) {
								return tag_t{.opening = true, .closing = true, .name = tag_name};
							}
						}
					} else {
						++it;
					}
				}
			}
			return tag_t{.opening = true, .closing = false, .name = tag_name};
		} else {
			return std::nullopt;
		}
	}
}

template <typename Attribute> auto skip_tag_or_comment(std::string_view::iterator & it, const std::string_view::iterator end, Attribute && attr) noexcept -> std::optional<tag_t> {
	auto mine = it;
	auto result = skip_tag_or_comment_inner(mine, end, std::forward<Attribute>(attr));

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

std::optional<char> replacement_for_tag(const tag_t & tag) {
	if (tag.name == "li" && tag.opening) {
		return '*';
	} else if (tag.name == "br") {
		return '\n';
	} else if (tag.name == "h1" || tag.name == "h2" || tag.name == "h3" || tag.name == "h4" || tag.name == "h5") {
		if (tag.opening) {
			return '#';
		} else {
			return '\n';
		}

	} else if (tag.name == "div") {
		return '\n';
	} else if (tag.name == "p") {
		return '\n';
	} else if (tag.name == "pre") {
		return '\n';
	} else if (tag.name == "code") {
		return '`';
	} else {
		return std::nullopt;
	}
}

template <typename Insert, typename Attribute> void convert_to_plain_text_ex(std::string_view input, Insert && insert, Attribute && attr) {
	// remove <script...>...</script>
	// remove <style...>...</style>
	// other tags only remove <X>[content]</X> and keep content
	// also remove <X/>
	// remove content of all attributes

	auto it = input.begin();
	const auto end = input.end();

	auto write_character = [&, previous_space = true](char32_t c) mutable {
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

		if (c >= 'A' && c <= 'Z') {
			c = (c - 'A') + 'a';
		}

		// TODO write unicode
		insert(static_cast<char>(c));
	};

	while (it != end) {
		const char c = *it;
		if (c == '<') {
			if (auto tag = skip_tag_or_comment(it, end, std::forward<Attribute>(attr))) {
				if (auto replacement = replacement_for_tag(*tag)) {
					write_character(static_cast<char32_t>(*replacement));
				}
				continue;
			}
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
}

std::string_view crawler::convert_to_plain_text(std::string_view input, std::span<char> output) {
	auto out = output.begin();

	const auto insert_character = [&out, end = output.end()](char c) {
		assert(out < end);
		*out++ = c;
	};

	const auto attribute_callback = [](std::string_view, std::string_view, std::string_view) {
		// std::cout << "['" << key << "' -> '" << value << "']\n";
	};

	assert(input.size() <= output.size());

	convert_to_plain_text_ex(input, insert_character, attribute_callback);

	return std::string_view(output.data(), (size_t)std::distance(output.begin(), out));
}

std::string_view crawler::convert_to_plain_text(std::string_view input, std::span<char> output, std::function<void(size_t, std::string_view)> target) {
	auto out = output.begin();

	const auto insert_character = [&out, end = output.end()](char c) {
		assert(out < end);
		*out++ = c;
	};

	const auto attribute_callback = [&out, beg = output.begin(), &target](std::string_view tag, std::string_view key, std::string_view value) {
		if ((tag == "div" || tag == "span" || tag == "li") && key == "id") {
			target(static_cast<size_t>(std::distance(beg, out)), value);
		}
	};

	assert(input.size() <= output.size());

	convert_to_plain_text_ex(input, insert_character, attribute_callback);

	return std::string_view(output.data(), (size_t)std::distance(output.begin(), out));
}

std::vector<crawler::id_and_text> crawler::convert_to_plain_text_by_nearest_anchor(std::string_view input) {
	std::vector<crawler::id_and_text> output;

	output.emplace_back();

	const auto insert_character = [&output](char c) {
		// TODO improve this!!
		output.back().text.append(1u, c);
	};

	const auto attribute_callback = [&output](std::string_view tag, std::string_view key, std::string_view value) {
		if ((tag == "div" || tag == "span" || tag == "li") && key == "id") {
			output.emplace_back(id_and_text{.id = std::string(value), .text = ""});
		}
	};

	convert_to_plain_text_ex(input, insert_character, attribute_callback);

	return output;
}

std::string crawler::convert_to_plain_text(std::string && mutable_input_output) {
	const auto result = convert_to_plain_text(mutable_input_output, mutable_input_output);
	// changed output was written into the string, so I can just resize it based on the size
	assert(result.size() <= mutable_input_output.size());
	mutable_input_output.resize(result.size());
	return std::move(mutable_input_output);
}

std::string crawler::convert_to_plain_text(std::string && mutable_input_output, std::function<void(size_t, std::string_view)> target) {
	const auto result = convert_to_plain_text(mutable_input_output, mutable_input_output, target);
	// changed output was written into the string, so I can just resize it based on the size
	assert(result.size() <= mutable_input_output.size());
	mutable_input_output.resize(result.size());
	return std::move(mutable_input_output);
}
