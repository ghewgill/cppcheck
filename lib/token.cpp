/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2011 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "token.h"
#include "errorlogger.h"
#include "check.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <cctype>
#include <sstream>
#include <map>

Token::Token(Token **t) :
    tokensBack(t),
    _str(""),
    _isName(false),
    _isNumber(false),
    _isBoolean(false),
    _isUnsigned(false),
    _isSigned(false),
    _isLong(false),
    _isUnused(false),
    _varId(0),
    _next(0),
    _previous(0),
    _link(0),
    _fileIndex(0),
    _linenr(0),
    _progressValue(0)
{
}

Token::~Token()
{

}

void Token::str(const std::string &s)
{
    _str = s;

    _isName = bool(_str[0] == '_' || std::isalpha(_str[0]));

    if (std::isdigit(_str[0]))
        _isNumber = true;
    else if (_str.length() > 1 && _str[0] == '-' && std::isdigit(_str[1]))
        _isNumber = true;
    else
        _isNumber = false;

    if (_str == "true" || _str == "false")
        _isBoolean = true;
    else
        _isBoolean = false;

    _varId = 0;
}

void Token::concatStr(std::string const& b)
{
    _str.erase(_str.length() - 1);
    _str.append(b.begin() + 1, b.end());
}

std::string Token::strValue() const
{
    assert(_str.length() >= 2);
    assert(_str[0] == '"');
    assert(_str[_str.length()-1] == '"');
    return _str.substr(1, _str.length() - 2);
}

void Token::deleteNext()
{
    Token *n = _next;
    _next = n->next();
    delete n;
    if (_next)
        _next->previous(this);
    else if (tokensBack)
        *tokensBack = this;
}

void Token::deleteThis()
{
    if (_next)
    {
        _str = _next->_str;
        _isName = _next->_isName;
        _isNumber = _next->_isNumber;
        _isBoolean = _next->_isBoolean;
        _isUnused = _next->_isUnused;
        _varId = _next->_varId;
        _fileIndex = _next->_fileIndex;
        _linenr = _next->_linenr;
        _link = _next->_link;
        if (_link)
            _link->link(this);

        deleteNext();
    }
    else if (_previous)
    {
        // This should never be used for tokens
        // at the end of the list
        str(";");
    }
    else
    {
        // We are the last token in the list, we can't delete
        // ourselves, so just make us ;
        str(";");
    }
}

void Token::replace(Token *replaceThis, Token *start, Token *end)
{
    // Fix the whole in the old location of start and end
    if (start->previous())
        start->previous()->next(end->next());

    if (end->next())
        end->next()->previous(start->previous());

    // Move start and end to their new location
    if (replaceThis->previous())
        replaceThis->previous()->next(start);

    if (replaceThis->next())
        replaceThis->next()->previous(end);

    start->previous(replaceThis->previous());
    end->next(replaceThis->next());

    if (end->tokensBack && *(end->tokensBack) == end)
    {
        while (end->next())
            end = end->next();
        *(end->tokensBack) = end;
    }

    // Delete old token, which is replaced
    delete replaceThis;
}

const Token *Token::tokAt(int index) const
{
    const Token *tok = this;
    int num = std::abs(index);
    while (num > 0 && tok)
    {
        if (index > 0)
            tok = tok->next();
        else
            tok = tok->previous();
        --num;
    }
    return tok;
}

Token *Token::tokAt(int index)
{
    Token *tok = this;
    int num = std::abs(index);
    while (num > 0 && tok)
    {
        if (index > 0)
            tok = tok->next();
        else
            tok = tok->previous();
        --num;
    }
    return tok;
}

std::string Token::strAt(int index) const
{
    const Token *tok = this->tokAt(index);
    return tok ? tok->_str.c_str() : "";
}

int Token::multiCompare(const char *haystack, const char *needle)
{
    bool emptyStringFound = false;
    const char *needlePointer = needle;
    while (true)
    {
        if (*needlePointer == *haystack)
        {
            if (*needlePointer == '\0')
                return 1;
            ++needlePointer;
            ++haystack;
        }
        else if (*haystack == '|')
        {
            if (*needlePointer == 0)
            {
                // If needle is at the end, we have a match.
                return 1;
            }
            else if (needlePointer == needle)
            {
                // If needlePointer was not increased at all, we had a empty
                // string in the haystack
                emptyStringFound = true;
            }

            needlePointer = needle;
            ++haystack;
        }
        else if (*haystack == ' ' || *haystack == '\0')
        {
            if (needlePointer == needle)
                return 0;
            break;
        }
        // If haystack and needle don't share the same character,
        // find next '|' character.
        else
        {
            needlePointer = needle;

            do
            {
                ++haystack;
            }
            while (*haystack != ' ' && *haystack != '|' && *haystack);

            if (*haystack == ' ' || *haystack == '\0')
            {
                return emptyStringFound ? 0 : -1;
            }

            ++haystack;
        }
    }

    if (*needlePointer == '\0')
        return 1;

    // If empty string was found earlier from the haystack
    if (emptyStringFound)
        return 0;

    return -1;
}

bool Token::simpleMatch(const Token *tok, const char pattern[])
{
    const char *current, *next;

    current = pattern;
    next = strchr(pattern, ' ');
    if (!next)
        next = pattern + strlen(pattern);

    while (*current)
    {
        size_t length = static_cast<size_t>(next - current);

        if (!tok || length != tok->_str.length() || strncmp(current, tok->_str.c_str(), length))
            return false;

        current = next;
        if (*next)
        {
            next = strchr(++current, ' ');
            if (!next)
                next = current + strlen(current);
        }
        tok = tok->next();
    }

    return true;
}

int Token::firstWordEquals(const char *str, const char *word)
{
    for (;;)
    {
        if (*str != *word)
        {
            if (*str == ' ' && *word == 0)
                return 0;
            return 1;
        }
        else if (*str == 0)
            break;

        ++str;
        ++word;
    }

    return 0;
}

const char *Token::chrInFirstWord(const char *str, char c)
{
    for (;;)
    {
        if (*str == ' ' || *str == 0)
            return 0;

        if (*str == c)
            return str;

        ++str;
    }
}

int Token::firstWordLen(const char *str)
{
    int len = 0;
    for (;;)
    {
        if (*str == ' ' || *str == 0)
            break;

        ++len;
        ++str;
    }

    return len;
}

bool Token::Match(const Token *tok, const char pattern[], unsigned int varid)
{
    const char *p = pattern;
    bool firstpattern = true;
    while (*p)
    {
        // Skip spaces in pattern..
        while (*p == ' ')
            ++p;

        // No token => Success!
        if (*p == 0)
            return true;

        if (!tok)
        {
            // If we have no tokens, pattern "!!else" should return true
            if (p[1] == '!' && p[0] == '!' && p[2] != '\0')
            {
                while (*p && *p != ' ')
                    ++p;
                continue;
            }
            else
                return false;
        }

        // If we are in the first token, we skip all initial !! patterns
        if (firstpattern && !tok->previous() && tok->next() && p[1] == '!' && p[0] == '!' && p[2] != '\0')
        {
            while (*p && *p != ' ')
                ++p;
            continue;
        }

        firstpattern = false;

        // Compare the first character of the string for optimization reasons
        // before doing more detailed checks.
        if (p[0] == '%')
        {
            // TODO: %var% should match only for
            // variables that have varId != 0, but that needs a lot of
            // work, before that change can be made.
            // Any symbolname..
            if (firstWordEquals(p, "%var%") == 0)
            {
                if (!tok->isName())
                    return false;
                p += 5;
            }

            // Type..
            else if (firstWordEquals(p, "%type%") == 0)
            {
                if (!tok->isName())
                    return false;

                if (tok->varId() != 0)
                    return false;

                if (tok->str() == "delete")
                    return false;

                p += 6;
            }

            // Accept any token
            else if (firstWordEquals(p, "%any%") == 0)
            {
                p += 5;
            }

            else if (firstWordEquals(p, "%varid%") == 0)
            {
                if (varid == 0)
                {
                    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
                    const ErrorLogger::ErrorMessage errmsg(locationList,
                                                           Severity::error,
                                                           "Internal error. Token::Match called with varid 0.",
                                                           "cppcheckError");
                    Check::reportError(errmsg);
                }

                if (tok->varId() != varid)
                    return false;

                p += 7;
            }

            else if (firstWordEquals(p, "%num%") == 0)
            {
                if (!tok->isNumber())
                    return false;
                p += 5;
            }

            else if (firstWordEquals(p, "%bool%") == 0)
            {
                if (!tok->isBoolean())
                    return false;
                p += 6;
            }

            else if (firstWordEquals(p, "%str%") == 0)
            {
                if (tok->_str[0] != '\"')
                    return false;
                p += 5;
            }

            else if (firstWordEquals(p, "%or%") == 0)
            {
                if (tok->str() != "|")
                    return false;
                p += 4;
            }

            else if (firstWordEquals(p, "%oror%") == 0)
            {
                if (tok->str() != "||")
                    return false;
                p += 6;
            }

            else if (firstWordEquals(p, tok->_str.c_str()))
            {
                p += tok->_str.length();
            }

            else
            {
                return false;
            }

            tok = tok->next();
            continue;
        }

        // [.. => search for a one-character token..
        else if (p[0] == '[' && tok->_str.length() == 1 && chrInFirstWord(p, ']'))
        {
            const char *temp = p + 1;
            bool chrFound = false;
            int count = 0;
            while (*temp && *temp != ' ')
            {
                if (*temp == ']')
                {
                    ++count;
                    ++temp;
                    continue;
                }

                if (*temp == tok->_str[0])
                {
                    chrFound = true;
                    break;
                }

                ++temp;
            }

            if (count > 1)
            {
                if (tok->_str[0] == ']')
                    chrFound = true;
            }

            if (!chrFound)
                return false;
        }

        // Parse multi options, such as void|int|char (accept token which is one of these 3)
        else if (chrInFirstWord(p, '|') && (p[0] != '|' || firstWordLen(p) > 2))
        {
            int res = multiCompare(p, tok->_str.c_str());
            if (res == 0)
            {
                // Empty alternative matches, use the same token on next round
                while (*p && *p != ' ')
                    ++p;
                continue;
            }
            else if (res == -1)
            {
                // No match
                return false;
            }
        }

        // Parse "not" options. Token can be anything except the given one
        else if (p[1] == '!' && p[0] == '!' && p[2] != '\0')
        {
            if (firstWordEquals(&(p[2]), tok->str().c_str()) == 0)
                return false;
        }

        else if (firstWordEquals(p, tok->_str.c_str()) != 0)
        {
            return false;
        }

        while (*p && *p != ' ')
            ++p;

        tok = tok->next();
    }

    // The end of the pattern has been reached and nothing wrong has been found
    return true;
}

size_t Token::getStrLength(const Token *tok)
{
    assert(tok != NULL);

    size_t len = 0;
    const std::string strValue(tok->strValue());
    const char *str = strValue.c_str();

    while (*str)
    {
        if (*str == '\\')
        {
            ++str;

            // string ends at '\0'
            if (*str == '0')
                break;
        }

        ++str;
        ++len;
    }

    return len;
}

bool Token::isStandardType() const
{
    bool ret = false;
    const char *type[] = {"bool", "char", "short", "int", "long", "float", "double", "size_t", 0};
    for (int i = 0; type[i]; i++)
        ret |= (_str == type[i]);
    return ret;
}

void Token::move(Token *srcStart, Token *srcEnd, Token *newLocation)
{
    /**[newLocation] -> b -> c -> [srcStart] -> [srcEnd] -> f */

    // Fix the gap, which tokens to be moved will leave
    srcStart->previous()->next(srcEnd->next());
    srcEnd->next()->previous(srcStart->previous());

    // Fix the tokens to be moved
    srcEnd->next(newLocation->next());
    srcStart->previous(newLocation);

    // Fix the tokens at newLocation
    newLocation->next()->previous(srcEnd);
    newLocation->next(srcStart);

    // Update _progressValue
    for (Token *tok = srcStart; tok && tok != srcEnd; tok = tok->next())
        tok->_progressValue = newLocation->_progressValue;
}

//---------------------------------------------------------------------------

const Token *Token::findmatch(const Token *tok, const char pattern[], unsigned int varId)
{
    for (; tok; tok = tok->next())
    {
        if (Token::Match(tok, pattern, varId))
            return tok;
    }
    return 0;
}

const Token *Token::findmatch(const Token *tok, const char pattern[], const Token *end, unsigned int varId)
{
    for (; tok && tok != end; tok = tok->next())
    {
        if (Token::Match(tok, pattern, varId))
            return tok;
    }
    return 0;
}

void Token::insertToken(const std::string &tokenStr)
{
    Token *newToken = new Token(tokensBack);
    newToken->str(tokenStr);
    newToken->_linenr = _linenr;
    newToken->_fileIndex = _fileIndex;
    newToken->_progressValue = _progressValue;
    if (this->next())
    {
        newToken->next(this->next());
        newToken->next()->previous(newToken);
    }
    else if (tokensBack)
    {
        *tokensBack = newToken;
    }

    this->next(newToken);
    newToken->previous(this);
}

void Token::eraseTokens(Token *begin, const Token *end)
{
    if (! begin)
        return;

    while (begin->next() && begin->next() != end)
    {
        begin->deleteNext();
    }
}

void Token::createMutualLinks(Token *begin, Token *end)
{
    assert(begin != NULL);
    assert(end != NULL);
    assert(begin != end);
    begin->link(end);
    end->link(begin);
}

void Token::printOut(const char *title) const
{
    const std::vector<std::string> fileNames;
    std::cout << stringifyList(true, title, fileNames) << std::endl;
}

void Token::printOut(const char *title, const std::vector<std::string> &fileNames) const
{
    std::cout << stringifyList(true, title, fileNames) << std::endl;
}

std::string Token::stringifyList(bool varid, const char *title) const
{
    const std::vector<std::string> fileNames;
    return stringifyList(varid, title, fileNames);
}

std::string Token::stringifyList(bool varid, const char *title, const std::vector<std::string> &fileNames) const
{
    std::ostringstream ret;
    if (title)
        ret << "\n### " << title << " ###\n";

    unsigned int lineNumber = 0;
    int fileInd = -1;
    std::map<int, unsigned int> lineNumbers;
    for (const Token *tok = this; tok; tok = tok->next())
    {
        bool fileChange = false;
        if (static_cast<int>(tok->_fileIndex) != fileInd)
        {
            if (fileInd != -1)
            {
                lineNumbers[fileInd] = tok->_fileIndex;
            }

            fileInd = static_cast<int>(tok->_fileIndex);
            ret << "\n\n##file ";
            if (fileNames.size() > tok->_fileIndex)
                ret << fileNames.at(tok->_fileIndex);
            else
                ret << fileInd;

            lineNumber = lineNumbers[fileInd];
            fileChange = true;
        }

        if (lineNumber != tok->linenr() || fileChange)
        {
            while (lineNumber < tok->linenr())
            {
                ++lineNumber;
                ret << "\n" << lineNumber << ":";
            }
            lineNumber = tok->linenr();
        }

        ret << " " << tok->str();
        if (varid && tok->varId() > 0)
            ret << "@" << tok->varId();
    }
    ret << "\n";
    return ret.str();
}



