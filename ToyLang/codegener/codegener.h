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

namespace codegener {

struct Scope {
	value::FunctionBodyValue* m_func;		// ��������������
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
	CodeGener(value::ValueSection* t_constSect);

	void EntryScope(value::FunctionBodyValue* subFunc = nullptr);

	void ExitScope();

	uint32_t AllocConst(std::unique_ptr<value::Value> value);

	uint32_t AllocVar(std::string varName);
;
	uint32_t GetVar(std::string varName);


	void RegistryFunctionBridge(std::string funcName, value::FunctionBridgeCall funcAddr);


	void Generate(ast::BlockStat* block, value::ValueSection* constSect);

	void GenerateBlock(ast::BlockStat* block);

	void GenerateStat(ast::Stat* stat);

	void GenerateFuncDefStat(ast::FuncDefStat* stat);

	void GenerateReturnStat(ast::ReturnStat* stat);

	void GenerateNewVarStat(ast::NewVarStat* stat);

	void GenerateAssignStat(ast::AssignStat* stat);

	void GenerateIfStat(ast::IfStat* stat);

	void GenerateWhileStat(ast::WhileStat* stat);

	void GenerateContinueStat(ast::ContinueStat* stat);

	void GenerateBreakStat(ast::BreakStat* stat);


	void GenerateExp(ast::Exp* exp);



private:

	// �������
	value::FunctionBodyValue* m_curFunc;		// ��ǰ���ɺ���

	// �������
	std::map<value::Value*, uint32_t> m_constMap;		// ��ʱ�����⣬ָ���û�취������<��
	value::ValueSection* m_constSect;		// ȫ�ֳ�����

	// ���������
	std::vector<Scope> m_scope;

	// ѭ�����
	uint32_t m_curLoopStartPc;
	std::vector<uint32_t>* m_curLoopRepairEndPcList;
};

} // namespace codegener

#endif // CODEGENER_CODEGENER_H_