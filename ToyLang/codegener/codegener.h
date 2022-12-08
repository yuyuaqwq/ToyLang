#ifndef CODEGENER_CODEGENER_H_
#define CODEGENER_CODEGENER_H_

#include <vector>
#include <map>
#include <set>
#include <string>

#include "vm/instr.h"
#include "vm/vm.h"

#include "value/value.h"
#include "value/section.h"

#include "ast/stat.h"
#include "ast/exp.h"

namespace toylang {

struct Scope {
	FunctionBodyValue* m_func;		// ��������������
	uint32_t varCount;		// ��ǰ�����ڵ�ǰ�������е���Ч��������
	std::map<std::string, uint32_t> m_varTable;		// ������keyΪ��������
};



// ��������ʱ�������쳣
class CodeGenerException : public std::exception{
public:
	CodeGenerException(const char* t_msg);
};


class CodeGener {
public:
	CodeGener(ValueSection* t_constSect);

public:
	void EntryScope(FunctionBodyValue* subFunc = nullptr);
	void ExitScope();
	uint32_t AllocConst(std::unique_ptr<Value> value);
	uint32_t AllocVar(std::string varName);
	uint32_t GetVar(std::string varName);
	void RegistryFunctionBridge(std::string funcName, FunctionBridgeCall funcAddr);

	void Generate(BlockStat* block, ValueSection* constSect);
	void GenerateBlock(BlockStat* block);
	void GenerateStat(Stat* stat);
	void GenerateFuncDefStat(FuncDefStat* stat);
	void GenerateReturnStat(ReturnStat* stat);
	void GenerateNewVarStat(NewVarStat* stat);
	void GenerateAssignStat(AssignStat* stat);
	void GenerateIfStat(IfStat* stat);
	void GenerateWhileStat(WhileStat* stat);
	void GenerateContinueStat(ContinueStat* stat);
	void GenerateBreakStat(BreakStat* stat);
	void GenerateExp(Exp* exp);

private:

	// �������
	FunctionBodyValue* m_curFunc;		// ��ǰ���ɺ���

	// �������
	std::map<Value*, uint32_t> m_constMap;		// ��ʱ�����⣬ָ���û�취������<��
	ValueSection* m_constSect;		// ȫ�ֳ�����

	// ���������
	std::vector<Scope> m_scope;

	// ѭ�����
	uint32_t m_curLoopStartPc;
	std::vector<uint32_t>* m_curLoopRepairEndPcList;
};

} // namespace codegener

#endif // CODEGENER_CODEGENER_H_