#ifndef LEXER_LEXER_H_
#define LEXER_LEXER_H_

#include <string>
#include <exception>

#include "lexer/token.h"

namespace lexer {

// �ʷ�����ʱ�������쳣
class LexerException : public std::exception {
public:
	LexerException(const char* t_msg);
};

// �ʷ���������
class Lexer {
public:
	Lexer(const char* t_src);
	~Lexer() noexcept;

private:
	char NextChar() noexcept;
	void SkipChar(int count) noexcept;
	bool TestStr(const std::string& str);
	bool TestChar(char c);

public:
	Token LookAHead();
	Token NextToken();
	Token MatchToken(TokenType type);

private:
	std::string m_src;
	size_t m_idx;
	Token m_save;
	int m_line;
};

} // namespace lexer

#endif // LEXER_LEXER_H_