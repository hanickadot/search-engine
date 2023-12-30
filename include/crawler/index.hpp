#ifndef CRAWLER_INDEX_HPP
#define CRAWLER_INDEX_HPP

#include <array>
#include <map>

namespace crawler {

template <size_t N> struct ngram_t: std::array<char8_t, N> { };

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

	constexpr void push(char8_t c) noexcept {
		// rotate one left
		std::rotate(data.begin(), data.begin() + 1, data.end());
		data.back() = c;
		pos.n++;
	}

	constexpr ngram_type ngram() const noexcept {
		return data;
	}

	constexpr position_t position() const noexcept {
		return pos - N;
	}

	constexpr explicit operator bool() const noexcept {
		return pos >= N;
	}
};

struct link_target {
	std::string target;
};

struct document_info {
	inline document_info(std::string url_) noexcept: url{std::move(url_)} { }

	std::string url;
	std::map<position_t, target> position_to_target;
	unsigned id{0};

	inline void add_target(position pos, std::string target) noexcept {
		// we want always the latest one...
		position_to_target.insert_or_assign(line, std::move(target));
	}
};

struct occurence_t {
	uint32_t position;
	const document_info & document;
};

struct leaf_t {
	std::set<occurence_t> set;
};

template <size_t N> struct index_t {
	using ngram_type = ngram_t<N>;

	std::map<position_t, document_info> documents;
	std::map<ngram_type, leaf_t> leafs;

	index_t() = default;
	index_t(index_t &&) = default;
	index_t(const index_t &) = delete;
	~index_t() noexcept = default;

	document_info * insert_document(std::string url) {
		auto && [it, success] = documents.emplace(std::move(url));
		if (!success) {
			return nullptr;
		}
		return std::addressof(it->second);
	}

	void insert_ngram(ngram_type ngram, position_t position, const document_info & doc) {
		auto && [it, success] leafs.try_emplace(ngram);
		it->second.set.emplace(position, doc);
	}

	void insert_ngram(ngram_builder_t & builder, const document_info & doc) {
		if (!builder) {
			return;
		}

		insert_ngram(builder.ngram(), builder.position(), doc);
	}
};

} // namespace crawler

#endif
