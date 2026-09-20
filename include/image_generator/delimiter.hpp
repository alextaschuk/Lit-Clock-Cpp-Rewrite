#pragma once

#include <string>
#include <vector>

enum class DelimiterType
{
    Italic,
    Bold,
    Time,
    EndOfLine,
    Count // only used to track number of delimiter types
};


// Defines a formatting delimiter and a variable to track how many of the delimiter have been seen in the text.
//
// type: The type of formatting to be applied to the wrapped text.
// character: The character to search for in the text to apply a specific formatting with (e.g. "◻").
// count: How many times the character has been seen in the text.
struct Delimiter // TODO: make a class?
{
    DelimiterType type;
    std::string character;
    int count = 0;

    Delimiter(DelimiterType t, const std::string& c) : type(t), character(c) {}
};

// Stores all delimiting characters to format one or more characters.
//
// ITALIC: One Text wrapped with this delimiter is written using an italicized version of the font.
// BOLD: Text wrapped with this delimiter is written using a bolded version of the font.
//  Note: This can be combined with the `ITALIC` delimiter to write text that is bold and italic.
// TIMESTR: The timestring part of the quote is automatically wrapped with this delimiter.
//  Note: A timestring should never be manually wrapped in the quote CSV file because it is
//      automatically wrapped when a quote is drawn.
struct CharacterDelimiters {
    std::string ITALIC  = "_";    // U+005F (Low Line)
    std::string BOLD    = "*";    // U+002A (Asterisk)
    std::string TIMESTR = "|";    // U+007C (Vertical Line)
    std::string ENDOFLINE = "\n"; // U+000A (End of Line)

    std::vector<std::string> getCharDelims() const {
        return {ITALIC, BOLD, TIMESTR, ENDOFLINE };
    }
};
