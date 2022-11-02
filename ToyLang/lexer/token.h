#ifndef LEXER_TOKEN_H_
#define LEXER_TOKEN_H_

#include <string>
#include <map>


namespace lexer {

// token���ͳ���
enum class TokenType {
	kNil = 0,

	kEof,
	kNull,
	kFalse,
	kTrue,
	kNumber,
	kString,
	kIdent,

	kSepSemi,		// ;
	kSepComma,		// ,
	kSepDot,		// .
	kSepColon,		// :

	kSepLParen,     // (
	kSepRParen,     // )
	kSepLBrack,		// [
	kSepRBrack,		// ]
	kSepLCurly,		// {
	kSepRCurly,		// }

	kOpNewVar,    // :=
	kOpAssign, // =
	kOpAdd,    // +
	kOpSub,    // -
	kOpMul,    // *
	kOpDiv,    // /

	kOpEq,     // ==
	kOpLt,     // <
	kOpLe,     // <=
	kOpGt,     // >
	kOpGe,     // >=


	kKwFunc,  // func
	kKwIf,    // if
	kKwElif,  // elif
	kKwElse,  // else
	kKwWhile, // while
	kKwFor,   // for
	kKwContinue,  // continue
	kKwBreak,   // break
	kKwReturn,  // return


};

extern std::map<std::string, TokenType> g_keywords;

// ����token�Ľṹ��
struct Token {
	bool Is(TokenType t_type) const noexcept;

	int line;			// �к�
	TokenType type;		// token����
	std::string str;	// �����Ҫ����Ϣ
};

} // namespace lexer

#endif // LEXER_TOKEN_H_