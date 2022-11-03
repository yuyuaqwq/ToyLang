#ifndef CODEGENER_CODEGENER_H_
#define CODEGENER_CODEGENER_H_

#include <vector>
#include <map>
#include <set>
#include <string>

#include "vm/instr.h"
#include "vm/value.h"

#include "ast/block.h"
#include "ast/stat.h"
#include "ast/exp.h"

namespace codegener {

// ��������ʱ�������쳣
class CodeGenerException : public std::exception{
public:
	CodeGenerException(const char* t_msg) : std::exception(t_msg) {

	}
};


class CodeGener {
public:
	CodeGener(vm::InstrSection* t_instrSection): m_instrSection(t_instrSection), m_level(0), m_varCount(0), m_varTable(1) {

	}

	void EntryScope() {
		m_level++;
		m_varTable.push_back(std::map<std::string, uint32_t>());
	}

	void ExitScope() {
		
		FreeMultVars(m_varTable[m_level].size());		// ������������ʹ�õı���
		m_level--;
		m_varTable.pop_back();
	}

	uint32_t AllocVar() {
		return m_varCount++;
	}

	void FreeVar() {
		m_varCount--;
	}

	void FreeMultVars(uint32_t count) {
		m_varCount -= count;
	}


	uint32_t GetVar(std::string name) {
		uint32_t varIdx = -1;
		// �ͽ��Ҿֲ�����
		do {
			auto& varScope = m_varTable[m_level];
			auto it = varScope.find(name);
			if (it != varScope.end()) {
				varIdx = it->second;
			}
		} while (m_level--);

		return varIdx;
	}


	void GenerateBlock(ast::Block* block) {
		EntryScope();
		for (auto& stat : block->statList) {
			GenerateStat(stat.get());
		}
		ExitScope();
	}

	void GenerateStat(ast::Stat* stat) {
		switch (stat->GetType())
		{
		case ast::StatType::kExp: {
			GenerateExp(static_cast<ast::ExpStat*>(stat)->exp.get());
			break;
		}
		case ast::StatType::kFuncDef: {
			GenerateFuncDefStat(static_cast<ast::FuncDefStat*>(stat));
			break;
		}
		default:
			break;
		}
	}

	void GenerateFuncDefStat(ast::FuncDefStat* stat) {
		if (m_funcMap.find(stat->funcName) != m_funcMap.end()) {
			throw CodeGenerException("function redefinition");
		}
		m_constTable.push_back(std::make_unique<vm::FunctionValue>(0, stat->parList.size()));
		int idx = m_constTable.size() - 1;
		m_funcMap.insert(std::make_pair(stat->funcName, idx));

		auto saveVarCount = m_varCount;
		m_varCount = 0;

		// ��Ҫ���ɽ�ջ�в������浽�ֲ�������Ĵ���


		// ����Ҳ����ֱ�ӽ������������ڱ�ĺ�����ָ�����У����ڵĽ����ǽ�ָ������װ��һ�����������У������Ҳ����һ��������������Lua��
		GenerateBlock(stat->block.get());


		m_varCount = saveVarCount;
	}


	// �ֲ������ڴ��޷���֪�ܹ���Ҫ���٣���˲��ܷ��䵽ջ��

	// vm����Ҫ��һ���ֶΣ���ŵ�ǰ�����Ļ��������������ʱ�����ָ�Ҫ�������
	// ÿ��callʱ������ǰ���ֶε�ֵpush��ջ�У����ҽ����ֶ���Ϊ��ǰ�ֲ�����������
	// retʱ�ʹ�ջ�лָ�����ֶ�

	// ��������ָ�����ʱ��m_varCount��0�������Ӻ��������еĿ鱻����ʱ���ֲ������������Ǵ�0��ʼ�ģ����غ�ָ�

	void GenerateNewVarStat(ast::NewVarStat* stat) {
		auto& varScope = m_varTable[m_level];
		if (varScope.find(stat->varName) != varScope.end()) {
			throw CodeGenerException("local var redefinition");
		}
		auto varIdx = AllocVar();
		varScope.insert(std::make_pair(stat->varName, varIdx));
		GenerateExp(stat->exp.get());		// ���ɱ��ʽ����ָ����ս���ᵽջ��
		m_instrSection->EmitPopV(varIdx);	// �������ֲ�������
	}

	void GenerateAssignStat(ast::AssignStat* stat) {
		auto varIdx = GetVar(stat->varName);
		if (varIdx == -1) {
			throw CodeGenerException("var not defined");
		}
		GenerateExp(stat->exp.get());		// ���ʽѹջ
		m_instrSection->EmitPopV(varIdx);	// �������ֲ�������
	}

	void GenerateExp(ast::Exp* exp) {
		switch (exp->GetType())
		{
		case ast::ExpType::kNull: {
			vm::NullValue null;
			uint32_t idx;
			auto it = m_constMap.find(null);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique<vm::NullValue>());
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_instrSection->EmitPushK(idx);
			break;
		}
		case ast::ExpType::kBool: {
			auto boolexp = static_cast<ast::BoolExp*>(exp);
			uint32_t idx;
			vm::BoolValue boolv(boolexp->value);
			auto it = m_constMap.find(boolv);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique<vm::BoolValue>(boolexp->value));
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_instrSection->EmitPushK(idx);
			break;
		}
		case ast::ExpType::kNumber: {
			auto numexp = static_cast<ast::NumberExp*>(exp);
			uint32_t idx;
			vm::NumberValue numv(numexp->value);
			auto it = m_constMap.find(numv);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique <vm::NumberValue> (numexp->value));
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_instrSection->EmitPushK(idx);
			break;
		}
		case ast::ExpType::kString: {
			auto strexp = static_cast<ast::StringExp*>(exp);
			uint32_t idx;
			vm::StringValue strv(strexp->value);
			auto it = m_constMap.find(strv);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique <vm::StringValue>(strexp->value));
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_instrSection->EmitPushK(idx);
			break;
		}
		case ast::ExpType::kName: {
			// ��ȡ����ֵ�Ļ������ҵ���Ӧ�ı�����ţ�������ջ
			auto nameExp = static_cast<ast::NameExp*>(exp);
			
			auto varIdx = GetVar(nameExp->name);
			if (varIdx == -1) {
				throw CodeGenerException("var not defined");
			}

			m_instrSection->EmitPushV(varIdx);
		}
		case ast::ExpType::kBinaOp: {
			auto binaOpExp = static_cast<ast::BinaOpExp*>(exp);

			// ���ұ��ʽ��ֵ��ջ
			GenerateExp(binaOpExp->leftExp.get());
			GenerateExp(binaOpExp->rightExp.get());

			// ��������ָ��
			switch (binaOpExp->oper) {
			case lexer::TokenType::kOpAdd:
				m_instrSection->EmitAdd();
				break;
			case lexer::TokenType::kOpSub:
				m_instrSection->EmitSub();
				break;
			case lexer::TokenType::kOpMul:
				m_instrSection->EmitMul();
				break;
			case lexer::TokenType::kOpDiv:
				m_instrSection->EmitDiv();
				break;
			case lexer::TokenType::kOpEq:
				m_instrSection->EmitEq();
				break;
			case lexer::TokenType::kOpLt:
				m_instrSection->EmitLt();
				break;
			case lexer::TokenType::kOpLe:
				m_instrSection->EmitLe();
				break;
			case lexer::TokenType::kOpGt:
				m_instrSection->EmitGt();
				break;
			case lexer::TokenType::kOpGe:
				m_instrSection->EmitGe();
				break;
			default:
				throw CodeGenerException("Unrecognized binary operator");
			}
			break;
		}
		case ast::ExpType::kFuncCall: {
			auto funcCallExp = static_cast<ast::FuncCallExp*>(exp);
			auto func = m_funcMap.find(funcCallExp->name);
			if (func == m_funcMap.end()) {
				throw CodeGenerException("Function not defined");
			}

			if (funcCallExp->parList.size() < m_constTable[func->second]->GetFunction()->parCount) {
				throw CodeGenerException("Wrong number of parameters passed during function call");
			}

			for (auto& parexp : funcCallExp->parList) {
				// ������ջ���ɺ������彫ջ�в����ŵ��ֲ���������
				GenerateExp(parexp.get());
			}
			
			// ����������ջ
			m_instrSection->EmitCall(func->second);
		}
		default:
			break;
		}
	}



private:
	vm::InstrSection* m_instrSection;
	
	std::map<std::string, uint32_t> m_funcMap;		// ȫ�ֺ���ӳ�䣬keyΪfunction��ȫ�ֳ����������
	
	std::map<vm::Value&, uint32_t> m_constMap;
	std::vector<std::unique_ptr<vm::Value>> m_constTable;		// ȫ�ֳ�����

	uint32_t m_level;		// ������㼶
	uint32_t m_varCount;			// �ܷ���ֲ���������
	std::vector<std::map<std::string, uint32_t>> m_varTable;		// ���������������йأ�keyΪ�ֲ���������
};

} // namespace codegener

#endif // CODEGENER_CODEGENER_H_