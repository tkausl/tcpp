#define TCPP_IMPLEMENTATION
#include "tcppLibrary.hpp"
#include <memory>
#include <iostream>



int main() {
	std::string src = R"(
__COUNTER__ __COUNTER__
#define Foo(a, ...) Hello a world __VA_ARGS__ !
Foo(Bar, baz, bay)
)";


	tcpp::Preprocessor::TPreprocessorConfigInfo cfg{};

	cfg.mOnErrorCallback = [](const tcpp::TErrorInfo& ei) {
		if (ei.msg.length())
			std::cout << ei.msg << '\n';
		else
			std::cout << tcpp::ErrorTypeToString(ei.mType) << " - " << ei.mLine << '\n';
	};

	cfg.mOnIncludeCallback = [](const std::string& file, bool system) {
		return std::string("#define foo Hello");
	};

	tcpp::Preprocessor pp(cfg);
	auto result = pp.Process(src);
	std::cout << result << '\n';
}