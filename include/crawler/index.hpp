#ifndef CRAWLER_INDEX_HPP
#define CRAWLER_INDEX_HPP

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <ranges>
#include <set>

namespace crawler {

static constexpr char to_hexdec(unsigned v) {
	assert(v < 16u);
	return "0123456789abcdef"[v];
}

template <size_t N> struct ngram_t: std::array<char8_t, N> {
	auto write_hexdec_into(auto it) const noexcept {
		for (unsigned byte: *this) {
			*it++ = to_hexdec(byte >> 4);
			*it++ = to_hexdec(byte & 0xFu);
		}
		return it;
	}
	std::string get_hexdec() const noexcept {
		auto output = std::string{};
		output.resize(N * 2);
		write_hexdec_into(output.begin());
		return output;
	}
	friend std::filesystem::path operator/(const std::filesystem::path & lhs, ngram_t rhs) {
		std::array<char, N * 2 + 5> tmp;
		auto it = rhs.write_hexdec_into(tmp.begin());
		*it++ = '.';
		*it++ = 'j';
		*it++ = 's';
		*it++ = 'o';
		*it++ = 'n';

		assert(it == tmp.end());

		return lhs / std::string_view(tmp.data(), tmp.size());
	}
};

struct position_t {
	uint32_t n;

	explicit constexpr position_t(uint32_t val) noexcept: n{val} { }

	constexpr friend bool operator==(position_t, position_t) noexcept = default;
	constexpr friend auto operator<=>(position_t, position_t) noexcept = default;
};

template <size_t N> struct ngram_builder_t {
	using ngram_type = ngram_t<N>;

	ngram_type data{0};
	position_t pos{0};

	constexpr explicit operator bool() const noexcept {
		return pos.n >= N;
	}

	constexpr bool push(char8_t c) noexcept {
		// rotate one left
		std::rotate(data.begin(), data.begin() + 1, data.end());
		data.back() = c;
		pos.n++;
		return static_cast<bool>(*this);
	}

	constexpr ngram_type ngram() const noexcept {
		return data;
	}

	constexpr position_t position() const noexcept {
		return position_t{static_cast<uint32_t>(pos.n - N)};
	}
};

struct link_target {
	std::string target;
};

struct document_info {
	std::string url;
	size_t ngrams{0};
	std::map<position_t, link_target> position_to_target{};

	explicit document_info(std::string url_): url{std::move(url_)} { }

	inline void add_target(position_t pos, std::string target) {
		// we want always the latest one...
		position_to_target.insert_or_assign(pos, link_target{std::move(target)});
	}
};

struct occurence_t {
	uint32_t id;
	position_t position;

	constexpr friend bool operator==(occurence_t, occurence_t) noexcept = default;
	constexpr friend auto operator<=>(occurence_t, occurence_t) noexcept = default;
};

struct leaf_t {
	std::set<occurence_t> data;
	std::vector<occurence_t> unsorted_data;

	template <size_t N> void save_to(ngram_t<N> ngram, const std::filesystem::path & prefix) {
		std::sort(unsorted_data.begin(), unsorted_data.end());

		const auto name = prefix / ngram;

		auto of = std::ofstream{name, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};

		if (!of) {
			std::cerr << "can't open: " << name << "\n";
			return;
		}
		of << "[";

		bool first = true;
		for (const occurence_t & occ: unsorted_data) {
			if (first) first = false;
			else
				of << ",";
			of << "[" << occ.id << "," << occ.position.n << "]";
		}

		of << "]";
	}
};

template <size_t N> struct index_t {
	using ngram_type = ngram_t<N>;
	using documents_type = std::vector<document_info>;

	documents_type documents{};
	std::map<ngram_type, leaf_t> leaves{};

	index_t() = default;
	index_t(index_t &&) = default;
	index_t(const index_t &) = delete;
	~index_t() noexcept = default;

	document_info & insert_document(std::string url) {
		return documents.emplace_back(std::move(url));
	}

	void insert_ngram(ngram_type ngram, position_t position, document_info & doc) {
		auto && [it, success] = leaves.try_emplace(ngram);
		it->second.unsorted_data.emplace_back((uint32_t)std::distance(documents.data(), std::addressof(doc)), position);
		++doc.ngrams;
	}

	void insert_ngram(ngram_builder_t<N> & builder, document_info & doc) {
		if (!builder) {
			return;
		}

		insert_ngram(builder.ngram(), builder.position(), doc);
	}

	void save_documents_list(const std::filesystem::path & name) const {
		auto of = std::ofstream{name, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};
		of << "[";
		bool first = true;
		for (const auto & doc: documents) {
			if (first) first = false;
			else
				of << ",";
			of << "[" << std::quoted(doc.url) << "," << doc.ngrams << "]";
		}
		of << "]";
	}

	void save_documents_targets(const std::filesystem::path & prefix) const {
		const auto beg = documents.begin();
		const auto end = documents.end();
		for (auto it = documents.begin(); it != end; ++it) {
			const auto & doc = *it;
			std::filesystem::path name = prefix / std::format("{:0>5}.json", std::distance(beg, it));
			auto of = std::ofstream{name, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};

			if (!of) {
				std::cerr << "can't open file: " << name << "\n";
				return;
			}

			of << "[";

			bool first2 = true;
			for (const auto & [position, target]: doc.position_to_target) {
				if (first2) first2 = false;
				else
					of << ",";
				of << "[";
				of << position.n << "," << std::quoted(target.target);
				of << "]";
			}
			of << "]";
		}
	}

	struct ngram_and_size_t {
		size_t size;
		ngram_type ngram;

		friend constexpr auto operator<=>(const ngram_and_size_t & lhs, const ngram_and_size_t & rhs) noexcept {
			// switch lhs and rhs intentionally
			return std::tie(rhs.size, rhs.ngram) <=> std::tie(lhs.size, lhs.ngram);
		}

		friend constexpr bool operator==(const ngram_and_size_t & lhs, const ngram_and_size_t & rhs) noexcept = default;
	};

	void save_outliers(const std::filesystem::path & name) const {
		auto of = std::ofstream{name, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};

		constexpr auto ngram_and_size = std::views::transform([](const auto & pair) {
			return ngram_and_size_t{pair.second.unsorted_data.size(), pair.first};
		});

		auto sizes = leaves | ngram_and_size | std::ranges::to<std::vector>();
		std::ranges::sort(sizes);

		bool first = true;
		of << "{";
		for (auto [size, ngram]: sizes | std::ranges::views::take(sizes.size() / 20)) { // top 5%
			if (first) first = false;
			else
				of << ",";

			of << std::quoted(ngram.get_hexdec()) << ":" << size;
		}
		of << "}";
	}

	void save_into(const std::filesystem::path & prefix) {
		auto ec = std::error_code{};
		std::filesystem::create_directories(prefix, ec);

		save_documents_list(prefix / "urls.json");

		const auto target_dir = prefix / "targets";
		std::filesystem::create_directories(target_dir, ec);

		save_documents_targets(target_dir);

		const auto leaf_dir = prefix / "leaves";
		std::filesystem::create_directories(leaf_dir, ec);

		for (auto & [ngram, leaf]: leaves) {
			leaf.save_to(ngram, leaf_dir);
		}

		save_outliers(prefix / "outliers.json");
	}
};

} // namespace crawler

#endif
