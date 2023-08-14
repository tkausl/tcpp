/*!
	\file tcppLibrary.hpp
	\date 29.12.2019
	\author Ildar Kasimov

	This file is a single-header library which was written in C++14 standard.
	The main purpose of the library is to implement simple yet flexible C/C++ preprocessor.
	We've hardly tried to stick to C98 preprocessor's specifications, but if you've found
	some mistakes or inaccuracies, please, make us to know about them. The project has started
	as minimalistic implementation of preprocessor for GLSL and HLSL languages.

	The usage of the library is pretty simple, just copy this file into your enviornment and
	predefine TCPP_IMPLEMENTATION macro before its inclusion like the following

	\code
		#define TCPP_IMPLEMENTATION
		#include "tcppLibrary.hpp"
	\endcode

	Because of the initial goal of the project wasn't full featured C preprocessor but its subset
	that's useful for GLSL and HLSL, some points from the specification of the C preprocessor could
	not be applied to this library, at least for now. There is a list of unimplemented features placed
	below

	\todo Implement support of char literals
	\todo Improve existing performance for massive input files
	\todo Add support of integral literals like L, u, etc
	\todo Implement built-in directives like #pragma, #error and others
	\todo Provide support of variadic macros
*/
#pragma once

#ifdef __INTELLISENSE__
#define TCPP_IMPLEMENTATION
#endif


#include <string>
#include <functional>
#include <vector>
#include <list>
#include <algorithm>
#include <tuple>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <sstream>
#include <memory>
#include <optional>

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)
#define ON_SCOPE_EXIT ScopeExitHandler CONCAT(on_exit_, __COUNTER__) = [&]()

template<typename Fn>
struct ScopeExitHandler {
	ScopeExitHandler(Fn fn)
		: fn(fn) {}

	~ScopeExitHandler() {
		fn();
	}
	Fn fn;
};


///< Library's configs
#define TCPP_DISABLE_EXCEPTIONS 1


#if TCPP_DISABLE_EXCEPTIONS
#define TCPP_NOEXCEPT noexcept
#else
#define TCPP_NOEXCEPT 
#endif

#include <cassert>
#define TCPP_ASSERT(assertion) assert(assertion)


namespace tcpp
{
/*!
	interface IInputStream

	\brief The interface describes the functionality that all input streams should
	provide
*/

struct Location {
	int line = 0;
	int column = 0;
};

class CharStream {
public:
	CharStream(std::string src)
		: src(src) {}

	char next() {
		if (i == src.length()) return 0;
		char ch = src[i];
		i++;
		if (ch == '\r') return next();
		_observe(ch);
		return ch;
	}

	bool next(char ch) {
		if (peek() == ch) {
			next();
			return true;
		}
		return false;
	}
	
	char peek(int offset = 1) {
		if (i + offset - 1 >= src.length()) return 0;
		return src[i + offset - 1];
	}

	char prev(int offset = 1) {
		if (marked_i < offset) return 0;
		return src[marked_i - offset];

	}

	bool eof() {
		return i == src.length();
	}

	Location location() const {
		return loc;
	}

	void mark() {
		marked_i = i;
		marked_loc = loc;
	}

	std::string GetRegion() {
		return src.substr(marked_i, i - marked_i);
	}

	std::pair<Location, Location> GetRegionLocation() {
		return { marked_loc, loc };
	}

private:
	void _observe(char c) {
		if (c == '\n') {
			loc.line++;
			loc.column = 0;
		}
		else {
			loc.column++;
		}
	}
	std::string src;

	size_t i = 0;
	Location loc = {};

	size_t marked_i = 0;
	Location marked_loc = {};
};

template<typename Callable>
size_t consume_while(CharStream& s, const Callable& predicate) {
	size_t r = 0;
	while (!s.eof() && predicate(s.peek())) {
		s.next();
		r++;
	}
	return r;
}

template<typename Callable>
size_t consume_until(CharStream& s, const Callable& predicate) {
	size_t r = 0;
	while (!s.eof() && !predicate(s.peek())) {
		s.next();
		r++;
	}
	return r;
}

template<typename Callable>
size_t consume_until_including(CharStream& s, const Callable& predicate) {
	size_t r = 0;
	while (!s.eof() && !predicate(s.peek())) {
		s.next();
		r++;
	}
	if (!s.eof()) {
		s.next();
		r++;
	}
	return r;
}

#define ENUMERATE_TOKEN_TYPES(ENUM) \
ENUM(IDENTIFIER) \
ENUM(HASH) \
ENUM(DOTDOTDOT) \
ENUM(SPACE) \
ENUM(BLOB) \
ENUM(OPEN_BRACKET) \
ENUM(CLOSE_BRACKET) \
ENUM(COMMA) \
ENUM(NEWLINE) \
ENUM(LESS) \
ENUM(GREATER) \
ENUM(QUOTES) \
ENUM(KEYWORD) \
ENUM(END) \
ENUM(REJECT_MACRO) \
ENUM(STRINGIZE_OP) \
ENUM(CONCAT_OP) \
ENUM(NUMBER) \
ENUM(PLUS) \
ENUM(MINUS) \
ENUM(SLASH) \
ENUM(STAR) \
ENUM(OR) \
ENUM(AND) \
ENUM(AMPERSAND) \
ENUM(VLINE) \
ENUM(LSHIFT) \
ENUM(RSHIFT) \
ENUM(NOT) \
ENUM(GE) \
ENUM(LE) \
ENUM(EQ) \
ENUM(NE) \
ENUM(SEMICOLON) \
ENUM(COMMENTARY) \
ENUM(UNKNOWN) \


enum class E_TOKEN_TYPE : unsigned int
{
#define ENUM(val) val,
ENUMERATE_TOKEN_TYPES(ENUM)
#undef ENUM
};

static const char* token_type_to_string(E_TOKEN_TYPE tt) {
	switch (tt) {
#define ENUM(val) case E_TOKEN_TYPE::val: return #val;
		ENUMERATE_TOKEN_TYPES(ENUM)
#undef ENUM
	}
	return "<unknown>";
}


/*!
	struct TToken

	\brief The structure is a type to contain all information about single token
*/

struct TToken
{
	E_TOKEN_TYPE mType;

	std::string mRawView;

	Location start;
	Location end;

	static TToken EOFToken;
};

#ifdef TCPP_IMPLEMENTATION
TToken TToken::EOFToken = { E_TOKEN_TYPE::END };
#endif

/*!
	class Lexer

	\brief The class implements lexer's functionality
*/

class TokenStream {
public:
	virtual ~TokenStream() = default;

	virtual TToken Next() noexcept = 0;
	virtual bool HasNext() noexcept = 0;
	TToken NextNonSpace() noexcept {
		auto t = Next();
		while (t.mType == E_TOKEN_TYPE::SPACE)
			t = Next();
		return t;
	}
};

class ReplayTokenStream : public TokenStream {
public:
	ReplayTokenStream(std::vector<TToken> tokens)
		: m_tokens(tokens) {}

	virtual TToken Next() noexcept {
		if (pos < m_tokens.size()) return m_tokens[pos++];
		return TToken::EOFToken;
	}

	virtual bool HasNext() noexcept {
		return pos < m_tokens.size();
	}

	std::vector<TToken> m_tokens;
	size_t pos = 0;
};

template<typename It1, typename It2>
class IteratorTokenStream : public TokenStream {
public:
	IteratorTokenStream(It1 it, It2 end)
		: it(it),
		end(end) {}

	virtual TToken Next() noexcept {
		if(it == end)
			return TToken::EOFToken;
		return *it++;
	}

	virtual bool HasNext() noexcept {
		return it != end;
	}

	It1 it;
	It2 end;
};

class RecordTokenStream : public TokenStream {
public:
	RecordTokenStream(TokenStream* base)
		: m_base(base) {}

	virtual TToken Next() noexcept override {
		m_storage.push_back(m_base->Next());
		return m_storage.back();
	}
	virtual bool HasNext() noexcept override {
		return m_base->HasNext();
	}

	ReplayTokenStream* Replay() noexcept {
		return new ReplayTokenStream(m_storage);
	}

	TokenStream* m_base;
	std::vector<TToken> m_storage;
};

class Lexer : public TokenStream
{
public:
	Lexer() TCPP_NOEXCEPT = delete;
	explicit Lexer(std::string pIinputStream) TCPP_NOEXCEPT;
	~Lexer() TCPP_NOEXCEPT = default;


	virtual TToken Next() noexcept override;
	virtual bool HasNext() noexcept override;
private:

	TToken makeToken(E_TOKEN_TYPE type);

	CharStream m_stream;
};


/*!
	struct TMacroDesc

	\brief The type describes a single macro definition's description
*/

class Preprocessor;

struct TMacroDesc
{
	std::string mName;
	std::vector<std::string> mArgsNames;
	std::vector<TToken> mValue;
	std::function<bool(const Preprocessor&, const TToken&, std::vector<TToken>&)> custom;
	bool isFunc;
	bool isVaArg;
};


enum class E_ERROR_TYPE : unsigned int
{
	UNEXPECTED_TOKEN,
	UNBALANCED_ENDIF,
	INVALID_MACRO_DEFINITION,
	MACRO_ALREADY_DEFINED,
	INCONSISTENT_MACRO_ARITY,
	UNDEFINED_MACRO,
	INVALID_INCLUDE_DIRECTIVE,
	UNEXPECTED_END_OF_INCLUDE_PATH,
	ANOTHER_ELSE_BLOCK_FOUND,
	ELIF_BLOCK_AFTER_ELSE_FOUND,
	UNDEFINED_DIRECTIVE,
};


std::string ErrorTypeToString(const E_ERROR_TYPE& errorType) TCPP_NOEXCEPT;


/*!
	struct TErrorInfo

	\brief The type contains all information about happened error
*/

struct TErrorInfo
{
	E_ERROR_TYPE mType;
	size_t mLine;
	std::string msg;
};

class MacroCollection {
public:
	bool Add(const TMacroDesc& desc, bool override = true) {
		auto exists = m_macros.find(desc.mName) != m_macros.end();
		if (override || !exists)
			m_macros.emplace(std::make_pair(desc.mName, desc));
		return exists;
	}

	bool Contains(const std::string& name) const {
		return m_macros.find(name) != m_macros.end();
	}

	bool Remove(const std::string& name) {
		return m_macros.erase(name) == 1;
	}

	std::optional<TMacroDesc> Get(const std::string& name) const {
		auto it = m_macros.find(name);
		if (it == m_macros.end()) return {};
		return it->second;
	}
private:

	std::unordered_map<std::string, TMacroDesc> m_macros;
};

class Preprocessor
{
public:
	using TOnErrorCallback = std::function<void(const TErrorInfo&)>;
	using TOnIncludeCallback = std::function<std::string(const std::string&, bool)>;
	using TDirectiveHandler = std::function<std::string(Preprocessor&, Lexer&, const std::string&)>;

	struct TPreprocessorConfigInfo
	{
		TOnErrorCallback   mOnErrorCallback = {};
		TOnIncludeCallback mOnIncludeCallback = {};

		bool               mSkipComments = false; ///< When it's true all tokens which are E_TOKEN_TYPE::COMMENTARY will be thrown away from preprocessor's output
	};

	struct ConditionContext
	{
		bool enabled;
		bool haselse = false;
		bool entered;
		bool parentEnabled = true;

		// Remember these for diagnostics
		TToken ifToken;
		TToken elseToken;

		ConditionContext(bool enabled)
			: enabled(enabled),
			entered(enabled) {}
	};

	using ConditionStack = std::stack<ConditionContext>;
public:
	Preprocessor() TCPP_NOEXCEPT = delete;
	Preprocessor(const Preprocessor&) TCPP_NOEXCEPT = delete;
	Preprocessor(const TPreprocessorConfigInfo& config) TCPP_NOEXCEPT;
	~Preprocessor() TCPP_NOEXCEPT = default;

	std::string Process(std::string) TCPP_NOEXCEPT;

	Preprocessor& operator= (const Preprocessor&) TCPP_NOEXCEPT = delete;

	MacroCollection& GetSymbolsTable() TCPP_NOEXCEPT;
	const MacroCollection& GetSymbolsTable() const noexcept { return mSymTable; }
public:
	void PushConditionContext(ConditionContext ctx) {
		ctx.parentEnabled = m_conditionStack.empty() || m_conditionStack.top().enabled;
		m_conditionStack.push(ctx);
	}

	bool HandleDirective(TokenStream*) TCPP_NOEXCEPT;
	void _createMacroDefinition(TokenStream* ts) TCPP_NOEXCEPT;
	void _removeMacroDefinition(const std::string& macroName) TCPP_NOEXCEPT;

	bool _expandMacroDefinition(const TMacroDesc& macroDesc, const TToken& idToken, TokenStream& ts, std::vector<TToken>&) const TCPP_NOEXCEPT;
	bool _expandSimpleMacro(const TMacroDesc& macroDesc, const TToken& idToken, TokenStream& ts, std::vector<TToken>&) const TCPP_NOEXCEPT;
	bool _expandFunctionMacro(const TMacroDesc& macroDesc, const TToken& idToken, TokenStream& ts, std::vector<TToken>&) const TCPP_NOEXCEPT;

public:
	template<typename... Ts>
	void _expect(const TToken& token, const Ts&... expectedType) const TCPP_NOEXCEPT {
		if (((token.mType != expectedType) && ...)) {
			std::string msg = std::to_string(token.start.line + 1) + ":" + std::to_string(token.start.column + 1) + ": Expected Token type ";
			msg += ((std::string(token_type_to_string(expectedType)) + ", ") + ...);
			msg += std::string("found ") + token_type_to_string(token.mType);
			mOnErrorCallback({ E_ERROR_TYPE::UNEXPECTED_TOKEN, (size_t)token.start.line, msg });
		}
	}

	void _processInclusion(TokenStream& ts) TCPP_NOEXCEPT;

	ConditionContext _processIfConditional(TokenStream& ts) TCPP_NOEXCEPT;
	ConditionContext _processIfdefConditional(TokenStream& ts) TCPP_NOEXCEPT;
	ConditionContext _processIfndefConditional(TokenStream& ts) TCPP_NOEXCEPT;
	void _processElseConditional(TokenStream& ts, ConditionContext& currStackEntry) TCPP_NOEXCEPT;
	void _processElifConditional(TokenStream& ts, ConditionContext& currStackEntry) TCPP_NOEXCEPT;

	int _evaluateExpression(const std::vector<TToken>& exprTokens) const TCPP_NOEXCEPT;

	bool _shouldTokenBeSkipped() const TCPP_NOEXCEPT;
public:

	TOnErrorCallback   mOnErrorCallback;
	TOnIncludeCallback mOnIncludeCallback;

	MacroCollection mSymTable;

	struct ExpansionContext {
		std::string symbol;
		Location start;
		Location end;
	};

	mutable std::vector<ExpansionContext> mContextStack;
	ConditionStack m_conditionStack;

	bool mSkipCommentsTokens;
};


///< implementation of the library is placed below
#if defined(TCPP_IMPLEMENTATION)


std::string ErrorTypeToString(const E_ERROR_TYPE& errorType) TCPP_NOEXCEPT
{
	switch (errorType)
	{
	case E_ERROR_TYPE::UNEXPECTED_TOKEN:
		return "Unexpected token";
	case E_ERROR_TYPE::UNBALANCED_ENDIF:
		return "Unbalanced endif";
	case E_ERROR_TYPE::INVALID_MACRO_DEFINITION:
		return "Invalid macro definition";
	case E_ERROR_TYPE::MACRO_ALREADY_DEFINED:
		return "The macro is already defined";
	case E_ERROR_TYPE::INCONSISTENT_MACRO_ARITY:
		return "Inconsistent number of arguments between definition and invocation of the macro";
	case E_ERROR_TYPE::UNDEFINED_MACRO:
		return "Undefined macro";
	case E_ERROR_TYPE::INVALID_INCLUDE_DIRECTIVE:
		return "Invalid #include directive";
	case E_ERROR_TYPE::UNEXPECTED_END_OF_INCLUDE_PATH:
		return "Unexpected end of include path";
	case E_ERROR_TYPE::ANOTHER_ELSE_BLOCK_FOUND:
		return "#else directive should be last one";
	case E_ERROR_TYPE::ELIF_BLOCK_AFTER_ELSE_FOUND:
		return "#elif found after #else block";
	case E_ERROR_TYPE::UNDEFINED_DIRECTIVE:
		return "Undefined directive";
	}

	return "";
}

Lexer::Lexer(std::string pIinputStream) TCPP_NOEXCEPT:
m_stream(std::move(pIinputStream))
{
}

bool Lexer::HasNext() TCPP_NOEXCEPT
{
	return !m_stream.eof();
}

static void ConsumeLine(CharStream& stream) TCPP_NOEXCEPT
{
	consume_until_including(stream, [](char c) { return c == '\n'; });
}


static void ConsumeMultilineComment(CharStream& stream) TCPP_NOEXCEPT
{
	do {
		consume_until_including(stream, [](char c) {
			return c == '*';
		});
	} while (!stream.next('/'));
}

bool ishexdigit(char c) {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool is_space_non_nl(char c) {
	return c == ' ' || c == '\t';
}

TToken Lexer::makeToken(E_TOKEN_TYPE type) {
	auto tmp = m_stream.GetRegionLocation();
	return {
		type,
		m_stream.GetRegion(),
		std::get<0>(tmp),
		std::get<1>(tmp)
	};
}

TToken Lexer::Next() TCPP_NOEXCEPT
{
	if (m_stream.eof())
		return TToken::EOFToken;

	m_stream.mark();
	auto ch = m_stream.next();

	switch (ch)
	{
	case ',':
		return makeToken(E_TOKEN_TYPE::COMMA);
	case '(':
		return makeToken(E_TOKEN_TYPE::OPEN_BRACKET);
	case ')':
		return makeToken(E_TOKEN_TYPE::CLOSE_BRACKET);
	case '<':
	{
		if (m_stream.next('<'))
			return makeToken(E_TOKEN_TYPE::LSHIFT);
		if (m_stream.next('='))
			return makeToken(E_TOKEN_TYPE::LE);
	}

	return makeToken(E_TOKEN_TYPE::LESS);
	case '>':
		if (m_stream.next('>'))
			return makeToken(E_TOKEN_TYPE::RSHIFT);
		if (m_stream.next('='))
			return makeToken(E_TOKEN_TYPE::GE);
		return makeToken(E_TOKEN_TYPE::GREATER);
	case '\"':
		return makeToken(E_TOKEN_TYPE::QUOTES);
	case '+':
		return makeToken(E_TOKEN_TYPE::PLUS);
	case '-':
		return makeToken(E_TOKEN_TYPE::MINUS);
	case '*':
		return makeToken(E_TOKEN_TYPE::STAR);
	case '/':
	{
		if (m_stream.next('/')) // \note Found a single line C++ style comment
		{
			ConsumeLine(m_stream);
			return makeToken(E_TOKEN_TYPE::COMMENTARY);
		}
		else if (m_stream.next('*')) /// \note multi-line commentary
		{
			ConsumeMultilineComment(m_stream);
			return makeToken(E_TOKEN_TYPE::COMMENTARY);
		}
		return makeToken(E_TOKEN_TYPE::SLASH);
	}
	case '&':
		if (m_stream.next('&'))
		{
			return makeToken(E_TOKEN_TYPE::AND);
		}

		return makeToken(E_TOKEN_TYPE::AMPERSAND);
	case '|':
		if (m_stream.next('|'))
		{
			return makeToken(E_TOKEN_TYPE::OR);
		}

		return makeToken(E_TOKEN_TYPE::VLINE);
	case '!':
		if (m_stream.next('='))
		{
			return makeToken(E_TOKEN_TYPE::NE);
		}

		return makeToken(E_TOKEN_TYPE::NOT);
	case '=':
		if (m_stream.next('='))
		{
			return makeToken(E_TOKEN_TYPE::EQ);
		}

		return makeToken(E_TOKEN_TYPE::BLOB);

	case ';':
		return makeToken(E_TOKEN_TYPE::SEMICOLON);
	case '.': {
		if (m_stream.peek(1) == '.' && m_stream.peek(2) == '.') {
			m_stream.next();
			m_stream.next();
			return makeToken(E_TOKEN_TYPE::DOTDOTDOT);
		}
	}
	}

	if (ch == '\n')
	{
		return makeToken(E_TOKEN_TYPE::NEWLINE);
	}

	if (is_space_non_nl(ch))
	{
		consume_while(m_stream, is_space_non_nl);
		return makeToken(E_TOKEN_TYPE::SPACE);
	}

	if (ch == '#') // \note it could be # operator or a directive
	{
		if (m_stream.next('#')) {
			return makeToken(E_TOKEN_TYPE::CONCAT_OP);
		}
		return makeToken(E_TOKEN_TYPE::HASH);
	}

	if (std::isdigit(ch))
	{
		if (m_stream.next('x')) {
			consume_while(m_stream, ishexdigit);
		}
		else {
			consume_while(m_stream, isdigit);
		}

		return makeToken(E_TOKEN_TYPE::NUMBER);
	}

	if (ch == '_' || std::isalpha(ch)) ///< \note parse identifier
	{
		consume_while(m_stream, [](char c) { return std::isalnum(c) || c == '_'; });
		return makeToken(E_TOKEN_TYPE::IDENTIFIER);
	}

	if (ch == '\\' && m_stream.next('\n')) {
		// Escaped newline. Just loop
		return Next();
	}

	return makeToken(E_TOKEN_TYPE::BLOB);
}

Preprocessor::Preprocessor(const TPreprocessorConfigInfo& config) TCPP_NOEXCEPT:
mOnErrorCallback(config.mOnErrorCallback), mOnIncludeCallback(config.mOnIncludeCallback), mSkipCommentsTokens(config.mSkipComments)
{
	mSymTable.Add({
		.mName = "__LINE__",
		.custom = [](const Preprocessor& pp, const TToken& id, std::vector<TToken>& result) {
			assert(pp.mContextStack.size());
			result.push_back(TToken{
				.mType = E_TOKEN_TYPE::NUMBER,
				.mRawView = std::to_string(pp.mContextStack.front().start.line + 1),
				.start = id.start,
				.end = id.end,
				});
				return true;
			}
		});

	static int count = 0;

	mSymTable.Add({
	.mName = "__COUNTER__",
	.custom = [] (const Preprocessor& pp, const TToken& id, std::vector<TToken>& result) {
		result.push_back(TToken{
			.mType = E_TOKEN_TYPE::NUMBER,
			.mRawView = std::to_string(count++),
			.start = id.start,
			.end = id.end,
			});
			return true;
		}
		});
}

bool Preprocessor::HandleDirective(TokenStream* ts) TCPP_NOEXCEPT {

	auto tok = ts->NextNonSpace();
	if (tok.mType != E_TOKEN_TYPE::IDENTIFIER)
		return false;
	auto directive = tok.mRawView;
	auto directiveToken = tok;
	if (directive == "define") {
		_createMacroDefinition(ts);
	}
	else if (directive == "undef") {
		tok = ts->NextNonSpace();
		_expect(tok, E_TOKEN_TYPE::IDENTIFIER);

		_removeMacroDefinition(tok.mRawView);
		tok = ts->NextNonSpace();
		_expect(tok, E_TOKEN_TYPE::NEWLINE);
	}
	else if (directive == "if") {
		PushConditionContext(_processIfConditional(*ts));
		m_conditionStack.top().ifToken = directiveToken;
	}
	else if (directive == "ifdef") {
		PushConditionContext(_processIfdefConditional(*ts));
		m_conditionStack.top().ifToken = directiveToken;
	}
	else if (directive == "ifndef") {
		PushConditionContext(_processIfndefConditional(*ts));
		m_conditionStack.top().ifToken = directiveToken;
	}
	else if (directive == "elif") {
		_processElifConditional(*ts, m_conditionStack.top());
	}
	else if (directive == "else") {
		_processElseConditional(*ts, m_conditionStack.top());
		m_conditionStack.top().elseToken = directiveToken;
	}
	else if (directive == "endif") {
		if (m_conditionStack.empty())
		{
			mOnErrorCallback({ E_ERROR_TYPE::UNBALANCED_ENDIF, 1 });
		}
		else
		{
			m_conditionStack.pop();
		}
	}
	else if (directive == "include") {
		_processInclusion(*ts);
	}
	else {
		return false;
	}
	return true;
}

std::vector<TToken> fullyexpand(Preprocessor& pp, std::vector<TToken> tokens);

template<typename T>
struct ScopedVarImpl {
	ScopedVarImpl(T& orig, T&& repl)
		: orig(orig),
		storage(std::move(orig)) 
	{
		orig = std::move(repl);
	}

	~ScopedVarImpl() {

	}
	T& orig;
	T storage;
};


#define SCOPED_VAR(var, repl) ScopedVarImpl CONCAT(scoped_var_, __LINE__)(var, repl)

std::string Preprocessor::Process(std::string src) TCPP_NOEXCEPT
{
	auto findSourceLine = [](std::string in, size_t line) {
		size_t off1 = 0;
		size_t off2 = in.find('\n', 0);
		while (off1 != std::string::npos && line) {
			off1 = off2 + 1;
			off2 = in.find('\n', off1);
			--line;
		}
		if (off1 == in.npos || line) return std::string{};
		return in.substr(off1, off2 - off1);
	};

	SCOPED_VAR(m_conditionStack, {});

	Lexer ts{ src };
	std::string processedStr;

	auto appendString = [&processedStr, this](const std::string& str)
	{
		if (_shouldTokenBeSkipped())
		{
			return;
		}

		processedStr.append(str);
	};

	// \note first stage of preprocessing, expand macros and include directives

	while (ts.HasNext())
	{
		std::vector<TToken> line;
		while (1) {
			auto currToken = ts.Next();
			line.push_back(currToken);
			if (currToken.mType == E_TOKEN_TYPE::NEWLINE || currToken.mType == E_TOKEN_TYPE::END)
				break;
		}
		
		auto firstNonSpace = std::find_if(line.begin(), line.end(), [](TToken& tok) { return tok.mType != E_TOKEN_TYPE::SPACE; });
		if (firstNonSpace != line.end() && firstNonSpace->mType == E_TOKEN_TYPE::HASH) {
			IteratorTokenStream replay( firstNonSpace + 1, line.end() );
			auto ident = replay.NextNonSpace();
			if (ident.mType != E_TOKEN_TYPE::IDENTIFIER) {
				// Starts with a Hash but doesn't seem to be a directive
				auto result = fullyexpand(*this, line);
				for (auto t : result)
					appendString(t.mRawView);
				continue;
			}
			replay = { firstNonSpace + 1, line.end() };

			if (!HandleDirective(&replay)) {
				// Unknown directive
			}
			continue;
		}

		if (_shouldTokenBeSkipped())
			continue; // Early out

		auto result = fullyexpand(*this, line);
		for (auto t : result)
			appendString(t.mRawView);
	}

	if (!m_conditionStack.empty()) {
		auto ifToken = m_conditionStack.top().ifToken;
		auto l = ifToken.start.line;
		auto line = findSourceLine(src, l);
		std::string msg = "Unbalanced IF at line " + std::to_string(l) + ":\n";
		msg += line;
		msg += "\n";
		for (int i = 0; i < ifToken.start.column; i++)
			msg += " ";

		for (int i = 0; i < ifToken.end.column - ifToken.start.column; i++)
			msg += "^";
		

		TErrorInfo ei;
		ei.msg = msg;
		mOnErrorCallback(ei);
	}

	return processedStr;
}

MacroCollection& Preprocessor::GetSymbolsTable() TCPP_NOEXCEPT
{
	return mSymTable;
}

bool parseParamList(TMacroDesc& md, Preprocessor& p, TokenStream& lex) {
		md.isFunc = true;
		auto tok = lex.NextNonSpace();

		while (true) {
			p._expect(tok, E_TOKEN_TYPE::IDENTIFIER, E_TOKEN_TYPE::DOTDOTDOT, E_TOKEN_TYPE::CLOSE_BRACKET);
			if (tok.mType == E_TOKEN_TYPE::CLOSE_BRACKET)
				break;
			if (tok.mType == E_TOKEN_TYPE::DOTDOTDOT) {
				md.isVaArg = true;
				tok = lex.NextNonSpace();
				p._expect(tok, E_TOKEN_TYPE::CLOSE_BRACKET);
				break;
			}
			md.mArgsNames.push_back(tok.mRawView);
			tok = lex.NextNonSpace();
			p._expect(tok, E_TOKEN_TYPE::COMMA, E_TOKEN_TYPE::CLOSE_BRACKET);
			if (tok.mType == E_TOKEN_TYPE::COMMA)
				tok = lex.NextNonSpace();
		}
		return true;
}

void parseMacroValue(TMacroDesc& md, Preprocessor& p, TokenStream& lex) {
	TToken tok = lex.NextNonSpace();

	while (tok.mType != E_TOKEN_TYPE::END && tok.mType != E_TOKEN_TYPE::NEWLINE) {
		if (tok.mType == E_TOKEN_TYPE::CONCAT_OP) {
			if (md.mValue.size() && md.mValue.back().mType == E_TOKEN_TYPE::SPACE) {
				md.mValue.resize(md.mValue.size() - 1);
			}
			md.mValue.push_back(tok);
			tok = lex.NextNonSpace();
		}

		md.mValue.push_back(tok);
		tok = lex.Next();
	}
	p._expect(tok, E_TOKEN_TYPE::END, E_TOKEN_TYPE::NEWLINE);
}

void SkipLine(TokenStream* lex) {
	auto tok = lex->Next();
	while (tok.mType != E_TOKEN_TYPE::NEWLINE && tok.mType != E_TOKEN_TYPE::END)
		tok = lex->Next();
}

void Preprocessor::_createMacroDefinition(TokenStream* ts) TCPP_NOEXCEPT
{
	if (_shouldTokenBeSkipped())
	{
		return;
	}

	TMacroDesc macroDesc = {};

	auto currToken = ts->NextNonSpace();
	_expect(currToken, E_TOKEN_TYPE::IDENTIFIER);

	macroDesc.mName = currToken.mRawView;


	currToken = ts->Next();
	switch (currToken.mType)
	{
	case E_TOKEN_TYPE::SPACE:	// object like macro
		parseMacroValue(macroDesc, *this, *ts);
		break;
	case E_TOKEN_TYPE::NEWLINE:
	case E_TOKEN_TYPE::END:
		// No value
		break;
	case E_TOKEN_TYPE::OPEN_BRACKET: // function line macro
		parseParamList(macroDesc, *this, *ts);
		parseMacroValue(macroDesc, *this, *ts);
	break;
	default:
		mOnErrorCallback({ E_ERROR_TYPE::INVALID_MACRO_DEFINITION, 0 });
		break;
	}

	if (mSymTable.Add(macroDesc)) {
		mOnErrorCallback({ E_ERROR_TYPE::MACRO_ALREADY_DEFINED, 0 });
	}
}

void Preprocessor::_removeMacroDefinition(const std::string& macroName) TCPP_NOEXCEPT
{
	if (_shouldTokenBeSkipped())
	{
		return;
	}

	if (!mSymTable.Remove(macroName)) {
		mOnErrorCallback({ E_ERROR_TYPE::UNDEFINED_MACRO, 0 });
	}
}

bool Preprocessor::_expandSimpleMacro(const TMacroDesc& macroDesc, const TToken& idToken, TokenStream& ts, std::vector<TToken>& result) const TCPP_NOEXCEPT
{
	mContextStack.push_back({
		idToken.mRawView,
		idToken.start,
		idToken.end
		});
	result = macroDesc.mValue;
	result.push_back({ E_TOKEN_TYPE::REJECT_MACRO });
	return true;
}

std::vector<TToken> ParseUntilCloseParen(TokenStream& ts) {
	int nesting = 1;
	std::vector<TToken> result;
	auto tok = ts.NextNonSpace();
	while (1) {
		switch (tok.mType) {
		case E_TOKEN_TYPE::OPEN_BRACKET:
			++nesting;
			break;
		case E_TOKEN_TYPE::CLOSE_BRACKET:
			if (!--nesting) goto end;
			break;
		default:
			result.push_back(tok);
		}
		tok = ts.Next();
	}
end:
	// Strip trailing space
	while (result.size() && result.back().mType == E_TOKEN_TYPE::SPACE)
		result.pop_back();
	return result;
}

TToken parseOneArg(TokenStream& ts, TToken tok, std::vector<TToken>& result) {
	int nesting = 1;

	while (1) {
		if (nesting > 1) {
			if (tok.mType == E_TOKEN_TYPE::OPEN_BRACKET) ++nesting;
			if (tok.mType == E_TOKEN_TYPE::CLOSE_BRACKET) --nesting;
			result.push_back(tok);
			continue;
		}


		switch (tok.mType) {
		case E_TOKEN_TYPE::OPEN_BRACKET:
			++nesting;
			result.push_back(tok);
			continue;
		case E_TOKEN_TYPE::CLOSE_BRACKET:
		case E_TOKEN_TYPE::COMMA:
			return tok;
		default:
			result.push_back(tok);
		}

		tok = ts.Next();
	}
}


std::vector<std::vector<TToken>> parseArgList(const TMacroDesc& macro, TokenStream& ts) {
	std::vector<std::vector<TToken>> args;

	auto tok = ts.NextNonSpace();
	if (tok.mType == E_TOKEN_TYPE::CLOSE_BRACKET) {
		// Empty argument list
		return {};
	}

	for (;;) {
		
		tok = parseOneArg(ts, tok, args.emplace_back());

		if (tok.mType == E_TOKEN_TYPE::CLOSE_BRACKET || args.size() == macro.mArgsNames.size())
			break;

		tok = ts.NextNonSpace();
	}

	if (tok.mType == E_TOKEN_TYPE::COMMA) {
		if(macro.isVaArg)
			args.push_back(ParseUntilCloseParen(ts));
		else
			while(tok.mType != E_TOKEN_TYPE::CLOSE_BRACKET)
				tok = parseOneArg(ts, tok, args.emplace_back());

	}
	return args;
}

std::vector<TToken> expandOnce(const Preprocessor& pp, const std::vector<TToken>& tokens) {
	std::vector<TToken> result;
	auto it = tokens.begin();
	for (; it != tokens.end(); ++it) {
		if (it->mType == E_TOKEN_TYPE::IDENTIFIER) {
			auto symb = pp.mSymTable.Get(it->mRawView);
			if (symb) {
				IteratorTokenStream strm(it + 1, tokens.end());
				std::vector<TToken> out;
				if (pp._expandMacroDefinition(*symb, *it, strm, out)) {
					result.insert(result.end(), out.begin(), out.end());
				}
			}
			else {
				result.push_back(*it);
			}
		}
		else {
			result.push_back(*it);
		}
	}
	return result;
}

bool Preprocessor::_expandFunctionMacro(const TMacroDesc& macroDesc, const TToken& idToken, TokenStream& ts, std::vector<TToken>& result) const TCPP_NOEXCEPT
{
	auto currToken = ts.NextNonSpace();

	if (currToken.mType != E_TOKEN_TYPE::OPEN_BRACKET)
		return false;

	ExpansionContext newCtx{
		idToken.mRawView,
		idToken.start,
		idToken.end
	};

	mContextStack.push_back(newCtx);

	auto processingTokens = parseArgList(macroDesc, ts);

	if (
		// Must have either N args...
		processingTokens.size() != macroDesc.mArgsNames.size() 
		// Or N+1 args IFF macro is vaargs
		&& (!macroDesc.isVaArg || (processingTokens.size() != macroDesc.mArgsNames.size() + 1))) {
		mOnErrorCallback({ E_ERROR_TYPE::INCONSISTENT_MACRO_ARITY, 0 });
		return false;
	}

	// \note execute macro's expansion
	std::vector<TToken> replacementList{ macroDesc.mValue.cbegin(), macroDesc.mValue.cend() };
	const auto& argsList = macroDesc.mArgsNames;

	auto maxI = std::min(processingTokens.size(), argsList.size());

	auto findVar = [&](std::string name) {
		for (size_t currArgIndex = 0; currArgIndex < maxI; ++currArgIndex)
		{
			const std::string& currArgName = argsList[currArgIndex];
			auto&& currArgValueTokens = processingTokens[currArgIndex];
			if (currArgName == name)
				return std::optional<std::vector<TToken>>{currArgValueTokens};
		}
		return std::optional<std::vector<TToken>>{};
	};

	for (auto it = replacementList.begin(); it != replacementList.end(); ++it) {
		if (it + 1 != replacementList.end()
			&& (it + 1)->mType == E_TOKEN_TYPE::CONCAT_OP
			&& it + 1 != replacementList.end()) {
			auto left = *it;
			auto right = *(it + 2);
			std::vector<TToken> leftVec = findVar(left.mRawView).value_or(std::vector<TToken>{ left });
			std::vector<TToken> rightVec = findVar(right.mRawView).value_or(std::vector<TToken>{ right });
			// Merge vectors.
			assert(leftVec.size() && rightVec.size());
			leftVec.back().mRawView += rightVec.front().mRawView;

			it = replacementList.erase(it, it + 3);
			it = replacementList.insert(it, leftVec.begin(), leftVec.end());
			it = replacementList.insert(it, rightVec.begin() + 1, rightVec.end());
		}
		else {
			if (it->mType == E_TOKEN_TYPE::IDENTIFIER) {
				auto val = findVar(it->mRawView);
				if (val) {
					auto expanded = expandOnce(*this, *val);
					it = replacementList.erase(it);
					it = replacementList.insert(it, expanded.begin(), expanded.end());
				}
			}
		}
	}

	if (macroDesc.isVaArg)
	{
		std::vector<TToken> tokens;

		for (size_t i = maxI; i < processingTokens.size(); ++i) {
			for (auto& tok : processingTokens[i]) {
				tokens.push_back(tok);
			}
			if (i != processingTokens.size() - 1) {
				tokens.push_back({ E_TOKEN_TYPE::COMMA, ", " });
			}
		}
		for (auto it = replacementList.begin(); it != replacementList.end(); ++it) {
			if ((it->mType != E_TOKEN_TYPE::IDENTIFIER) || (it->mRawView != "__VA_ARGS__"))
			{
				continue;
			}
			it = replacementList.erase(it);
			it = replacementList.insert(it, tokens.begin(), tokens.end());
		}
	}

	replacementList.push_back({ E_TOKEN_TYPE::REJECT_MACRO, macroDesc.mName });
	result = replacementList;
	return true;
}

bool Preprocessor::_expandMacroDefinition(const TMacroDesc& macroDesc, const TToken& idToken, TokenStream& ts, std::vector<TToken>& result) const TCPP_NOEXCEPT
{
	if (macroDesc.custom) {
		mContextStack.push_back({ idToken.mRawView,
			idToken.start,
			idToken.end });
		macroDesc.custom(*this, idToken, result);
		result.push_back({
			E_TOKEN_TYPE::REJECT_MACRO
			});
		return true;
	}
	if (macroDesc.isFunc)
		return _expandFunctionMacro(macroDesc, idToken, ts, result);
	else
		return _expandSimpleMacro(macroDesc, idToken, ts, result);
}

void Preprocessor::_processInclusion(TokenStream& ts) TCPP_NOEXCEPT
{
	if (_shouldTokenBeSkipped())
	{
		return;
	}

	TToken currToken = ts.NextNonSpace();

	if (currToken.mType != E_TOKEN_TYPE::LESS && currToken.mType != E_TOKEN_TYPE::QUOTES)
	{
		mOnErrorCallback({ E_ERROR_TYPE::INVALID_INCLUDE_DIRECTIVE, 0 });
		return;
	}

	bool isSystemPathInclusion = currToken.mType == E_TOKEN_TYPE::LESS;

	std::string path;

	while (true)
	{
		if ((currToken = ts.Next()).mType == E_TOKEN_TYPE::QUOTES ||
			currToken.mType == E_TOKEN_TYPE::GREATER)
		{
			break;
		}

		if (currToken.mType == E_TOKEN_TYPE::NEWLINE)
		{
			mOnErrorCallback({ E_ERROR_TYPE::UNEXPECTED_END_OF_INCLUDE_PATH, 0 });
			break;
		}

		path.append(currToken.mRawView);
	}

	currToken = ts.NextNonSpace();

	if (E_TOKEN_TYPE::NEWLINE != currToken.mType && E_TOKEN_TYPE::END != currToken.mType)
	{
		mOnErrorCallback({ E_ERROR_TYPE::UNEXPECTED_TOKEN, 1 });
	}

	if (mOnIncludeCallback)
	{
		auto src = mOnIncludeCallback(path, isSystemPathInclusion);
		auto res = this->Process(src);
		//mpLexer->PushStream(std::move());
	}
}

Preprocessor::ConditionContext Preprocessor::_processIfConditional(TokenStream& ts) TCPP_NOEXCEPT
{
	TToken currToken;

	std::vector<TToken> expressionTokens;

	while ((currToken = ts.NextNonSpace()).mType != E_TOKEN_TYPE::END)
	{
		expressionTokens.push_back(currToken);
	}

	bool enabled = !!_evaluateExpression(expressionTokens);
	return ConditionContext(enabled);
}


Preprocessor::ConditionContext Preprocessor::_processIfdefConditional(TokenStream& ts) TCPP_NOEXCEPT
{
	auto currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::SPACE);

	currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::IDENTIFIER);

	std::string macroIdentifier = currToken.mRawView;

	currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::NEWLINE);

	bool enabled = mSymTable.Contains(macroIdentifier);
	return ConditionContext(enabled);
}

Preprocessor::ConditionContext Preprocessor::_processIfndefConditional(TokenStream& ts) TCPP_NOEXCEPT
{
	auto currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::SPACE);

	currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::IDENTIFIER);

	std::string macroIdentifier = currToken.mRawView;

	currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::NEWLINE);

	bool enabled = !mSymTable.Contains(macroIdentifier);
	return ConditionContext(enabled);
}

void Preprocessor::_processElseConditional(TokenStream& ts, ConditionContext& currStackEntry) TCPP_NOEXCEPT
{
	if (currStackEntry.haselse)
	{
		mOnErrorCallback({ E_ERROR_TYPE::ANOTHER_ELSE_BLOCK_FOUND, 0 });
		return;
	}

	currStackEntry.enabled = !currStackEntry.entered;
	currStackEntry.haselse = true;
}

void Preprocessor::_processElifConditional(TokenStream& ts, ConditionContext& currStackEntry) TCPP_NOEXCEPT
{
	if (currStackEntry.haselse)
	{
		mOnErrorCallback({ E_ERROR_TYPE::ELIF_BLOCK_AFTER_ELSE_FOUND, 0 });
		return;
	}

	auto currToken = ts.Next();
	_expect(currToken, E_TOKEN_TYPE::SPACE);

	std::vector<TToken> expressionTokens;

	while ((currToken = ts.Next()).mType != E_TOKEN_TYPE::END && (currToken = ts.Next()).mType != E_TOKEN_TYPE::NEWLINE)
	{
		expressionTokens.push_back(currToken);
	}

	_expect(currToken, E_TOKEN_TYPE::END, E_TOKEN_TYPE::NEWLINE);

	currStackEntry.enabled = !currStackEntry.entered && !!_evaluateExpression(expressionTokens);
	if (currStackEntry.enabled) currStackEntry.entered = true;
}

std::vector<TToken> fullyexpand(Preprocessor& pp, std::vector<TToken> tokens) {
	std::vector<TToken> result;
	for (auto it = tokens.begin(); it != tokens.end();) {
		if (it->mType == E_TOKEN_TYPE::REJECT_MACRO) {
			pp.mContextStack.pop_back();
			++it;
			continue;
		}

		if (it->mType != E_TOKEN_TYPE::IDENTIFIER) {
			result.push_back(*it);
			++it;
			continue;
		}

		auto m = pp.GetSymbolsTable().Get(it->mRawView);
		if (!m) {
			result.push_back(*it);
			++it;
			continue;
		}
		
		auto id = *it;

		ReplayTokenStream ts({ it + 1, tokens.end() });
		std::vector<TToken> r;
		pp._expandMacroDefinition(*m, *it, ts, r);
		it = tokens.erase(it, it + ts.pos + 1);
		it = tokens.insert(it, r.begin(), r.end());
	}
	return result;
}

int Preprocessor::_evaluateExpression(const std::vector<TToken>& exprTokens) const TCPP_NOEXCEPT
{
	//std::vector<TToken> tokens = fullyexpand(*const_cast<Preprocessor*>(this), exprTokens);
	std::vector<TToken> tokens{ exprTokens };
	// Removing all spaces as they are not needed
	auto spaces = std::ranges::remove(tokens, E_TOKEN_TYPE::SPACE, &TToken::mType);
	tokens.erase(spaces.begin(), spaces.end());


	tokens.push_back({ E_TOKEN_TYPE::END });

	auto evalPrimary = [this, &tokens]()
	{
		auto currToken = tokens.front();

		switch (currToken.mType)
		{
		case E_TOKEN_TYPE::IDENTIFIER:
		{
			// \note macro call
			TToken identifierToken;
			if (currToken.mRawView == "defined")
			{
				// defined ( X )
				tokens.erase(tokens.cbegin());
				_expect(tokens.front(), E_TOKEN_TYPE::OPEN_BRACKET);
				tokens.erase(tokens.cbegin());
				_expect(tokens.front(), E_TOKEN_TYPE::IDENTIFIER);

				identifierToken = tokens.front();

				tokens.erase(tokens.cbegin());

				_expect(tokens.front(), E_TOKEN_TYPE::CLOSE_BRACKET);

				// \note simple identifier
				return static_cast<int>(mSymTable.Contains(identifierToken.mRawView));
			}
			else
			{
				tokens.erase(tokens.cbegin());
				identifierToken = currToken;
			}

			/// \note Try to expand macro's value
			auto it = mSymTable.Get(identifierToken.mRawView);

			if (!it)
			{
				/// \note Lexer for now doesn't support numbers recognition so numbers are recognized as identifiers too
				return atoi(identifierToken.mRawView.c_str());
			}
			else
			{
				if (it->mArgsNames.empty())
				{
					return _evaluateExpression(it->mValue); /// simple macro replacement
				}

				/// \note Macro function call so we should firstly expand that one
				std::vector<TToken> expanded;
				ReplayTokenStream replay{ tokens };
				_expandMacroDefinition(*it, identifierToken, replay, expanded);
				auto r = std::ranges::remove(expanded, E_TOKEN_TYPE::REJECT_MACRO, &TToken::mType);
				expanded.erase(r.begin(), r.end());
				return _evaluateExpression(expanded);
			}

			return 0; /// \note Something went wrong so return 0
		}

		case E_TOKEN_TYPE::NUMBER:
			tokens.erase(tokens.cbegin());
			return std::stoi(currToken.mRawView);

		case E_TOKEN_TYPE::OPEN_BRACKET:
			tokens.erase(tokens.cbegin());
			return _evaluateExpression(tokens);

		default:
			break;
		}

		return 0;
	};

	auto evalUnary = [&tokens, &evalPrimary]()
	{
		while (E_TOKEN_TYPE::SPACE == tokens.front().mType) /// \note Skip whitespaces
		{
			tokens.erase(tokens.cbegin());
		}

		bool resultApply = false;
		TToken currToken;
		while ((currToken = tokens.front()).mType == E_TOKEN_TYPE::NOT || currToken.mType == E_TOKEN_TYPE::MINUS)
		{
			switch (currToken.mType)
			{
			case E_TOKEN_TYPE::MINUS:
				// TODO fix this
				break;
			case E_TOKEN_TYPE::NOT:
				tokens.erase(tokens.cbegin());
				resultApply = !resultApply;
				break;
			default:
				break;
			}
		}

		// even number of NOTs: false ^ false = false, false ^ true = true
		// odd number of NOTs: true ^ false = true (!false), true ^ true = false (!true)
		return static_cast<int>(resultApply) ^ evalPrimary();
	};

	auto evalMultiplication = [&tokens, &evalUnary]()
	{
		int result = evalUnary();
		int secondOperand = 0;

		TToken currToken;
		while ((currToken = tokens.front()).mType == E_TOKEN_TYPE::STAR || currToken.mType == E_TOKEN_TYPE::SLASH)
		{
			switch (currToken.mType)
			{
			case E_TOKEN_TYPE::STAR:
				tokens.erase(tokens.cbegin());
				result = result * evalUnary();
				break;
			case E_TOKEN_TYPE::SLASH:
				tokens.erase(tokens.cbegin());

				secondOperand = evalUnary();
				result = secondOperand ? (result / secondOperand) : 0 /* division by zero is considered as false in the implementation */;
				break;
			default:
				break;
			}
		}

		return result;
	};

	auto evalAddition = [&tokens, &evalMultiplication]()
	{
		int result = evalMultiplication();

		TToken currToken;
		while ((currToken = tokens.front()).mType == E_TOKEN_TYPE::PLUS || currToken.mType == E_TOKEN_TYPE::MINUS)
		{
			switch (currToken.mType)
			{
			case E_TOKEN_TYPE::PLUS:
				tokens.erase(tokens.cbegin());
				result = result + evalMultiplication();
				break;
			case E_TOKEN_TYPE::MINUS:
				tokens.erase(tokens.cbegin());
				result = result - evalMultiplication();
				break;
			default:
				break;
			}
		}

		return result;
	};

	auto evalComparison = [&tokens, &evalAddition]()
	{
		int result = evalAddition();

		TToken currToken;
		while ((currToken = tokens.front()).mType == E_TOKEN_TYPE::LESS ||
			currToken.mType == E_TOKEN_TYPE::GREATER ||
			currToken.mType == E_TOKEN_TYPE::LE ||
			currToken.mType == E_TOKEN_TYPE::GE)
		{
			switch (currToken.mType)
			{
			case E_TOKEN_TYPE::LESS:
				tokens.erase(tokens.cbegin());
				result = result < evalAddition();
				break;
			case E_TOKEN_TYPE::GREATER:
				tokens.erase(tokens.cbegin());
				result = result > evalAddition();
				break;
			case E_TOKEN_TYPE::LE:
				tokens.erase(tokens.cbegin());
				result = result <= evalAddition();
				break;
			case E_TOKEN_TYPE::GE:
				tokens.erase(tokens.cbegin());
				result = result >= evalAddition();
				break;
			default:
				break;
			}
		}

		return result;
	};

	auto evalEquality = [&tokens, &evalComparison]()
	{
		int result = evalComparison();

		TToken currToken;
		while ((currToken = tokens.front()).mType == E_TOKEN_TYPE::EQ || currToken.mType == E_TOKEN_TYPE::NE)
		{
			switch (currToken.mType)
			{
			case E_TOKEN_TYPE::EQ:
				tokens.erase(tokens.cbegin());
				result = result == evalComparison();
				break;
			case E_TOKEN_TYPE::NE:
				tokens.erase(tokens.cbegin());
				result = result != evalComparison();
				break;
			default:
				break;
			}
		}

		return result;
	};

	auto evalAndExpr = [&tokens, &evalEquality]()
	{
		int result = evalEquality();

		while (E_TOKEN_TYPE::SPACE == tokens.front().mType)
		{
			tokens.erase(tokens.cbegin());
		}

		while (tokens.front().mType == E_TOKEN_TYPE::AND)
		{
			tokens.erase(tokens.cbegin());
			result = result && evalEquality();
		}

		return result;
	};

	auto evalOrExpr = [&tokens, &evalAndExpr]()
	{
		int result = evalAndExpr();

		while (tokens.front().mType == E_TOKEN_TYPE::OR)
		{
			tokens.erase(tokens.cbegin());
			result = result || evalAndExpr();
		}

		return result;
	};

	return evalOrExpr();
}

bool Preprocessor::_shouldTokenBeSkipped() const TCPP_NOEXCEPT
{
	bool enabled = m_conditionStack.empty() || m_conditionStack.top().enabled && m_conditionStack.top().parentEnabled;
	return !enabled;
}

#endif
}