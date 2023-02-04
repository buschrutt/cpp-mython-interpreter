#include "lexer.h"

#include <algorithm>
#include <iostream>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number)
        VALUED_OUTPUT(Id)
        VALUED_OUTPUT(String)
        VALUED_OUTPUT(Char)

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class)
        UNVALUED_OUTPUT(Return)
        UNVALUED_OUTPUT(If)
        UNVALUED_OUTPUT(Else)
        UNVALUED_OUTPUT(Def)
        UNVALUED_OUTPUT(Newline)
        UNVALUED_OUTPUT(Print)
        UNVALUED_OUTPUT(Indent)
        UNVALUED_OUTPUT(Dedent)
        UNVALUED_OUTPUT(And)
        UNVALUED_OUTPUT(Or)
        UNVALUED_OUTPUT(Not)
        UNVALUED_OUTPUT(Eq)
        UNVALUED_OUTPUT(NotEq)
        UNVALUED_OUTPUT(LessOrEq)
        UNVALUED_OUTPUT(GreaterOrEq)
        UNVALUED_OUTPUT(None)
        UNVALUED_OUTPUT(True)
        UNVALUED_OUTPUT(False)
        UNVALUED_OUTPUT(Eof)

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input): input_(input) {
        NextToken(); // parse first token in constructor --may be Eof-- to avoid CurrentToken null pointer
    }

    void Lexer::RemoveSpaces() {
        while(input_.get(c_)){
            if (c_ != ' '){
                input_.unget();
                return;
            }
        }
    }

    void Lexer::RemoveComment() {
        getline(input_, line_);
    }

    void Lexer::RemoveEmptyLines() {
        int space_count = 0;
        while (input_.get(c_)){
            if (c_ == '#'){
                getline(input_, line_);
            } else {
                if (c_ != ' ' && c_ != '\n') {
                    for (int i = 0; i <= space_count; i++){
                        input_.unget();
                    }
                    return;
                } else if (c_ == '\n') {
                    space_count = 0;
                } else {
                    space_count++;
                }
            }
        }
    }

    Token Lexer::ParseNextToken() {
        if (c_ == '#') {
            RemoveComment();
            if (!CurrentToken().Is<token_type::Newline>()) {
                doc_.emplace_back(token_type::Newline{});
                return doc_.at(doc_.size() - 1);
            }
        } else if (c_ == '\n'){
            doc_.emplace_back(token_type::Newline{});
            return doc_.at(doc_.size() - 1);
        }
        if (c_ == '=' || c_ == '!' || c_ == '<' || c_ == '>') { return GetEqLexeme(); } // Get of %%%%% parses =, !, <, >, ==, !=, <=, >=,
        if (lang_chars.count(c_) > 0) { return GetCharLexeme(); } // Get of %%%%% parses =, !, <, >, ==, !=, <=, >=
        if (c_ == '\'' || c_ == '\"') { return GetStringLexeme(); }  // Get of %%%%% get string lexeme
        if (isdigit(c_)) {return GetNumberLexeme();} // Get of %%%%% get number lexeme
        if (c_ == 95 || (c_ > 64 && c_ < 91) || (c_ > 96 && c_ < 123)) {return GetIdOrKeyLexeme();} // Get of %%%%% get id lexeme
        if (c_ == ' ') {throw LexerError("ParseNextToken(): FLAG error-- "s);}
        throw LexerError("ParseNextToken(): Unknown error-- "s + c_);
    }

    const Token& Lexer::CurrentToken() const {
        return doc_.at(doc_.size() - 1);
    }

    Token Lexer::NextToken() {
        if (doc_.empty() || CurrentToken().Is<token_type::Newline>()){
            RemoveEmptyLines();
        }
        if (input_.get(c_)){
            if (c_ == ' ' && CurrentToken().Is<token_type::Newline>()) {
                if (IsIndentLexeme()) { return doc_.at(doc_.size() - 1); }
            } else if (c_ != ' ' && last_indent_ > 0 && (CurrentToken().Is<token_type::Newline>() || dedent_chain_)) {
                dedent_chain_ = true;
                input_.unget();
                last_indent_--;
                doc_.emplace_back(token_type::Dedent{});
                return doc_.at(doc_.size() - 1);
            }
            dedent_chain_ = false;
            return ParseNextToken();
        } else {
            if (last_indent_ > 0) {
                last_indent_--;
                doc_.emplace_back(token_type::Dedent{});
                return doc_.at(doc_.size() - 1);
            }
            if (!doc_.empty()){
                if (!CurrentToken().Is<token_type::Newline>() && !CurrentToken().Is<token_type::Eof>() && !CurrentToken().Is<token_type::Dedent>()){
                    doc_.emplace_back(token_type::Newline{});
                    return doc_.at(doc_.size() - 1);
                }
                if (!std::holds_alternative<token_type::Eof>(doc_.at(doc_.size() - 1))){
                    doc_.emplace_back(token_type::Eof{});
                }
            } else {
                doc_.emplace_back(token_type::Eof{});
            }
            return doc_.at(doc_.size() - 1);
        }
    }

    // Begin of %%%%% %%%%% %%%%% Get lexeme funcs %%%%% %%%%% %%%%%
    // Begin of %%%%% %%%%% %%%%% Get lexeme funcs %%%%% %%%%% %%%%%

    Token Lexer::GetEqLexeme() {
        string eq_lexeme;
        char next_c;
        input_.get(next_c);
        if (next_c == '=') {
            eq_lexeme.push_back(c_);
            eq_lexeme.push_back(next_c);
            if (eq_lexeme == "==") { doc_.emplace_back(token_type::Eq{});}
            else if (eq_lexeme == "!=") { doc_.emplace_back(token_type::NotEq{});}
            else if (eq_lexeme == "<=") { doc_.emplace_back(token_type::LessOrEq{});}
            else if (eq_lexeme == ">=") { doc_.emplace_back(token_type::GreaterOrEq{});}
        } else {
            input_.unget();
            doc_.emplace_back(token_type::Char{c_});
        }
        RemoveSpaces();
        return doc_.at(doc_.size() - 1);
    }

    Token Lexer::GetCharLexeme() {
        doc_.emplace_back(token_type::Char{c_});
        RemoveSpaces();
        return doc_.at(doc_.size() - 1);
    }

    Token Lexer::GetStringLexeme() {
        string string_lexeme;
        char end_marker = c_;
        while (input_.get(c_)) {
            if (c_ != end_marker) {
                if (c_ == '\\') {
                    input_.get(c_);
                    if (c_ == 'n') { c_ = '\n'; }
                    else if (c_ == 't') {c_ = '\t'; }
                    { string_lexeme.push_back(c_);}
                }
                else { string_lexeme.push_back(c_);}
            } else if (c_ == '\n') {
                throw LexerError("GetStringLexeme(): Wrong string format"s);
            } else {
                    doc_.emplace_back(token_type::String{string_lexeme});
                    RemoveSpaces();
                    return doc_.at(doc_.size() - 1);
            }
        }
        throw LexerError("GetStringLexeme(): Wrong string format"s);
    }

    Token Lexer::GetNumberLexeme() {
        string number_lexeme;
        number_lexeme.push_back(c_);
        while (input_.get(c_)) {
            if (c_ != ' ' && c_ != '#' && c_ != '\n' && marks_chars.count(c_) == 0) {
                if (isdigit(c_)) {
                    number_lexeme.push_back(c_);
                }
                else {
                    throw LexerError("GetNumberLexeme(): Wrong number format"s);
                }
            } else {
                break;
            }
        }
        input_.unget();
        doc_.emplace_back(token_type::Number{stoi(number_lexeme)});
        RemoveSpaces();
        return doc_.at(doc_.size() - 1);
    }

    Token Lexer::GetIdOrKeyLexeme() {
        string str_lexeme;
        str_lexeme.push_back(c_);
        while (input_.get(c_)){
            if (c_ != ' ' && c_ != '#' && c_ != '\n' && lang_chars.count(c_) == 0){
                str_lexeme.push_back(c_);
            } else {
                input_.unget();
                break;
            }
        }
        if (str_lexeme == "class") { doc_.emplace_back(token_type::Class{});}
        else if (str_lexeme == "return") { doc_.emplace_back(token_type::Return{});}
        else if (str_lexeme == "if") { doc_.emplace_back(token_type::If{});}
        else if (str_lexeme == "else") { doc_.emplace_back(token_type::Else{});}
        else if (str_lexeme == "def") { doc_.emplace_back(token_type::Def{});}
        else if (str_lexeme == "print") { doc_.emplace_back(token_type::Print{});}
        else if (str_lexeme == "and") { doc_.emplace_back(token_type::And{});}
        else if (str_lexeme == "or") { doc_.emplace_back(token_type::Or{});}
        else if (str_lexeme == "not") { doc_.emplace_back(token_type::Not{});}
        else if (str_lexeme == "None") { doc_.emplace_back(token_type::None{});}
        else if (str_lexeme == "True") { doc_.emplace_back(token_type::True{});}
        else if (str_lexeme == "False") { doc_.emplace_back(token_type::False{});}
        else {doc_.emplace_back(token_type::Id{str_lexeme});}
        RemoveSpaces();
        return doc_.at(doc_.size() - 1);
    }

    bool Lexer::IsIndentLexeme() {
        int indent_count = 1;
        while (input_.get(c_)){
            if (c_ == ' ') {
                indent_count++;
                if (indent_count / 2 - 1 == last_indent_) {
                    last_indent_++;
                    doc_.emplace_back(token_type::Indent{});
                    return true;
                }
            } else {
                if (indent_count / 2 == last_indent_) {
                    return false;
                } else {
                    if (indent_count / 2 + 1 != last_indent_){
                        for (int i = 0; i < 2; i++) {
                            input_.unget();
                        }
                    }
                    input_.unget();
                    last_indent_--;
                    doc_.emplace_back(token_type::Dedent{});
                    return true;
                }
            }
        }
        return false;
    }
    // End of %%%%% %%%%% %%%%% Get lexeme funcs %%%%% %%%%% %%%%%
    // End of %%%%% %%%%% %%%%% Get lexeme funcs %%%%% %%%%% %%%%%

}  // namespace parse