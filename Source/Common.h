/*
** Dn-FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2020-2025 D.P.C.M.
** FamiTracker Copyright (C) 2005-2020 Jonathan Liss
** 0CC-FamiTracker Copyright (C) 2014-2018 HertzDevil
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program. If not, see https://www.gnu.org/licenses/.
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if ((p) != nullptr) { delete (p); (p) = nullptr; } } while (0)
#endif

#ifndef SAFE_RELEASE_ARRAY
#define SAFE_RELEASE_ARRAY(p) do { if ((p) != nullptr) { delete [] (p); (p) = nullptr; } } while (0)
#endif

#ifndef AfxDebugBreak
#define AfxDebugBreak() assert(false)
#endif

#ifndef ASSERT
#define ASSERT(expr) assert(expr)
#endif

#ifndef VERIFY
#define VERIFY(expr) assert(expr)
#endif

#define _MAIN_H_

#define SAMPLES_IN_BYTES(x) (x << SampleSizeShift)

enum decay_rate_t {		// // // 050B
	DECAY_SLOW,
	DECAY_FAST
};

enum module_error_level_t {
	MODULE_ERROR_NONE,		/*!< No error checking at all (warning) */
	MODULE_ERROR_DEFAULT,	/*!< Usual error checking */
	MODULE_ERROR_STRICT,	/*!< Extra validation for some values */
	MODULE_ERROR_MAX = MODULE_ERROR_STRICT,
};

// Used to get the DPCM state
struct stDPCMState {
	int SamplePos;
	int DeltaCntr;
};

// Used to play the audio when the buffer is full
class IAudioCallback {
public:
	virtual void FlushBuffer(int16_t const * Buffer, uint32_t Size) = 0;
};

#ifndef _AFX

#ifndef UINT
typedef unsigned int UINT;
#endif

#ifndef BYTE
typedef uint8_t BYTE;
#endif

#ifndef WORD
typedef uint16_t WORD;
#endif

#ifndef DWORD
typedef uint32_t DWORD;
#endif

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef LONGLONG
typedef int64_t LONGLONG;
#endif

#ifndef ULONGLONG
typedef uint64_t ULONGLONG;
#endif

#ifndef LPCTSTR
typedef const char* LPCTSTR;
#endif

#ifndef LPCSTR
typedef const char* LPCSTR;
#endif

#ifndef LPTSTR
typedef char* LPTSTR;
#endif

#ifndef LPSTR
typedef char* LPSTR;
#endif

#ifndef TCHAR
typedef char TCHAR;
#endif

#ifndef _T
#define _T(x) x
#endif

#ifndef _TEXT
#define _TEXT(x) x
#endif

#ifndef __cdecl
#define __cdecl
#endif

#ifndef CALL_MEMBER_FN
#define CALL_MEMBER_FN(obj, ptr) ((obj)->*(ptr))
#endif

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif

#ifndef sprintf_s
#define sprintf_s(buf, sz, ...) snprintf(buf, sz, __VA_ARGS__)
#endif

#ifndef _sntprintf_s
#define _sntprintf_s(buf, sz, truncate, ...) snprintf(buf, sz, __VA_ARGS__)
#endif

#ifndef strcpy_s
inline int strcpy_s(char* dest, size_t destsz, const char* src) {
	if (!dest || !src || destsz == 0) return -1;
	size_t len = strlen(src);
	if (len >= destsz) { dest[0] = '\0'; return -1; }
	memcpy(dest, src, len + 1);
	return 0;
}
template <size_t N>
inline int strcpy_s(char (&dest)[N], const char* src) {
	return strcpy_s(dest, N, src);
}
#endif

#ifndef strncpy_s
inline int strncpy_s(char* dest, size_t destsz, const char* src, size_t count) {
	if (!dest || !src || destsz == 0) return -1;
	size_t copy_len = (count < destsz - 1) ? count : (destsz - 1);
	size_t src_len = strlen(src);
	if (copy_len > src_len) copy_len = src_len;
	memcpy(dest, src, copy_len);
	dest[copy_len] = '\0';
	return 0;
}
template <size_t N>
inline int strncpy_s(char (&dest)[N], const char* src, size_t count) {
	return strncpy_s(dest, N, src, count);
}
#endif

class CString : public std::string {
public:
	using std::string::string;
	CString() = default;
	CString(const std::string& s) : std::string(s) {}
	CString(std::string&& s) : std::string(std::move(s)) {}
	CString(const char* s) : std::string(s ? s : "") {}
	CString(char c) : std::string(1, c) {}

	int GetLength() const { return static_cast<int>(length()); }
	bool IsEmpty() const { return empty(); }
	void Empty() { clear(); }

	void AppendChar(char ch) { push_back(ch); }
	void Append(const char* s) { if (s) append(s); }
	void Append(const std::string& s) { append(s); }

	operator const char*() const { return c_str(); }

	int Compare(const char* s) const { return compare(s ? s : ""); }
	int CompareNoCase(const char* s) const {
		if (!s) return 1;
		const char* p1 = c_str();
		const char* p2 = s;
		while (*p1 && *p2) {
			int d = tolower((unsigned char)*p1) - tolower((unsigned char)*p2);
			if (d != 0) return d;
			p1++; p2++;
		}
		return tolower((unsigned char)*p1) - tolower((unsigned char)*p2);
	}

	CString Left(int nCount) const {
		if (nCount <= 0) return CString();
		if (nCount >= (int)length()) return *this;
		return substr(0, nCount);
	}

	CString Right(int nCount) const {
		if (nCount <= 0) return CString();
		if (nCount >= (int)length()) return *this;
		return substr(length() - nCount);
	}

	CString Mid(int nFirst, int nCount = -1) const {
		if (nFirst < 0) nFirst = 0;
		if (nFirst >= (int)length()) return CString();
		if (nCount < 0 || nFirst + nCount > (int)length()) return substr(nFirst);
		return substr(nFirst, nCount);
	}

	int Find(char ch, int nStart = 0) const {
		auto pos = find(ch, nStart);
		return pos == std::string::npos ? -1 : static_cast<int>(pos);
	}

	int Find(const char* s, int nStart = 0) const {
		auto pos = find(s, nStart);
		return pos == std::string::npos ? -1 : static_cast<int>(pos);
	}

	int ReverseFind(char ch) const {
		auto pos = rfind(ch);
		return pos == std::string::npos ? -1 : static_cast<int>(pos);
	}

	char GetAt(int nIndex) const { return c_str()[nIndex]; }
	void SetAt(int nIndex, char ch) { data()[nIndex] = ch; }

	void Format(const char* pszFormat, ...) {
		va_list args;
		va_start(args, pszFormat);
		va_list args_copy;
		va_copy(args_copy, args);
		int len = vsnprintf(nullptr, 0, pszFormat, args_copy);
		va_end(args_copy);
		if (len > 0) {
			resize(len);
			vsnprintf(data(), len + 1, pszFormat, args);
		} else {
			clear();
		}
		va_end(args);
	}

	void AppendFormat(const char* pszFormat, ...) {
		va_list args;
		va_start(args, pszFormat);
		va_list args_copy;
		va_copy(args_copy, args);
		int len = vsnprintf(nullptr, 0, pszFormat, args_copy);
		va_end(args_copy);
		if (len > 0) {
			size_t oldSize = size();
			resize(oldSize + len);
			vsnprintf(data() + oldSize, len + 1, pszFormat, args);
		}
		va_end(args);
	}

	char* GetBuffer(int nMinBufLength = 0) {
		if (nMinBufLength > (int)size()) {
			resize(nMinBufLength);
		}
		return empty() ? const_cast<char*>("") : data();
	}

	void ReleaseBuffer(int nNewLength = -1) {
		if (nNewLength >= 0) {
			resize(nNewLength);
		} else {
			resize(strlen(c_str()));
		}
	}
};

using CStringA = CString;

class CT2A {
public:
	CT2A(const char* s) : m_s(s ? s : "") {}
	CT2A(const CString& s) : m_s(s) {}
	operator const char*() const { return m_s.c_str(); }
	operator char*() { return &m_s[0]; }
private:
	std::string m_s;
};
using CA2T = CT2A;

class CStringArray : public std::vector<CString> {
public:
	int GetSize() const { return static_cast<int>(size()); }
	int GetCount() const { return static_cast<int>(size()); }
	int Add(const CString& str) { push_back(str); return static_cast<int>(size() - 1); }
	void RemoveAll() { clear(); }
	CString GetAt(int idx) const { return (*this)[idx]; }
	void SetAt(int idx, const CString& str) { (*this)[idx] = str; }
};

namespace std {
	template <>
	struct hash<CString> {
		size_t operator()(const CString& s) const noexcept {
			return std::hash<std::string>{}(s);
		}
	};
}

#include <map>

template <typename Key, typename ArgKey, typename Val, typename ArgVal>
class CMap : public std::map<Key, Val> {
public:
	BOOL Lookup(ArgKey key, Val& rValue) const {
		auto it = this->find(key);
		if (it != this->end()) {
			rValue = it->second;
			return TRUE;
		}
		return FALSE;
	}
	void SetAt(ArgKey key, ArgVal newValue) {
		(*this)[key] = newValue;
	}
};

class CCriticalSection {
public:
	void Lock() { m_mutex.lock(); }
	void Unlock() { m_mutex.unlock(); }
private:
	std::recursive_mutex m_mutex;
};

class CMutex {
public:
	void Lock() { m_mutex.lock(); }
	void Unlock() { m_mutex.unlock(); }
private:
	std::recursive_mutex m_mutex;
};

class CFileException {
public:
	int m_cause = 0;
	void Delete() {}
	BOOL GetErrorMessage(char* szBuf, UINT nMaxError) {
		if (szBuf && nMaxError > 0) {
			strncpy_s(szBuf, nMaxError, "File error", nMaxError - 1);
		}
		return TRUE;
	}
};

class CFile {
public:
	enum OpenFlags {
		modeRead = 0x0000,
		modeWrite = 0x0001,
		modeReadWrite = 0x0002,
		modeCreate = 0x1000,
		typeBinary = 0x4000,
		typeText = 0x0000,
		shareDenyWrite = 0x0200,
		shareDenyNone = 0x0000
	};
	enum SeekPosition {
		begin = 0x0,
		current = 0x1,
		end = 0x2
	};

	CFile() : m_pFile(nullptr), m_memData(nullptr), m_memSize(0), m_memPos(0), m_isMem(false) {}
	CFile(const char* lpszFileName, UINT nOpenFlags) : CFile() {
		Open(lpszFileName, nOpenFlags);
	}
	virtual ~CFile() {
		Close();
	}

	virtual BOOL Open(const char* lpszFileName, UINT nOpenFlags, CFileException* pError = nullptr) {
		(void)pError;
		Close();
		m_isMem = false;
		const char* mode = "rb";
		if ((nOpenFlags & modeCreate) && (nOpenFlags & modeWrite)) {
			mode = "wb+";
		} else if (nOpenFlags & modeWrite) {
			mode = "wb";
		}
		m_pFile = fopen(lpszFileName, mode);
		return m_pFile != nullptr;
	}

	BOOL OpenMemory(const void* data, size_t size) {
		Close();
		m_isMem = true;
		m_memData = static_cast<const uint8_t*>(data);
		m_memSize = size;
		m_memPos = 0;
		return TRUE;
	}

	virtual void Close() {
		if (m_pFile) {
			fclose(m_pFile);
			m_pFile = nullptr;
		}
		m_isMem = false;
		m_memData = nullptr;
		m_memSize = 0;
		m_memPos = 0;
	}

	virtual UINT Read(void* lpBuf, UINT nCount) {
		if (m_isMem) {
			if (m_memPos >= m_memSize) return 0;
			size_t bytesToRead = (nCount < m_memSize - m_memPos) ? nCount : (m_memSize - m_memPos);
			memcpy(lpBuf, m_memData + m_memPos, bytesToRead);
			m_memPos += bytesToRead;
			return static_cast<UINT>(bytesToRead);
		}
		if (!m_pFile) return 0;
		return static_cast<UINT>(fread(lpBuf, 1, nCount, m_pFile));
	}

	virtual void Write(const void* lpBuf, UINT nCount) {
		if (m_isMem) return;
		if (m_pFile) {
			fwrite(lpBuf, 1, nCount, m_pFile);
		}
	}

	virtual ULONGLONG Seek(LONGLONG lOff, UINT nFrom) {
		if (m_isMem) {
			LONGLONG newPos = m_memPos;
			switch (nFrom) {
			case begin: newPos = lOff; break;
			case current: newPos += lOff; break;
			case end: newPos = (LONGLONG)m_memSize + lOff; break;
			}
			if (newPos < 0) newPos = 0;
			if (newPos > (LONGLONG)m_memSize) newPos = (LONGLONG)m_memSize;
			m_memPos = static_cast<size_t>(newPos);
			return m_memPos;
		}
		if (!m_pFile) return 0;
		int origin = SEEK_SET;
		if (nFrom == current) origin = SEEK_CUR;
		else if (nFrom == end) origin = SEEK_END;
		fseek(m_pFile, (long)lOff, origin);
		return (ULONGLONG)ftell(m_pFile);
	}

	virtual ULONGLONG GetPosition() const {
		if (m_isMem) return m_memPos;
		if (!m_pFile) return 0;
		return (ULONGLONG)ftell(m_pFile);
	}

	virtual ULONGLONG GetLength() const {
		if (m_isMem) return m_memSize;
		if (!m_pFile) return 0;
		long current = ftell(m_pFile);
		fseek(m_pFile, 0, SEEK_END);
		long len = ftell(m_pFile);
		fseek(m_pFile, current, SEEK_SET);
		return (ULONGLONG)len;
	}

protected:
	FILE* m_pFile;
	const uint8_t* m_memData;
	size_t m_memSize;
	size_t m_memPos;
	bool m_isMem;
};

#endif // !_AFX
