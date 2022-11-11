#include <tree-sitter-format/Util.h>

#include <tree-sitter-format/Constants.h>

#include <array>
#include <unordered_map>
#include <assert.h>

namespace tree_sitter_format {

[[nodiscard]] TSNode NullNode() {
    static TSNode node = {
        {0, 0, 0, 0},
        nullptr,
        nullptr,
    };

    return node;
}

std::string_view ChildFieldName(TSNode node, uint32_t childIndex) {
    // There is currently a bug where this does not return the right field name
    // in all cases: https://github.com/tree-sitter/tree-sitter/issues/1642
    //
    // In our case, this is most noticable when comments are inserted between
    // fields.
    //
    // IE: for(auto i : c) // help
    //          i++;
    // 
    //      labels the "//help" comment node as the body, and will crash
    //      when querying the field name of the "i++;" child.
    //
    //  We have a locally modified tree_sitter library that fixes this issue.
    const char* name = ts_node_field_name_for_child(node, childIndex);
    if(name == nullptr) {
        return std::string_view();
    } else {
        return name;
    }
}

[[nodiscard]] TSNode FindFirstNonExtraChild(TSNode node, uint32_t startingIndex) {
    uint32_t childCount = ts_node_child_count(node);

    TSNode child = ts_node_child(node, startingIndex++);
    while(ts_node_is_extra(child) && startingIndex < childCount) {
        child = ts_node_child(node, startingIndex++);
    }

    if (ts_node_is_extra(child)) {
        return NullNode();
    }

    return child;
}

[[nodiscard]] TSNode FindLastNonExtraChild(TSNode node, uint32_t startingIndex) {
    TSNode child = ts_node_child(node, startingIndex);
    if (!ts_node_is_extra(child)) {
        return child;
    }

    if (startingIndex == 0) {
        return NullNode();
    }

    do {
        child = ts_node_child(node, startingIndex);
    } while(ts_node_is_extra(child) && --startingIndex > 0);

    if (!ts_node_is_extra(child)) {
        return child;
    }

    if (startingIndex == 0) {
        child = ts_node_child(node, startingIndex);
        if (!ts_node_is_extra(child)) {
            return child;
        }
    }

    return NullNode();
}

[[nodiscard]] bool IsCompoundStatementLike(TSNode node) {
    TSSymbol symbol = ts_node_symbol(node);

    return symbol == COMPOUND_STATEMENT ||
           symbol == DECLARATION_LIST ||
           symbol == FIELD_DECLARATION_LIST;
}

[[nodiscard]] bool IsCaseWithSingleStatementBody(TSNode node) {
    TSSymbol symbol = ts_node_symbol(node);

    if (symbol == CASE_STATEMENT) {
        uint32_t childCount = ts_node_child_count(node);
        bool isDefaultCase = !ts_node_is_named(ts_node_child(node, 1));

        return childCount == (isDefaultCase ? 3u : 4u);
    }

    return false;
}
[[nodiscard]] bool IsDeclarationLike(TSNode node) {
    TSSymbol symbol = ts_node_symbol(node);
    return symbol == DECLARATION ||
            symbol == FIELD_DECLARATION;
}

[[nodiscard]] bool IsIdentifierLike(TSNode node) {
    TSSymbol symbol = ts_node_symbol(node);
    return symbol == IDENTIFIER ||
            symbol == FIELD_IDENTIFIER;
}

[[nodiscard]] bool IsAssignmentLike(TSNode node) {
    TSSymbol symbol = ts_node_symbol(node);
    if (symbol == EXPRESSION_STATEMENT) {
        // This is an empty statement terminated by a semicolon
        if (ts_node_child_count(node) == 1) {
            return false;
        }

        TSNode child = ts_node_child(node, 0);
        return ts_node_symbol(child) == ASSIGNMENT_EXPRESSION;
    } else if (symbol == DECLARATION) {
            TSNode firstDeclarator = FindFirstNonExtraChild(node, 1);
            assert(!ts_node_is_null(firstDeclarator));
            return ts_node_symbol(firstDeclarator) == INIT_DECLARATOR;
    } else if (symbol == FIELD_DECLARATION) {
        TSNode defaultValue = ts_node_child_by_field_name(node, "default_value", 13);
        return !ts_node_is_null(defaultValue);
    }

    return false;
}

[[nodiscard]] bool IsBitfieldDeclaration(TSNode node) {
    if (ts_node_symbol(node) != FIELD_DECLARATION) {
        return false;
    }
    
    uint32_t childCount = ts_node_child_count(node);
    for(uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);

        if (ts_node_symbol(child) == BITFIELD_CLAUSE) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] std::string_view GetSpaces(uint32_t count) {
    static std::unordered_map<uint32_t, std::string> spaces;

    if (!spaces.contains(count)) {
        spaces[count] = std::string(count, ' ');
    }

    return spaces[count];
}


[[nodiscard]] bool IsWhitespace(char32_t character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

[[nodiscard]] bool IsDigit(char32_t character) {
    return character == '0' ||
           character == '1' ||
           character == '2' ||
           character == '3' ||
           character == '4' ||
           character == '5' ||
           character == '6' ||
           character == '7' ||
           character == '8' ||
           character == '9';
}

[[nodiscard]] bool IsPunctuation(char32_t character) {
    static std::array<char, 32> PunctuationCharacters = {
        '`', '~', '!', '@', '#', '$', '%',
        '^', '&', '*', '(', ')', '-', '_',
        '=', '+', '[', '{', ']', '}', '\\',
        '|', ';', ':', '\'', '"', ',', '<',
        '.', '>', '/', '?'
    };

    for(char c : PunctuationCharacters) {
        if (character == char32_t(c)) {
            return true;
        }
    }

    return false;
}

}