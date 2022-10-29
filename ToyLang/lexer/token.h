#ifndef LEXER_TOKEN_H_
#define LEXER_TOKEN_H_

#include <string>

namespace lexer {

// token���ͳ���
enum class TokenType {
	kNil = 0,

	kEof,
	kNumber,

	kOpAdd,    // +
	kOpSub,    // -
	kOpMul,    // *
	kOpDiv,    // /

	kSepLParen,  // (
	kSepRParen,  // )
};

// ����token�Ľṹ��
struct Token {
	bool Is(TokenType t_type) const noexcept;

	int line;			// �к�
	TokenType type;		// token����
	std::string str;	// �����Ҫ����Ϣ
};

} // namespace lexer

#endif // LEXER_TOKEN_H_