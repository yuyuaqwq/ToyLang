#ifndef VM_VM_H_
#define VM_VM_H_

#include <stdint.h>

#include <vector>
#include <string>
#include <memory>


#include "vm/instr.h"
#include "vm/value.h"

namespace codegener{
class CodeGener;
}

namespace vm {

class VMException : public std::exception {
public:
	VMException(const char* t_msg) : std::exception(t_msg) {

	}
};


class VM {
public:
	friend class codegener::CodeGener;

public:
	VM() : m_mainFunc{ 0 }, m_pc{ 0 }, m_curFunc{ 0 }{
		m_curFunc = &m_mainFunc;
	}

	std::string Disassembly() {
		return m_curFunc->Disassembly();
	}

	Value* GetVar(uint32_t idx) {
		if (m_curFunc->varSect[idx]->GetType() == ValueType::kUp) {
			// upvalue�����γ�����(�������ɽ׶Σ����ݴ���������������ҵ��˱��������Ǹñ���ʵ����Ҳ��upvalue)�����Ҫ�ظ�������ֱ������upvalue
			// ��ʱ����ԴӴ��������Ǳ��Ż���Ҳ����ѭ�������ң�ֱ������ָ��upvalue
			auto func = m_curFunc;
			auto upvalue = func->varSect[idx]->GetUp();
			while (upvalue->funcProto->varSect[upvalue->index]->GetType() == ValueType::kUp) {
				func = upvalue->funcProto;
				upvalue = func->varSect[upvalue->index]->GetUp();
			}
			return upvalue->funcProto->varSect[upvalue->index].get();
		}
		return m_curFunc->varSect[idx].get();
	}

	std::unique_ptr<Value> GetVarCopy(uint32_t idx) {
		return GetVar(idx)->Copy();
	}

	void SetVar(uint32_t idx, std::unique_ptr<Value> var) {
		if (idx >= m_curFunc->varSect.size()) {
			m_curFunc->varSect.resize(idx + 1);
		}
		else if (m_curFunc->varSect[idx].get() && m_curFunc->varSect[idx]->GetType() == ValueType::kUp) {
			auto func = m_curFunc;
			auto upvalue = func->varSect[idx]->GetUp();
			while (upvalue->funcProto->varSect[upvalue->index]->GetType() == ValueType::kUp) {
				func = upvalue->funcProto;
				upvalue = func->varSect[upvalue->index]->GetUp();
			}
			upvalue->funcProto->varSect[upvalue->index] = std::move(var);
			return;
		}
		m_curFunc->varSect[idx] = std::move(var);
	}

	void SetVar(uint32_t idx, Value* var) {
		SetVar(idx, var->Copy());
	}


	void Run() {
		do {
			auto opcode = m_curFunc->instrSect.GetOpcode(m_pc++);
			switch (opcode) {
			case OpcodeType::kStop:
				return;
			case OpcodeType::kNop:
				break;
			case OpcodeType::kPushK: {
				auto constIdx = m_curFunc->instrSect.GetU32(m_pc);
				m_pc += 4;
				m_stackSect.push_back(m_constSect[constIdx]->Copy());
				break;
			}
			case OpcodeType::kPushV: {
				auto varIdx = m_curFunc->instrSect.GetU32(m_pc);
				m_pc += 4;
				m_stackSect.push_back(GetVarCopy(varIdx));
				break;
			}
			case OpcodeType::kPop: {
				m_stackSect.pop_back();
				break;
			}
			case OpcodeType::kPopV: {
				auto& a = m_stackSect[m_stackSect.size() - 1];
				auto varIdx = m_curFunc->instrSect.GetU32(m_pc);
				m_pc += 4;
				SetVar(varIdx, std::move(a));
				m_stackSect.pop_back();
				break;
			}
			case OpcodeType::kAdd: {
				auto& a = m_stackSect[m_stackSect.size() - 1];
				auto& b = m_stackSect[m_stackSect.size() - 2];
				b->GetNumber()->value += a->GetNumber()->value;
				m_stackSect.pop_back();
				break;
			}
			case OpcodeType::kSub: {
				auto& a = m_stackSect[m_stackSect.size() - 1];
				auto& b = m_stackSect[m_stackSect.size() - 2];
				b->GetNumber()->value -= a->GetNumber()->value;
				m_stackSect.pop_back();
				break;
			}
			case OpcodeType::kMul: {
				auto& a = m_stackSect[m_stackSect.size() - 1];
				auto& b = m_stackSect[m_stackSect.size() - 2];
				b->GetNumber()->value *= a->GetNumber()->value;
				m_stackSect.pop_back();
				break;
			}
			case OpcodeType::kDiv: {
				auto& a = m_stackSect[m_stackSect.size() - 1];
				auto& b = m_stackSect[m_stackSect.size() - 2];
				b->GetNumber()->value /= a->GetNumber()->value;
				m_stackSect.pop_back();
				break;
			}
			case OpcodeType::kCall: {
				// �Ƚ�ջ�еĲ����ƶ�����������

				auto varIdx = m_curFunc->instrSect.GetU32(m_pc);
				m_pc += 4;
				auto value = GetVar(varIdx)->GetFunctionProto()->value;

				if (value->GetType() == ValueType::kFunctionBody) {
					
					auto callFunc = value->GetFunctionBody();

					printf("%s\n", callFunc->Disassembly().c_str());

					auto saveFunc = m_curFunc;
					auto savePc = m_pc;

					// �л�����
					m_curFunc = callFunc;
					m_pc = 0;

					// �ƶ�ջ�ϵĲ������º����ľֲ�������
					//m_varIdxBase = m_varSect.size();
					for (int i = m_curFunc->parCount - 1; i >= 0; i--) {
						SetVar(i, std::move(m_stackSect[m_stackSect.size() - 1]));
						m_stackSect.pop_back();
					}

					// ���浱ǰ����
					m_stackSect.push_back(std::make_unique<NumberValue>((uint64_t)saveFunc));
					m_stackSect.push_back(std::make_unique<NumberValue>(savePc));
					
				}
				else if (value->GetType() == ValueType::kFunctionBridge) {
					auto callFunc = value->GetFunctionBirdge();
					callFunc->funcAddr(1, &m_stackSect);
				}
				else {
					throw VMException("Wrong call type");
				}
				break;
			}
			case OpcodeType::kRet: {
				auto& pc = m_stackSect[m_stackSect.size() - 1];
				auto& curFunc = m_stackSect[m_stackSect.size() - 2];

				// �ָ�����
				m_curFunc = (FunctionBodyValue*)curFunc->GetNumber()->value;
				m_pc = pc->GetNumber()->value;

				m_stackSect.pop_back();
				m_stackSect.pop_back();
				break;
			}
			default:
				throw VMException("Unknown instruction");
			}
		} while (true);
	}

private:
	uint32_t m_pc;
	FunctionBodyValue m_mainFunc;
	FunctionBodyValue* m_curFunc;
	std::vector<std::unique_ptr<Value>> m_stackSect;
	std::vector<std::unique_ptr<Value>> m_constSect;
	
};

} // namespace vm 

#endif // VM_VM_H_