#include <stdafx.hpp>
#include <attrib.hpp>

#include <attributed_string_common.hpp>

using namespace StE::Text;
using namespace StE::Text::Attributes;

attributed_string_common<char> attrib::operator()(const std::string &str) const {
	attributed_string_common<char> newstr(str);
	newstr.add_attrib({ 0,newstr.length() }, *this);
	return newstr;
}

attributed_string_common<char16_t> attrib::operator()(const std::u16string &str) const {
	attributed_string_common<char16_t> newstr(str);
	newstr.add_attrib({ 0,newstr.length() }, *this);
	return newstr;
}

attributed_string_common<char32_t> attrib::operator()(const std::u32string &str) const {
	attributed_string_common<char32_t> newstr(str);
	newstr.add_attrib({ 0,newstr.length() }, *this);
	return newstr;
}

attributed_string_common<wchar_t> attrib::operator()(const std::wstring &str) const {
	attributed_string_common<wchar_t> newstr(str);
	newstr.add_attrib({ 0,newstr.length() }, *this);
	return newstr;
}

attributed_string_common<char> attrib::operator()(const char* str) const {
	return (*this)(std::string(str));
}

attributed_string_common<char16_t> attrib::operator()(const char16_t* str) const {
	return (*this)(std::u16string(str));
}

attributed_string_common<char32_t> attrib::operator()(const char32_t* str) const {
	return (*this)(std::u32string(str));
}

attributed_string_common<wchar_t> attrib::operator()(const wchar_t* str) const {
	return (*this)(std::wstring(str));
}
