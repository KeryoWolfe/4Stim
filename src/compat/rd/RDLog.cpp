// 4Stim: RDSupport.h's logging, into 4Stim.log.
#include "RDSupport.h"

namespace RD
{
	void LogErrorText(const std::string& a_text) { REX::ERROR("{}", a_text); }
	void LogWarnText(const std::string& a_text) { REX::WARN("{}", a_text); }
	void LogInfoText(const std::string& a_text) { REX::INFO("{}", a_text); }
}
