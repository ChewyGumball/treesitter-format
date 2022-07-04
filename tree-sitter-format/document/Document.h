#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

// https://en.wikipedia.org/wiki/Piece_table

namespace tree_sitter_format {

class Document {
    std::string original;
    std::vector<std::string_view> elements;

    struct BytePosition {
        size_t index;
        size_t offset;
    };

    std::optional<BytePosition> findBytePosition(uint32_t position) const;
    void splitAtPosition(uint32_t position);

    friend std::ostream& operator<<(std::ostream& out, const Document& document);

    static const char* Read(void* payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read);

public:
    struct Range {
        uint32_t start;
        uint32_t end;

        Range(uint32_t start, uint32_t end) : start(start), end(end) {}
        Range(TSNode node) : start(ts_node_start_byte(node)), end(ts_node_end_byte(node)) {}

        uint32_t count() { return end - start; }
    };

    Document(const std::filesystem::path& file);
    Document(const std::string&& contents);

    void insertBytes(uint32_t position, std::string_view bytes);
    void deleteBytes(Range range);

    const std::string& originalContents() const;
    const std::string_view originalContentsAt(Range range) const;

    TSInput inputReader();
};

std::ostream& operator<<(std::ostream& out, const Document& document);

}
