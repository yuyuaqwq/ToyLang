﻿#include "lexer/lexer.h"
#include "parser/parser.h"

int main() {
    using namespace lexer;
    using namespace parser;

    Lexer lexer{ "1 + 33 - 0 * (33 / 999) - 123" };
    /*do {
        auto token = lexer.NextToken();
        if (token.Is(TokenType::kEof)) {
            break;
        }

        switch (token.type) {
        case TokenType::kNumber:{
            printf("%s\n", token.str.c_str()); break;
        }
        case TokenType::kOpAdd: {
            printf("+\n"); break;
        }
        case TokenType::kOpDiv: {
            printf("/\n"); break;
        }
        case TokenType::kOpMul: {
            printf("*\n"); break;
        }
        case TokenType::kOpSub: {
            printf("-\n"); break;
        }
        case TokenType::kSepLParen: {
            printf("(\n"); break;
        }
        case TokenType::kSepRParen: {
            printf(")\n"); break;
        }
        }
    } while (true);*/

    Parser parser(&lexer);

    auto exp = parser.ParseExp();


}