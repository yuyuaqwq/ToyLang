#include "codegener.h"


namespace toylang {

CodeGenerException::CodeGenerException(const char* t_msg) : std::exception(t_msg) {

}




CodeGener::CodeGener(ValueSection* t_constSect) : m_constSect(t_constSect), m_curLoopRepairEndPcList{ nullptr } {
	t_constSect->Clear();
	t_constSect->Push(std::make_unique<FunctionBodyValue>(0));
	m_curFunc = t_constSect->Get(0)->GetFunctionBody();

	m_scope.push_back(Scope{ m_curFunc });
}

void CodeGener::EntryScope(FunctionBodyValue* subFunc) {
	if (!subFunc) {
		// ��������������º���
		m_scope.push_back(Scope{ m_curFunc, m_scope[m_scope.size() - 1].varCount });
		return;
	}
	// ��������������º���
	m_scope.push_back(Scope{ subFunc, 0 });
}

void CodeGener::ExitScope() {
	m_scope.pop_back();
}

uint32_t CodeGener::AllocConst(std::unique_ptr<Value> value) {
	uint32_t constIdx;
	auto it = m_constMap.find(value.get());
	if (it == m_constMap.end()) {
		m_constSect->Push(std::move(value));
		constIdx = m_constSect->Size() - 1;
	}
	else {
		constIdx = it->second;
	}
	return constIdx;
}

uint32_t CodeGener::AllocVar(std::string varName) {
	auto& varTable = m_scope[m_scope.size() - 1].m_varTable;
	if (varTable.find(varName) != varTable.end()) {
		throw CodeGenerException("local var redefinition");
	}
	auto varIdx = m_scope[m_scope.size() - 1].varCount++;
	varTable.insert(std::make_pair(varName, varIdx));
	return varIdx;
}

uint32_t CodeGener::GetVar(std::string varName) {
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
				auto constIdx = AllocConst(std::make_unique<UpValue>(it->second, m_scope[i].m_func));
				m_curFunc->instrSect.EmitPushK(constIdx);
				varIdx = AllocVar(varName);
				m_curFunc->instrSect.EmitPopV(varIdx);
			}
			break;
		}
	}
	return varIdx;
}


void CodeGener::RegistryFunctionBridge(std::string funcName, FunctionBridgeCall funcAddr) {
	auto varIdx = AllocVar(funcName);
	auto constIdx = AllocConst(std::make_unique<FunctionBridgeValue>(funcAddr));


	// ���ɽ������ŵ��������еĴ���
	// ���������ִ��ʱȥ���أ���������ּ��صĳ����Ǻ����壬�ͻὫ����ԭ�͸����ֲ�����
	m_curFunc->instrSect.EmitPushK(constIdx);
	m_curFunc->instrSect.EmitPopV(varIdx);
}


void CodeGener::Generate(BlockStat* block, ValueSection* constSect) {

	for (auto& stat : block->statList) {
		GenerateStat(stat.get());
	}
	m_curFunc->instrSect.EmitStop();

}

void CodeGener::GenerateBlock(BlockStat* block) {
	EntryScope();
	for (auto& stat : block->statList) {
		GenerateStat(stat.get());
	}
	ExitScope();
}

void CodeGener::GenerateStat(Stat* stat) {
	switch (stat->GetType())
	{
	case StatType::kBlock: {
		GenerateBlock(static_cast<BlockStat*>(stat));
		break;
	}
	case StatType::kExp: {
		auto expStat = static_cast<ExpStat*>(stat)->exp.get();

		// ���������ʽ�������ս��
		if (expStat) {
			GenerateExp(expStat);
			m_curFunc->instrSect.EmitPop();
		}

		break;
	}
	case StatType::kFuncDef: {
		GenerateFuncDefStat(static_cast<FuncDefStat*>(stat));
		break;
	}
	case StatType::kReturn: {
		GenerateReturnStat(static_cast<ReturnStat*>(stat));
		break;
	}
	case StatType::kAssign: {
		GenerateAssignStat(static_cast<AssignStat*>(stat));
		break;
	}
	case StatType::kNewVar: {
		GenerateNewVarStat(static_cast<NewVarStat*>(stat));
		break;
	}
	case StatType::kIf: {
		GenerateIfStat(static_cast<IfStat*>(stat));
		break;
	}
	case StatType::kWhile: {
		GenerateWhileStat(static_cast<WhileStat*>(stat));
		break;
	}
	case StatType::kContinue: {
		GenerateContinueStat(static_cast<ContinueStat*>(stat));
		break;
	}
	case StatType::kBreak: {
		GenerateBreakStat(static_cast<BreakStat*>(stat));
		break;
	}

	default:
		throw CodeGenerException("Unknown statement type");
	}
}

void CodeGener::GenerateFuncDefStat(FuncDefStat* stat) {
	auto varIdx = AllocVar(stat->funcName);
	auto constIdx = AllocConst(std::make_unique<FunctionBodyValue>(stat->parList.size()));

	// ���ɽ������ŵ��������еĴ���
	// ���������ִ��ʱȥ���أ���������ּ��صĳ����Ǻ����壬�ͻὫ����ԭ�͸����ֲ�����
	m_curFunc->instrSect.EmitPushK(constIdx);
	m_curFunc->instrSect.EmitPopV(varIdx);


	// ���滷������������ָ����
	auto savefunc = m_curFunc;

	// �л�����
	EntryScope(m_constSect->Get(constIdx)->GetFunctionBody());
	m_curFunc = m_constSect->Get(constIdx)->GetFunctionBody();


	for (int i = 0; i < m_curFunc->parCount; i++) {
		AllocVar(stat->parList[i]);
	}

	auto block = stat->block.get();
	for (int i = 0; i < block->statList.size(); i++) {
		auto& stat = block->statList[i];
		GenerateStat(stat.get());
		if (i == block->statList.size() - 1) {
			if (stat->GetType() != StatType::kReturn) {
				// ��ȫĩβ��return
				m_curFunc->instrSect.EmitPushK(AllocConst(std::make_unique<NullValue>()));
				m_curFunc->instrSect.EmitRet();
			}
		}
	}

	// �ָ�����
	ExitScope();
	m_curFunc = savefunc;
}

void CodeGener::GenerateReturnStat(ReturnStat* stat) {
	if (stat->exp.get()) {
		GenerateExp(stat->exp.get());
	}
	else {
		m_curFunc->instrSect.EmitPushK(AllocConst(std::make_unique<NullValue>()));
	}
	m_curFunc->instrSect.EmitRet();
}

// Ϊ�˼����������ǰ����/����޸��ֲ���������������˲��ܷ��䵽ջ��
// ���������
	// �����ṩ����������

// ������ָ�����ı�������������ʱ���޷�ȷ���������������
// ԭ��
	// ����ʱ�޷���ǰ��֪call��λ�ã���call�����жദ��ÿ��callʱ���������״̬���ܶ���һ��
// ���������
	// Ϊÿ�������ṩһ���Լ��ı��������ŵ�������У�callʱ�л�������
	// �ڴ������ɹ����У���Ҫ��ȡ����ʱ���������ʹ�õı����ǵ�ǰ����֮����ⲿ������ģ��ͻ��ڳ������д���һ������Ϊupvalue�ı����������ص���ǰ�����ı�����
	// upvalue�洢���ⲿ������Body��ַ���Լ���Ӧ�ı�������


void CodeGener::GenerateNewVarStat(NewVarStat* stat) {
	auto varIdx = AllocVar(stat->varName);
	GenerateExp(stat->exp.get());		// ���ɱ��ʽ����ָ����ս���ᵽջ��
	m_curFunc->instrSect.EmitPopV(varIdx);	// �������ֲ�������
}

void CodeGener::GenerateAssignStat(AssignStat* stat) {
	auto varIdx = GetVar(stat->varName);
	if (varIdx == -1) {
		throw CodeGenerException("var not defined");
	}
	GenerateExp(stat->exp.get());		// ���ʽѹջ
	m_curFunc->instrSect.EmitPopV(varIdx);	// ������������
}

void CodeGener::GenerateIfStat(IfStat* stat) {
	GenerateExp(stat->exp.get());		// ���ʽ���ѹջ


	uint32_t ifPc = m_curFunc->instrSect.GetPc() + 1;		// ������һ��elif/else�޸�
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

	std::vector<uint32_t> repairEndPcList;
	for (auto& elifStat : stat->elifStatList) {
		repairEndPcList.push_back(m_curFunc->instrSect.GetPc() + 1);
		m_curFunc->instrSect.EmitJmp(0);		// ��ǰд����һ��֧�˳�if��֧�ṹ��jmp��ת

		// �޸�����Ϊfalseʱ����ת��if/elif��֮��ĵ�ַ
		*(uint32_t*)m_curFunc->instrSect.GetPtr(ifPc) = m_curFunc->instrSect.GetPc();

		GenerateExp(elifStat->exp.get());		// ���ʽ���ѹջ
		ifPc = m_curFunc->instrSect.GetPc() + 1;		// ������һ��elif/else�޸�
		m_curFunc->instrSect.EmitJcf(0);		// ��ǰд������Ϊfalseʱ��ת��ָ��

		GenerateBlock(elifStat->block.get());
	}



	if (stat->elseStat.get()) {
		repairEndPcList.push_back(m_curFunc->instrSect.GetPc() + 1);
		m_curFunc->instrSect.EmitJmp(0);		// ��ǰд����һ��֧�˳�if��֧�ṹ��jmp��ת

		// �޸�����Ϊfalseʱ����ת��if/elif��֮��ĵ�ַ
		*(uint32_t*)m_curFunc->instrSect.GetPtr(ifPc) = m_curFunc->instrSect.GetPc();

		GenerateBlock(stat->elseStat->block.get());
	}
	else {
		// �޸�����Ϊfalseʱ����ת��if/elif��֮��ĵ�ַ
		*(uint32_t*)m_curFunc->instrSect.GetPtr(ifPc) = m_curFunc->instrSect.GetPc();
	}

	// ����if��֧�ṹ�������޸������˳���֧�ṹ�ĵ�ַ
	for (auto repairEndPc : repairEndPcList) {
		*(uint32_t*)m_curFunc->instrSect.GetPtr(repairEndPc) = m_curFunc->instrSect.GetPc();
	}



}

void CodeGener::GenerateWhileStat(WhileStat* stat) {

	auto saveCurLoopRepairEndPcList = m_curLoopRepairEndPcList;
	auto saveCurLoopStartPc = m_curLoopStartPc;



	std::vector<uint32_t> loopRepairEndPcList;
	m_curLoopRepairEndPcList = &loopRepairEndPcList;

	uint32_t loopStartPc = m_curFunc->instrSect.GetPc();		// ��¼����ѭ����pc
	m_curLoopStartPc = loopStartPc;


	GenerateExp(stat->exp.get());		// ���ʽ���ѹջ


	loopRepairEndPcList.push_back(m_curFunc->instrSect.GetPc() + 1);		// �ȴ��޸�
	m_curFunc->instrSect.EmitJcf(0);		// ��ǰд������Ϊfalseʱ����ѭ����ָ��

	GenerateBlock(stat->block.get());

	m_curFunc->instrSect.EmitJmp(loopStartPc);		// ���»�ȥ���Ƿ���Ҫѭ��

	for (auto repairEndPc : loopRepairEndPcList) {
		*(uint32_t*)m_curFunc->instrSect.GetPtr(repairEndPc) = m_curFunc->instrSect.GetPc();		// �޸�����ѭ����ָ���pc
	}

	m_curLoopStartPc = saveCurLoopStartPc;
	m_curLoopRepairEndPcList = saveCurLoopRepairEndPcList;

}

void CodeGener::GenerateContinueStat(ContinueStat* stat) {
	if (m_curLoopRepairEndPcList == nullptr) {
		throw CodeGenerException("Cannot use break in acyclic scope");
	}
	m_curFunc->instrSect.EmitJmp(m_curLoopStartPc);		// ���ص�ǰѭ������ʼpc
}

void CodeGener::GenerateBreakStat(BreakStat* stat) {
	if (m_curLoopRepairEndPcList == nullptr) {
		throw CodeGenerException("Cannot use break in acyclic scope");
	}
	m_curLoopRepairEndPcList->push_back(m_curFunc->instrSect.GetPc() + 1);
	m_curFunc->instrSect.EmitJmp(0);		// �޷���ǰ��֪����pc��������޸�pc���ȴ��޸�
}


void CodeGener::GenerateExp(Exp* exp) {
	switch (exp->GetType())
	{
	case ExpType::kNull: {
		auto constIdx = AllocConst(std::make_unique<NullValue>());
		m_curFunc->instrSect.EmitPushK(constIdx);
		break;
	}
	case ExpType::kBool: {
		auto boolexp = static_cast<BoolExp*>(exp);
		auto constIdx = AllocConst(std::make_unique<BoolValue>(boolexp->value));
		m_curFunc->instrSect.EmitPushK(constIdx);
		break;
	}
	case ExpType::kNumber: {
		auto numexp = static_cast<NumberExp*>(exp);
		auto constIdx = AllocConst(std::make_unique<NumberValue>(numexp->value));
		m_curFunc->instrSect.EmitPushK(constIdx);
		break;
	}
	case ExpType::kString: {
		auto strexp = static_cast<StringExp*>(exp);
		auto constIdx = AllocConst(std::make_unique<StringValue>(strexp->value));
		m_curFunc->instrSect.EmitPushK(constIdx);
		break;
	}
	case ExpType::kName: {
		// ��ȡ����ֵ�Ļ������ҵ���Ӧ�ı�����ţ�������ջ
		auto nameExp = static_cast<NameExp*>(exp);

		auto varIdx = GetVar(nameExp->name);
		if (varIdx == -1) {
			throw CodeGenerException("var not defined");
		}

		m_curFunc->instrSect.EmitPushV(varIdx);	// �ӱ����л�ȡ


		break;
	}
	case ExpType::kBinaOp: {
		auto binaOpExp = static_cast<BinaOpExp*>(exp);

		// ���ұ��ʽ��ֵ��ջ
		GenerateExp(binaOpExp->leftExp.get());
		GenerateExp(binaOpExp->rightExp.get());

		// ��������ָ��
		switch (binaOpExp->oper) {
		case TokenType::kOpAdd:
			m_curFunc->instrSect.EmitAdd();
			break;
		case TokenType::kOpSub:
			m_curFunc->instrSect.EmitSub();
			break;
		case TokenType::kOpMul:
			m_curFunc->instrSect.EmitMul();
			break;
		case TokenType::kOpDiv:
			m_curFunc->instrSect.EmitDiv();
			break;
		case TokenType::kOpNe:
			m_curFunc->instrSect.EmitNe();
			break;
		case TokenType::kOpEq:
			m_curFunc->instrSect.EmitEq();
			break;
		case TokenType::kOpLt:
			m_curFunc->instrSect.EmitLt();
			break;
		case TokenType::kOpLe:
			m_curFunc->instrSect.EmitLe();
			break;
		case TokenType::kOpGt:
			m_curFunc->instrSect.EmitGt();
			break;
		case TokenType::kOpGe:
			m_curFunc->instrSect.EmitGe();
			break;
		default:
			throw CodeGenerException("Unrecognized binary operator");
		}
		break;
	}
	case ExpType::kFuncCall: {
		auto funcCallExp = static_cast<FuncCallExp*>(exp);

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

		m_curFunc->instrSect.EmitPushK(AllocConst(std::make_unique<NumberValue>(funcCallExp->parList.size())));

		// ����ԭ�ʹ���ڱ�������
		m_curFunc->instrSect.EmitCall(varIdx);
		break;
	}
	default:
		break;
	}
}


} // namespace codegener