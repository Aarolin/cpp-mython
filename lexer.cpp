#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

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

		VALUED_OUTPUT(Number);
		VALUED_OUTPUT(Id);
		VALUED_OUTPUT(String);
		VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

		UNVALUED_OUTPUT(Class);
		UNVALUED_OUTPUT(Return);
		UNVALUED_OUTPUT(If);
		UNVALUED_OUTPUT(Else);
		UNVALUED_OUTPUT(Def);
		UNVALUED_OUTPUT(Newline);
		UNVALUED_OUTPUT(Print);
		UNVALUED_OUTPUT(Indent);
		UNVALUED_OUTPUT(Dedent);
		UNVALUED_OUTPUT(And);
		UNVALUED_OUTPUT(Or);
		UNVALUED_OUTPUT(Not);
		UNVALUED_OUTPUT(Eq);
		UNVALUED_OUTPUT(NotEq);
		UNVALUED_OUTPUT(LessOrEq);
		UNVALUED_OUTPUT(GreaterOrEq);
		UNVALUED_OUTPUT(None);
		UNVALUED_OUTPUT(True);
		UNVALUED_OUTPUT(False);
		UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

		return os << "Unknown token :("sv;
	}

	Lexer::Lexer(std::istream& input) {

		std::string buffer;
		bool is_string_now = false;
		char type_quote;

		while (input) {
			char s = input.get();
			if (buffer.empty() && s == '\n') {
				continue;
			}

			if (is_string_now) {

				if (s == '\\') {
					char next_s = input.get();

					switch (next_s) {
					case 'n':
						buffer.push_back('\n');
						break;
					case 't':
						buffer.push_back('\t');
						break;
					case '\\':
						buffer.push_back('\\');
						break;
					case 'r':
						buffer.push_back('\r');
						break;
					case '"':
						buffer.push_back('"');
						break;
					case '\'':
						buffer.push_back('\'');
						break;
					default:
						break;
					}
					continue;
				}

				buffer.push_back(s);
				if (s == type_quote) {
					is_string_now = false;
					type_quote = 0;
				}
			}
			else if (s == '\'' || s == '\"') {
				is_string_now = true;
				type_quote = s;
				buffer.push_back(s);
			}
			else if (s == '\n') {
				ParseString(buffer);
				tokens_set_.push_back(token_type::Newline{});
				buffer.clear();
			}
			else if (s == '#') {
				if (!buffer.empty()) {
					ParseString(buffer);
					tokens_set_.push_back(token_type::Newline{});
					buffer.clear();
				}
				if (input) {
					char cm = input.get();
					while (input && cm != '\n') {
						cm = input.get();
					}
				}
			}
			else {
				if (!input) {
					ParseString(buffer);
					buffer.clear();
					break;
				}
				buffer.push_back(s);
			}

		}

		if (!buffer.empty()) {
			ParseString(buffer);
		}

		if (!tokens_set_.empty()) {
			const Token& last_token = tokens_set_.back();
			if (!last_token.Is<token_type::Newline>()) {
				tokens_set_.push_back(token_type::Newline{});
			}
		}


		while (pos_new_line_ > 0) {
			tokens_set_.push_back(token_type::Dedent{});
			indents_--;
			UpdatePosNewLine();
		}

		tokens_set_.push_back(token_type::Eof{});

	}

	void Lexer::CreateToken(std::string& buffer) {

		if (buffer.empty()) {
			return;
		}

		if (buffer == "class"s) {
			tokens_set_.push_back(Token(token_type::Class{}));
		}
		else if (buffer == "def"s) {
			tokens_set_.push_back(Token(token_type::Def{}));
		}
		else if (buffer == "True"s) {
			tokens_set_.push_back(Token(token_type::True{}));
		}
		else if (buffer == "False"s) {
			tokens_set_.push_back(Token(token_type::False{}));
		}
		else if (buffer == "None"s) {
			tokens_set_.push_back(Token(token_type::None{}));
		}
		else if (buffer == "if"s) {
			tokens_set_.push_back(Token(token_type::If{}));
		}
		else if (buffer == "else"s) {
			tokens_set_.push_back(Token(token_type::Else{}));
		}
		else if (buffer == "and"s) {
			tokens_set_.push_back(Token(token_type::And{}));
		}
		else if (buffer == "or"s) {
			tokens_set_.push_back(Token(token_type::Or{}));
		}
		else if (buffer == "not"s) {
			tokens_set_.push_back(Token(token_type::Not{}));
		}
		else if (buffer == "print"s) {
			tokens_set_.push_back(Token(token_type::Print{}));
		}
		else if (buffer == "=="s) {
			tokens_set_.push_back(Token(token_type::Eq{}));
		}
		else if (buffer == "<="s) {
			tokens_set_.push_back(Token(token_type::LessOrEq{}));
		}
		else if (buffer == ">="s) {
			tokens_set_.push_back(Token(token_type::GreaterOrEq{}));
		}
		else if (buffer == "!="s) {
			tokens_set_.push_back(Token(token_type::NotEq{}));
		}
		else if (buffer == "return"s) {
			tokens_set_.push_back(Token(token_type::Return{}));
		}
		else {
			char first_symbol = buffer[0];
			if (first_symbol == '_' || is_alpha(first_symbol)) {
				tokens_set_.push_back(Token(token_type::Id{ buffer }));
			}
			else if (is_digit(buffer)) {
				tokens_set_.push_back(Token(token_type::Number{ std::stoi(buffer) }));
			}
			else if (first_symbol == '\"' || first_symbol == '\'') {
				tokens_set_.push_back(Token(token_type::String{ std::string(buffer.begin() + 1, buffer.end() - 1) }));
			}
			else {
				for (char ch : buffer) {
					tokens_set_.push_back(Token(token_type::Char{ ch }));
				}
			}
		}
	}

	void Lexer::ParseString(std::string& buffer) {

		std::string substr;
		int spaces = 0;
		int position = -1;
		bool is_string_now = false;
		char type_quote;

		for (size_t i = 0; i < buffer.size(); ++i) {
			char ch = buffer[i];
			position++;

			if (is_string_now) {

				if (ch == '\\') {
					char next_ch = buffer[i + 1];

					switch (next_ch) {
					case 'n':
						buffer.push_back('\n');
						break;
					case 't':
						buffer.push_back('\t');
						break;
					case '\\':
						buffer.push_back('\\');
						break;
					case 'r':
						buffer.push_back('\r');
						break;
					case '"':
						buffer.push_back('"');
						break;
					case '\'':
						buffer.push_back('\'');
						break;
					default:
						break;
					}
					continue;
				}

				substr.push_back(ch);
				if (ch == type_quote) {
					is_string_now = false;
					type_quote = 0;
				}
			}
			else if (ch == '\'' || ch == '\"') {
				if (i < (buffer.size() - 1)) {
					char next_ch = buffer[i + 1];
					if (next_ch != '\\') {
						is_string_now = true;
						type_quote = ch;
					}
				}

				substr.push_back(ch);
			}
			else if (is_math_symbol(ch)) {
				if (!substr.empty()) {
					CreateToken(substr);
					substr.clear();
				}
				tokens_set_.push_back(token_type::Char{ ch });
			}
			else if (ch == ' ' && substr.size() == 0) {
				spaces++;
			}
			else if (ch == ' ' && substr.size() != 0) {
				CreateToken(substr);
				substr.clear();
			}
			else if (ch == ':' || ch == '(' || ch == ')' || ch == ',' || ch == '.') {
				if (substr.size() != 0) {
					CreateToken(substr);
					substr.clear();
				}
				tokens_set_.push_back(token_type::Char{ ch });
			}
			else {
				if (substr.empty() && InsertIndent(spaces, position)) {
					while ((position - pos_new_line_) > 0) {
						tokens_set_.push_back(token_type::Indent{});
						indents_++;
						UpdatePosNewLine();
					}
				}
				else if (substr.empty() && InsertDedent(spaces, position)) {
					while ((pos_new_line_ - position) > 0) {
						tokens_set_.push_back(token_type::Dedent{});
						indents_--;
						UpdatePosNewLine();
					}
				}
				substr.push_back(ch);
			}

		}

		if (!substr.empty()) {
			CreateToken(substr);
		}

	}

	bool Lexer::InsertIndent(int spaces, int position) {
		if (spaces == position && (position - pos_new_line_) > 0) {
			return true;
		}
		return false;
	}

	bool Lexer::InsertDedent(int spaces, int position) {
		if (spaces == position && (pos_new_line_ - position) > 0) {
			return true;
		}
		return false;
	}

	void Lexer::UpdatePosNewLine() {
		pos_new_line_ = indents_ * 2;
	}

	const Token& Lexer::CurrentToken() const {
		if (tokens_set_.empty()) {
			throw std::logic_error("Not implemented"s);
		}
		return tokens_set_[curr_token_];

	}

	Token Lexer::NextToken() {
		if (tokens_set_.empty()) {
			throw std::logic_error("Not implemented"s);
		}
		if (tokens_set_.size() == curr_token_ + 1) {
			return token_type::Eof{};
		}
		curr_token_++;
		return tokens_set_[curr_token_];
	}

	bool is_alpha(char ch) {
		return std::isalpha(static_cast<unsigned char>(ch));
	}

	bool is_digit(const std::string& str) {
		return str.find_first_not_of("0123456789") == std::string::npos;
	}

	bool is_math_symbol(char ch) {
		if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
			return true;
		}
		return false;
	}

}  // namespace parse