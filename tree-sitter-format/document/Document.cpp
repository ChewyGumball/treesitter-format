#include <tree_sitter_format/document/Document.h>

#include <fstream>
#include <sstream>

#include <assert.h>

namespace {
    std::string ReadFile(const std::filesystem::path& file) {
        std::ifstream in(file);
        std::ostringstream s;
        s << in.rdbuf();

        return s.str();
    }
}

extern "C" {
const TSLanguage* tree_sitter_cpp(void);
}

namespace tree_sitter_format {
    Document::Location Document::findByteLocation(uint32_t position) const {
        assert(position < length);

        for(size_t i = 0, currentLength = 0; i < elements.size(); i++) {
            size_t nextSize = elements[i].size();

            if (currentLength + nextSize > position) {
                return Location {
                    .index = i,
                    .offset = position - currentLength,
                };
            }

            currentLength += nextSize;
        }

        assert(false);
        // We should never get here
        return Location();
    }

    size_t Document::splitAtPosition(uint32_t position) {
        assert(position <= length);

        if (position == length) {
            return elements.size();
        }

        // Find the element with the greatest start point before the split point        
        Location element = findByteLocation(position);

        // If the split is in the middle of this element, split it in two at that point.
        if (element.offset > 0) {
            std::string_view remaining = elements[element.index].substr(element.offset);
            elements.insert(elements.begin() + element.index + 1, remaining);
            elements[element.index].remove_suffix(remaining.size());

            return element.index + 1;
        } else {
            return element.index;
        }
    }
    
    const char* Document::Read(void* payload, uint32_t byte_index, [[maybe_unused]]TSPoint position_unused, uint32_t *bytes_read) {
        Document* document = (Document*)payload;

        if (byte_index >= document->length) {
            *bytes_read = 0;
            return nullptr;
        }

        Location location = document->findByteLocation(byte_index);
        std::string_view element = document->elements[location.index];
        element.remove_prefix(location.offset);

        *bytes_read = uint32_t(element.size());
        return element.data();
    }

    Document::Document(const std::filesystem::path& file) : Document(ReadFile(file)) {}
    Document::Document(const std::string&& contents) : original(std::move(contents)), elements({original}), length(original.size()), parser(ts_parser_new(), ts_parser_delete), tree(nullptr, ts_tree_delete) {       
        ts_parser_set_language(parser.get(), tree_sitter_cpp());
        tree.reset(ts_parser_parse(parser.get(), nullptr, inputReader()));
    }

    void Document::insertBefore(TSNode node, std::string_view bytes){
        Position position {
            .location = ts_node_start_point(node),
            .byteOffset = ts_node_start_byte(node),
        };

        insertBytes(position, bytes);
    }
    void Document::insertAfter(TSNode node, std::string_view bytes){
        Position position {
            .location = ts_node_end_point(node),
            .byteOffset = ts_node_end_byte(node),
        };
        
        insertBytes(position, bytes);
    }

    void Document::insertBytes(Position position, std::string_view bytes) {
        size_t insertPosition = splitAtPosition(position.byteOffset);
        elements.insert(elements.begin() + insertPosition, bytes);

        length += bytes.size();
        
        TSInputEdit edit {
            .start_byte = position.byteOffset,
            .old_end_byte = position.byteOffset,
            .new_end_byte = position.byteOffset + uint32_t(bytes.size()),
            .start_point = position.location,
            .old_end_point = position.location,
            .new_end_point = TSPoint{
                .row = position.location.row,
                .column = position.location.column + uint32_t(bytes.size()),
            },
        };

        ts_tree_edit(tree.get(), &edit);
        tree.reset(ts_parser_parse(parser.get(), tree.release(), inputReader()));
        // TODO delete old tree or no?
    }

    void Document::deleteBytes(Range range) {
        size_t deletePosition = splitAtPosition(range.start.byteOffset);
        
        // Start deleting
        size_t bytesRemaining = range.byteCount();
        while(bytesRemaining > 0) {
            // If we were asked to delete past the end of the contents, just ignore it
            if (deletePosition >= elements.size()) {
                return;
            }

            std::string_view& element = elements[deletePosition];

            size_t bytesToDelete = std::min(bytesRemaining, element.size());
            bytesRemaining -= bytesToDelete;
            length -= bytesToDelete;

            element.remove_prefix(bytesToDelete);
            if (element.empty()) {
                elements.erase(elements.begin() + deletePosition);
            } 
        }

        TSInputEdit edit {
            .start_byte = range.start.byteOffset,
            .old_end_byte = range.end.byteOffset,
            .new_end_byte = range.start.byteOffset,
            .start_point = range.start.location,
            .old_end_point = range.end.location,
            .new_end_point = range.start.location,
        };

        ts_tree_edit(tree.get(), &edit);
        tree.reset(ts_parser_parse(parser.get(), tree.release(), inputReader()));
        // TODO delete old tree or no?
    }

    const std::string& Document::originalContents() const {
        return original;
    }
    
    const std::string_view Document::originalContentsAt(Range range) const {
        return std::string_view(original).substr(range.start.byteOffset, range.byteCount());
    }
    
    std::string Document::contentsAt(Range range) const {
        std::ostringstream s;

        Location location = findByteLocation(range.start.byteOffset);

        size_t index = location.index;
        uint32_t remaining = range.byteCount();

        std::string_view element = elements[index++];
        element.remove_prefix(location.offset);
        if (range.byteCount() < element.size()) {
            element.remove_suffix(element.size() - range.byteCount());
        }
        s << element;

        remaining -= uint32_t(element.size());
        while (remaining > 0) {
            element = elements[index++];
            if (remaining > element.size()) {
                element.remove_suffix(element.size() - remaining);
            }

            s << element;
            remaining -= uint32_t(element.size());
        }

        return s.str();
    }

    
    char Document::characterAt(uint32_t bytePosition) const {
        Location location = findByteLocation(bytePosition);
        return elements[location.index][location.offset];
    }
    
    TSNode Document::root() const {
        return ts_tree_root_node(tree.get());
    }
    
    TSInput Document::inputReader() {
        return TSInput {
            .payload = this,
            .read = &Document::Read,
            .encoding = TSInputEncodingUTF8,
        };
    }

    std::ostream& operator<<(std::ostream& out, const Document& document) {
        for(const std::string_view& e : document.elements) {
            out << e;
        }

        return out;
    }

}
