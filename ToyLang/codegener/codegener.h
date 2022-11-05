#ifndef CODEGENER_CODEGENER_H_
#define CODEGENER_CODEGENER_H_

#include <vector>
#include <map>
#include <set>
#include <string>

#include "vm/instr.h"
#include "vm/value.h"
#include "vm/vm.h"
#include "vm/section.h"

#include "ast/stat.h"
#include "ast/exp.h"

namespace codegener {

struct Scope {
	vm::FunctionBodyValue* m_func;		// ��������������
	uint32_t varCount;		// ��ǰ�����ڵ�ǰ�������е���Ч��������
	std::map<std::string, uint32_t> m_varTable;		// ������keyΪ��������
};



// ��������ʱ�������쳣
class CodeGenerException : public std::exception{
public:
	CodeGenerException(const char* t_msg) : std::exception(t_msg) {

	}
};


class CodeGener {
public:
	CodeGener(vm::VM* t_vm) : m_vm{ t_vm } {
		m_curFunc = &m_vm->m_mainFunc;
		m_scope.push_back(Scope{ m_curFunc });
	}

	void EntryScope(vm::FunctionBodyValue* subFunc = nullptr) {
		if (!subFunc) {
			// ��������������º���
			m_scope.push_back(Scope{ m_curFunc, m_scope[m_scope.size() - 1].varCount });
			return;
		}
		// ��������������º���
		m_scope.push_back(Scope{ subFunc, 0 });
	}

	void ExitScope() {
		m_scope.pop_back();
	}

	uint32_t AllocConst(std::unique_ptr<vm::Value> value) {
		uint32_t constIdx;
		auto it = m_constMap.find(value.get());
		if (it == m_constMap.end()) {
			m_constSect.Push(std::move(value));
			constIdx = m_constSect.Size() - 1;
		}
		else {
			constIdx = it->second;
		}
		return constIdx;
	}

	uint32_t AllocVar(std::string varName) {
		auto& varTable = m_scope[m_scope.size() - 1].m_varTable;
		if (varTable.find(varName) != varTable.end()) {
			throw CodeGenerException("local var redefinition");
		}
		auto varIdx = m_scope[m_scope.size() - 1].varCount++;
		varTable.insert(std::make_pair(varName, varIdx));
		return varIdx;
	}

	uint32_t GetVar(std::string varName) {
		uint32_t varIdx = -1;
		// �ͽ��ұ���
		
		for (int i = m_scope.size() - 1; i >= 0; i--) {
			auto& varTable = m_scope[i].m_varTable;
			auto it = varTable.find(varName);
			if (it != varTable.end()) {
				if (m_scope[i].m_func == m_curFunc) {
					varIdx = it->second;
				}
				else {
					// �����ⲿ�����ı�������Ҫ����Ϊ��ǰ��������upvalue����
					auto constIdx = AllocConst(std::make_unique<vm::UpValue>(it->second, m_scope[i].m_func));
					m_curFunc->instrSect.EmitPushK(constIdx);
					varIdx = AllocVar(varName);
					m_curFunc->instrSect.EmitPopV(varIdx);
				}
				break;
			}
		}
		return varIdx;
	}


	void RegistryFunctionBridge(std::string funcName, vm::FunctionBridgeCall funcAddr) {
		auto varIdx = AllocVar(funcName);
		auto constIdx = AllocConst(std::make_unique<vm::FunctionBridgeValue>(funcAddr));


		// ���ɽ������ŵ��������еĴ���
		// ���������ִ��ʱȥ���أ���������ּ��صĳ����Ǻ����壬�ͻὫ����ԭ�͸����ֲ�����
		m_curFunc->instrSect.EmitPushK(constIdx);
		m_curFunc->instrSect.EmitPopV(varIdx);
	}


	void Generate(ast::BlockStat* block) {
		
		for (auto& stat : block->statList) {
			GenerateStat(stat.get());
		}
		m_vm->m_constSect = std::move(m_constSect);
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
			auto expStat = static_cast<ast::ExpStat*>(stat)->exp.get();

			// ���������ʽ�������ս��
			 if (expStat) {
				 GenerateExp(expStat);
				 m_curFunc->instrSect.EmitPop();		
			 }
			
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
		case ast::StatType::kIf:
			GenerateIfStat(static_cast<ast::IfStat*>(stat));
			break;
		case ast::StatType::kReturn:
			GenerateReturnStat(static_cast<ast::ReturnStat*>(stat));
			break;
		default:
			throw CodeGenerException("Unknown statement type");
		}
	}

	void GenerateFuncDefStat(ast::FuncDefStat* stat) {
		auto varIdx = AllocVar(stat->funcName);
		auto constIdx = AllocConst(std::make_unique<vm::FunctionBodyValue>(stat->parList.size()));

		// ���ɽ������ŵ��������еĴ���
		// ���������ִ��ʱȥ���أ���������ּ��صĳ����Ǻ����壬�ͻὫ����ԭ�͸����ֲ�����
		m_curFunc->instrSect.EmitPushK(constIdx);
		m_curFunc->instrSect.EmitPopV(varIdx);
		
		
		// ���滷������������ָ����
		auto savefunc = m_curFunc;

		// �л�����
		EntryScope(m_constSect.Get(constIdx)->GetFunctionBody());
		m_curFunc = m_constSect.Get(constIdx)->GetFunctionBody();

		
		for (int i = 0; i < m_curFunc->parCount; i++) {
			AllocVar(stat->parList[i]);
		}

		auto block = stat->block.get();
		for (int i = 0; i < block->statList.size(); i++) {
			auto& stat = block->statList[i];
			GenerateStat(stat.get());
			if (i == block->statList.size() - 1) {
				if (stat->GetType() != ast::StatType::kReturn) {
					// ��ȫĩβ��return
					m_curFunc->instrSect.EmitPushK(AllocConst(std::make_unique<vm::NullValue>()));
					m_curFunc->instrSect.EmitRet();
				}
			}
		}

		// �ָ�����
		ExitScope();
		m_curFunc = savefunc;
	}

	void GenerateReturnStat(ast::ReturnStat* stat) {
		if (stat->exp.get()) {
			GenerateExp(stat->exp.get());
		}
		else {
			m_curFunc->instrSect.EmitPushK(AllocConst(std::make_unique<vm::NullValue>()));
		}
		m_curFunc->instrSect.EmitRet();
	}

	// �ֲ������ڴ��޷���ǰ��֪�ܹ���Ҫ���٣���˲��ܷ��䵽ջ��
	// ���������
		// �����ṩ����������

	// ������ָ�����ı�������������ʱ���޷�ȷ���������������
	// ԭ��
		// ����ʱ�޷���ǰ��֪call��λ�ã���call�����жദ��ÿ��callʱ���������״̬���ܶ���һ��
	// ���������
		// Ϊÿ�������ṩһ���Լ��ı��������ŵ�������У�callʱ�л�������
	    // �ڴ������ɹ����У���Ҫ��ȡ����ʱ���������ʹ�õı����ǵ�ǰ����֮����ⲿ������ģ��ͻ��ڳ������д���һ������Ϊupvalue�ı����������ص���ǰ�����ı�����
		// upvalue�洢���ⲿ������Body��ַ���Լ���Ӧ�ı�������


	void GenerateNewVarStat(ast::NewVarStat* stat) {
		auto varIdx = AllocVar(stat->varName);
		GenerateExp(stat->exp.get());		// ���ɱ��ʽ����ָ����ս���ᵽջ��
		m_curFunc->instrSect.EmitPopV(varIdx);	// �������ֲ�������
	}

	void GenerateAssignStat(ast::AssignStat* stat) {
		auto varIdx = GetVar(stat->varName);
		if (varIdx == -1) {
			throw CodeGenerException("var not defined");
		}
		GenerateExp(stat->exp.get());		// ���ʽѹջ
		m_curFunc->instrSect.EmitPopV(varIdx);	// ������������
	}

	void GenerateIfStat(ast::IfStat* stat) {
		GenerateExp(stat->exp.get());		// ���ʽ���ѹջ

		
		uint32_t ifpc = m_curFunc->instrSect.GetPc() + 1;		// ������һ��elif/else�޸�
		m_curFunc->instrSect.EmitJcf(0);		// ��ǰд������Ϊfalseʱ��ת��ָ��

		GenerateBlock(stat->block.get());

		
		
		// if
		
		// jcf end
		// ...
	// end:
		// ...


		// if
		// else

		// jcf else
		// ...
		// jmp end
	// else:
		// ...
	// end:
		// ....


		// if 
		// elif
		// else

		// jcf elif
		// ...
		// jmp end
	// elif:
		// jcf else
		// ...
		// jmp end
	// else:
		// ...
	// end:
		// ....


		// if 
		// elif
		// elif
		// else

		// jcf elif1
		// ...
		// jmp end
	// elif1:
		// jcf elif2
		// ...
		// jmp end
	// elif2:
		// jcf else
		// ...
		// jmp end
	// else:
		// ...
	// end:
		// ....

		std::vector<uint32_t> endPcList;
		for (auto& elifStat : stat->elifStatList) {
			endPcList.push_back(m_curFunc->instrSect.GetPc() + 1);
			m_curFunc->instrSect.EmitJmp(0);		// ��ǰд����һ��֧�˳�if��֧�ṹ��jmp��ת

			// �޸�����Ϊfalseʱ����ת��if/elif��֮��ĵ�ַ
			*(uint32_t*)m_curFunc->instrSect.GetPtr(ifpc) = m_curFunc->instrSect.GetPc();

			GenerateExp(elifStat->exp.get());		// ���ʽ���ѹջ
			ifpc = m_curFunc->instrSect.GetPc() + 1;		// ������һ��elif/else�޸�
			m_curFunc->instrSect.EmitJcf(0);		// ��ǰд������Ϊfalseʱ��ת��ָ��

			GenerateBlock(elifStat->block.get());
		}

		

		if (stat->elseStat.get()) {
			endPcList.push_back(m_curFunc->instrSect.GetPc() + 1);
			m_curFunc->instrSect.EmitJmp(0);		// ��ǰд����һ��֧�˳�if��֧�ṹ��jmp��ת

			// �޸�����Ϊfalseʱ����ת��if/elif��֮��ĵ�ַ
			*(uint32_t*)m_curFunc->instrSect.GetPtr(ifpc) = m_curFunc->instrSect.GetPc();

			GenerateBlock(stat->elseStat->block.get());
		}
		else {
			// �޸�����Ϊfalseʱ����ת��if/elif��֮��ĵ�ַ
			*(uint32_t*)m_curFunc->instrSect.GetPtr(ifpc) = m_curFunc->instrSect.GetPc();
		}

		// ����if��֧�ṹ�������޸������˳���֧�ṹ�ĵ�ַ
		for (auto endPc : endPcList) {
			*(uint32_t*)m_curFunc->instrSect.GetPtr(endPc) = m_curFunc->instrSect.GetPc();
		}
		


	}


	void GenerateExp(ast::Exp* exp) {
		switch (exp->GetType())
		{
		case ast::ExpType::kNull: {
			auto constIdx = AllocConst(std::make_unique<vm::NullValue>());
			m_curFunc->instrSect.EmitPushK(constIdx);
			break;
		}
		case ast::ExpType::kBool: {
			auto boolexp = static_cast<ast::BoolExp*>(exp);
			auto constIdx = AllocConst(std::make_unique<vm::BoolValue>(boolexp->value));
			m_curFunc->instrSect.EmitPushK(constIdx);
			break;
		}
		case ast::ExpType::kNumber: {
			auto numexp = static_cast<ast::NumberExp*>(exp);
			auto constIdx = AllocConst(std::make_unique<vm::NumberValue>(numexp->value));
			m_curFunc->instrSect.EmitPushK(constIdx);
			break;
		}
		case ast::ExpType::kString: {
			auto strexp = static_cast<ast::StringExp*>(exp);
			auto constIdx = AllocConst(std::make_unique<vm::StringValue>(strexp->value));
			m_curFunc->instrSect.EmitPushK(constIdx);
			break;
		}
		case ast::ExpType::kName: {
			// ��ȡ����ֵ�Ļ������ҵ���Ӧ�ı�����ţ�������ջ
			auto nameExp = static_cast<ast::NameExp*>(exp);
			
			auto varIdx = GetVar(nameExp->name);
			if (varIdx == -1) {
				throw CodeGenerException("var not defined");
			}

			m_curFunc->instrSect.EmitPushV(varIdx);	// �ӱ����л�ȡ

			
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
			case lexer::TokenType::kOpNe:
				m_curFunc->instrSect.EmitNe();
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

			auto varIdx = GetVar(funcCallExp->name);
			if (varIdx == -1) {
				throw CodeGenerException("Function not defined");
			}

			//if (funcCallExp->parList.size() < m_constTable[]->GetFunctionBody()->parCount) {
			//	throw CodeGenerException("Wrong number of parameters passed during function call");
			//}

			for (int i = funcCallExp->parList.size() - 1; i >= 0; i--) {
				// ����������ջ����call����ջ�в����ŵ���������
				GenerateExp(funcCallExp->parList[i].get());
			}

			m_curFunc->instrSect.EmitPushK(AllocConst(std::make_unique<vm::NumberValue>(funcCallExp->parList.size())));

			// ����ԭ�ʹ���ڱ�������
			m_curFunc->instrSect.EmitCall(varIdx);
			break;
		}
		default:
			break;
		}
	}



private:

	vm::VM* m_vm;

	vm::FunctionBodyValue* m_curFunc;		// ��ǰ���ɺ���

	// ��ʱ�����⣬ָ���û�취������<��
	std::map<vm::Value*, uint32_t> m_constMap;
	vm::ValueSection m_constSect;		// ȫ�ֳ�����

	std::vector<Scope> m_scope;
};

} // namespace codegener

#endif // CODEGENER_CODEGENER_H_