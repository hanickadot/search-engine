#include <crawler/strip-tags.hpp>
#include <fstream>
#include <iostream>
#include <ranges>

std::string read_stream(std::istream & input) {
	input >> std::noskipws;
	return std::string{std::istreambuf_iterator<char>{input}, {}};
}

int main(int argc, char ** argv) {
	std::string content;

	if (argc < 2) {
		content = read_stream(std::cin);
	} else {
		for (int i = 1; i != argc; ++i) {
			auto file = std::ifstream(argv[i], std::ofstream::in | std::ofstream::binary);
			content.append(read_stream(file));
		}
	}

	std::string striped_content = crawler::convert_to_plain_text(std::move(content));

	std::cout << striped_content << "\n";

	// for (auto [a, b, c]: std::views::adjacent_view<3>(striped_content)) {
	// }
}
