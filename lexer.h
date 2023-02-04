#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <set>

namespace parse {

    namespace token_type {

        struct Number {  // --lexeme «number»
            int value;   // number
        };

        struct Id {             // lexeme «identifier»
            std::string value;  // identifier's name
        };

        struct Char {    // --lexeme «character»
            char value;  // character's code
        };

        struct String {  // lexeme «string constant»
            std::string value;
        };

        struct Class {};    // --lexeme «class»
        struct Return {};   // --lexeme «return»
        struct If {};       // --lexeme «if»
        struct Else {};     // --lexeme «else»
        struct Def {};      // --lexeme «def»
        struct Newline {};  // --lexeme «end of line»
        struct Print {};    // --lexeme «print»
        struct Indent {};  // --lexeme «indent increase», two whitespaces
        struct Dedent {};  // --lexeme «indent decrease»
        struct Eof {};     // --lexeme «end of file»
        struct And {};     // --lexeme «and»
        struct Or {};      // --lexeme «or»
        struct Not {};     // --lexeme «not»
        struct Eq {};      // --lexeme «==»
        struct NotEq {};   // --lexeme «!=»
        struct LessOrEq {};     // --lexeme «<=»
        struct GreaterOrEq {};  // --lexeme «>=»
        struct None {};         // lexeme «None»
        struct True {};         // lexeme «True»
        struct False {};        // lexeme «False»
    }  // namespace token_type

    using TokenBase
            = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
            token_type::Class, token_type::Return, token_type::If, token_type::Else,
            token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
            token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
            token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
            token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        Token GetEqLexeme();

        Token GetCharLexeme();

        Token GetStringLexeme();

        Token GetNumberLexeme();

        Token GetIdOrKeyLexeme();

        bool IsIndentLexeme(); //

        void RemoveSpaces(); // supporting func --removes all spaces before next char in stream

        void RemoveComment(); // supporting func --removes all line after # !-- need Newline if line isn't empty

        void RemoveEmptyLines(); // supporting func --removes empty lines

        Token ParseNextToken(); // parses next token from stream

        [[nodiscard]] const Token& CurrentToken() const; // returns current token pointer or token_type::Eof, if stream ends

        Token NextToken(); // returns current token or token_type::Eof, if stream ends

        // If current token has type T, method returns its pointer
        // Else exception LexerError
        template <typename T>
        const T& Expect() const {
            using namespace std::literals;
            if (CurrentToken().Is<T>()){
                return CurrentToken().As<T>();
            } else {
                throw LexerError("Expect(): Wrong current token type"s);
            }
        }

        // If current token has type T and a value, method returns its pointer
        // Else exception LexerError
        template <typename T, typename U>
        void Expect(const U& value) const {
            using namespace std::literals;
            if (CurrentToken().Is<T>()){
                if (CurrentToken().As<T>().value != value){
                    throw LexerError("Expect(): Wrong current token type or value"s);
                }
            }
        }

        // If next token has type T, method returns its pointer
        // Else exception LexerError
        template <typename T>
        const T& ExpectNext() {
            using namespace std::literals;
            Token token = NextToken();
            if (token.Is<T>()){
                return token.As<T>();
            } else {
                throw LexerError("ExpectNext(): Wrong next token type"s);
            }
        }

        // If next token has type T and a value, method returns its pointer
        // Else exception LexerError
        template <typename T, typename U>
        void ExpectNext(const U& value) {
            using namespace std::literals;
            Token token = NextToken();
            if (token.Is<T>()){
                if (token.As<T>().value != value){
                    throw LexerError("ExpectNext(): Wrong next token type or value"s);
                }
            }
        }

    private:
        char c_{};
        std::istream& input_;
        std::string line_;
        std::vector<Token> doc_;
        int last_indent_ = 0;
        std::unordered_map<std::string, int> var_values_;
        std::set<char> lang_chars {'.', ',', '(', ')', '*', '/', '+', '-', ':', ';'};
        std::set<char> marks_chars {',', '(', ')', '*', '/', '+', '-', ':', ';'};
        std::string input_data_;
        bool dedent_chain_ = false;
    };

}  // namespace parse