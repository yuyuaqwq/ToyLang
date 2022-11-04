#ifndef CODEGENER_CODEGENER_H_
#define CODEGENER_CODEGENER_H_

#include <vector>
#include <map>
#include <set>
#include <string>

#include "vm/instr.h"
#include "vm/value.h"
#include "vm/vm.h"

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
	CodeGener(vm::VM* t_vm) : m_vm(t_vm), m_level(0), m_varCount(0), m_varTable(1) {
		m_curFunc = &m_vm->m_mainFunc;
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
		// �ͽ��ұ���
		auto i = m_level;
		do {
			auto& varScope = m_varTable[i];
			auto it = varScope.find(name);
			if (it != varScope.end()) {
				varIdx = it->second;
			}
		} while (i--);

		return varIdx;
	}


	void RegistryFunctionBridge(std::string funcName, vm::FunctionBridgeCall funcAddr) {
		auto& varScope = m_varTable[m_level];
		if (varScope.find(funcName) != varScope.end()) {
			throw CodeGenerException("function redefinition");
		}
		m_constTable.push_back(std::make_unique<vm::FunctionBridgeValue>(funcAddr));
		int constIdx = m_constTable.size() - 1;

		auto varIdx = AllocVar();
		varScope.insert(std::make_pair(funcName, varIdx));

		// ���ɽ������ŵ��������еĴ���
		// ���������ִ��ʱȥ���أ���������ּ��صĳ����Ǻ����壬�ͻὫ����ԭ�͸����ֲ�����
		m_curFunc->instrSect.EmitPushK(constIdx);
		m_curFunc->instrSect.EmitPopV(varIdx);
	}


	void Generate(ast::BlockStat* block) {
		
		for (auto& stat : block->statList) {
			GenerateStat(stat.get());
		}
		m_vm->m_constSect = std::move(m_constTable);
		m_curFunc->instrSect.EmitStop();
	}

	void GenerateBlock(ast::BlockStat* block) {
		EntryScope();
		for (auto& stat : block->statList) {
			GenerateStat(stat.get());
		}
		ExitScope();
	}

	void GenerateStat(ast::Stat* stat) {
		switch (stat->GetType())
		{
		case ast::StatType::kBlock: {
			GenerateBlock(static_cast<ast::BlockStat*>(stat));
			break;
		}
		case ast::StatType::kExp: {
			GenerateExp(static_cast<ast::ExpStat*>(stat)->exp.get());
			break;
		}
		case ast::StatType::kFuncDef: {
			GenerateFuncDefStat(static_cast<ast::FuncDefStat*>(stat));
			break;
		}
		case ast::StatType::kAssign: {
			GenerateAssignStat(static_cast<ast::AssignStat*>(stat));
			break;
		}
		case ast::StatType::kNewVar: {
			GenerateNewVarStat(static_cast<ast::NewVarStat*>(stat));
			break;
		}
		default:
			break;
		}
	}

	void GenerateFuncDefStat(ast::FuncDefStat* stat) {
		auto& varScope = m_varTable[m_level];
		if (varScope.find(stat->funcName) != varScope.end()) {
			throw CodeGenerException("function redefinition");
		}
		m_constTable.push_back(std::make_unique<vm::FunctionBodyValue>(stat->parList.size()));
		int constIdx = m_constTable.size() - 1;
		
		auto varIdx = AllocVar();
		varScope.insert(std::make_pair(stat->funcName, varIdx));

		// ���ɽ������ŵ��������еĴ���
		// ���������ִ��ʱȥ���أ���������ּ��صĳ����Ǻ����壬�ͻὫ����ԭ�͸����ֲ�����
		m_curFunc->instrSect.EmitPushK(constIdx);
		m_curFunc->instrSect.EmitPopV(varIdx);
		
		
		// ���ݵ�ǰ���������»���ȥ���ɺ���ָ��(�����Ƕ����������򣬲��ܲ�������������ķ���)
		auto saveLevel = m_level;
		auto saveVarCount = m_varCount;
		auto saveVarTable = std::move(m_varTable);
		auto savefunc = m_curFunc;

		m_level = 0;
		m_varCount = 0;
		m_varTable.resize(1);
		m_curFunc = m_constTable[constIdx]->GetFunctionBody();

		for (int i = 0; i < m_curFunc->parCount; i++) {
			m_varTable[m_level].insert(std::make_pair(stat->parList[i], AllocVar()));
		}

		auto block = stat->block.get();
		for (auto& stat : block->statList) {
			GenerateStat(stat.get());
		}
		m_curFunc->instrSect.EmitRet();

		// �ָ�����
		m_curFunc = savefunc;
		m_varTable = std::move(saveVarTable);
		m_varCount = saveVarCount;
		m_level = saveLevel;
	}


	// �ֲ������ڴ��޷���֪�ܹ���Ҫ���٣���˲��ܷ��䵽ջ��

	// vm����Ҫ��һ���ֶΣ���ŵ�ǰ�����Ļ��������������ʱ�����ָ�Ҫ�������
	// ÿ��callʱ������ǰ���ֶε�ֵpush��ջ�У����ҽ����ֶ���Ϊ��ǰ�ֲ�����������
	// retʱ�ʹ�ջ�лָ�����ֶ�

	// ��������ָ�����ʱ��m_varCount��0�������Ӻ��������еĿ鱻����ʱ�������������Ǵ�0��ʼ�ģ����غ�ָ�

	void GenerateNewVarStat(ast::NewVarStat* stat) {
		auto& varScope = m_varTable[m_level];
		if (varScope.find(stat->varName) != varScope.end()) {
			throw CodeGenerException("local var redefinition");
		}
		auto varIdx = AllocVar();
		varScope.insert(std::make_pair(stat->varName, varIdx));
		GenerateExp(stat->exp.get());		// ���ɱ��ʽ����ָ����ս���ᵽջ��
		m_curFunc->instrSect.EmitPopV(varIdx);	// �������ֲ�������
	}

	void GenerateAssignStat(ast::AssignStat* stat) {
		auto varIdx = GetVar(stat->varName);
		if (varIdx == -1) {
			throw CodeGenerException("var not defined");
		}
		GenerateExp(stat->exp.get());		// ���ʽѹջ
		m_curFunc->instrSect.EmitPopV(varIdx);	// �������ֲ�������
	}

	void GenerateExp(ast::Exp* exp) {
		switch (exp->GetType())
		{
		case ast::ExpType::kNull: {
			vm::NullValue null;
			uint32_t idx;
			auto it = m_constMap.find(&null);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique<vm::NullValue>());
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_curFunc->instrSect.EmitPushK(idx);
			break;
		}
		case ast::ExpType::kBool: {
			auto boolexp = static_cast<ast::BoolExp*>(exp);
			uint32_t idx;
			vm::BoolValue boolv(boolexp->value);
			auto it = m_constMap.find(&boolv);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique<vm::BoolValue>(boolexp->value));
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_curFunc->instrSect.EmitPushK(idx);
			break;
		}
		case ast::ExpType::kNumber: {
			auto numexp = static_cast<ast::NumberExp*>(exp);
			uint32_t idx;
			vm::NumberValue numv(numexp->value);
			auto it = m_constMap.find(&numv);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique <vm::NumberValue> (numexp->value));
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_curFunc->instrSect.EmitPushK(idx);
			break;
		}
		case ast::ExpType::kString: {
			auto strexp = static_cast<ast::StringExp*>(exp);
			uint32_t idx;
			vm::StringValue strv(strexp->value);
			auto it = m_constMap.find(&strv);
			if (it == m_constMap.end()) {
				m_constTable.push_back(std::make_unique <vm::StringValue>(strexp->value));
				idx = m_constTable.size() - 1;
			}
			else {
				idx = it->second;
			}
			m_curFunc->instrSect.EmitPushK(idx);
			break;
		}
		case ast::ExpType::kName: {
			// ��ȡ����ֵ�Ļ������ҵ���Ӧ�ı�����ţ�������ջ
			auto nameExp = static_cast<ast::NameExp*>(exp);
			
			auto varIdx = GetVar(nameExp->name);
			if (varIdx == -1) {
				throw CodeGenerException("var not defined");
			}

			m_curFunc->instrSect.EmitPushV(varIdx);
			break;
		}
		case ast::ExpType::kBinaOp: {
			auto binaOpExp = static_cast<ast::BinaOpExp*>(exp);

			// ���ұ��ʽ��ֵ��ջ
			GenerateExp(binaOpExp->leftExp.get());
			GenerateExp(binaOpExp->rightExp.get());

			// ��������ָ��
			switch (binaOpExp->oper) {
			case lexer::TokenType::kOpAdd:
				m_curFunc->instrSect.EmitAdd();
				break;
			case lexer::TokenType::kOpSub:
				m_curFunc->instrSect.EmitSub();
				break;
			case lexer::TokenType::kOpMul:
				m_curFunc->instrSect.EmitMul();
				break;
			case lexer::TokenType::kOpDiv:
				m_curFunc->instrSect.EmitDiv();
				break;
			case lexer::TokenType::kOpEq:
				m_curFunc->instrSect.EmitEq();
				break;
			case lexer::TokenType::kOpLt:
				m_curFunc->instrSect.EmitLt();
				break;
			case lexer::TokenType::kOpLe:
				m_curFunc->instrSect.EmitLe();
				break;
			case lexer::TokenType::kOpGt:
				m_curFunc->instrSect.EmitGt();
				break;
			case lexer::TokenType::kOpGe:
				m_curFunc->instrSect.EmitGe();
				break;
			default:
				throw CodeGenerException("Unrecognized binary operator");
			}
			break;
		}
		case ast::ExpType::kFuncCall: {
			auto funcCallExp = static_cast<ast::FuncCallExp*>(exp);

			auto funcIdx = GetVar(funcCallExp->name);
			if (funcIdx == -1) {
				throw CodeGenerException("Function not defined");
			}

			//if (funcCallExp->parList.size() < m_constTable[]->GetFunctionBody()->parCount) {
			//	throw CodeGenerException("Wrong number of parameters passed during function call");
			//}

			for (auto& parexp : funcCallExp->parList) {
				// ������ջ����call����ջ�в����ŵ���������
				GenerateExp(parexp.get());
			}

			

			// �����ڱ����������
			m_curFunc->instrSect.EmitCall(funcIdx);
			break;
		}
		default:
			break;
		}
	}



private:

	vm::VM* m_vm;

	vm::FunctionBodyValue* m_curFunc;		// ��ǰ���ɺ���

	// ��ʱ�����⣬ָ���û�취��������
	std::map<vm::Value*, uint32_t> m_constMap;
	std::vector<std::unique_ptr<vm::Value>> m_constTable;		// ȫ�ֳ�����

	uint32_t m_level;		// ������㼶
	uint32_t m_varCount;			// �ܷ���ֲ���������
	std::vector<std::map<std::string, uint32_t>> m_varTable;		// ���������������йأ�keyΪ�ֲ���������
};

} // namespace codegener

#endif // CODEGENER_CODEGENER_H_