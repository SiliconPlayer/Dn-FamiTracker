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

#include <string>
#include <vector>
#include <exception>
#include <memory>
#include <cstdio>

#include "Common.h"

#if defined(_AFX) && defined(BUILD_GUI)
#include "FamiTracker.h"
#include "Settings.h"
inline int GetCurrentModuleErrorLevel() {
	return (theApp.GetSettings()) ? theApp.GetSettings()->Version.iErrorLevel : MODULE_ERROR_DEFAULT;
}
#else
inline int GetCurrentModuleErrorLevel() {
	return MODULE_ERROR_DEFAULT;
}
#endif

/*!
	\brief An exception object raised while reading and writing FTM files.
*/
class CModuleException : public std::exception
{
public:
	/*!	\brief Constructor of the exception object with an empty message. */
	CModuleException();
	/*! \brief Virtual destructor. */
	virtual ~CModuleException() { }

	/*!	\brief Raises the exception object.
		\details All derived classes must override this method with the exact same function body in order
		to throw polymorphically. */
	[[noreturn]] virtual void Raise() { throw this; };

	/*!	\brief Obtains the error description.
		\details The description consists of zero or more lines followed by the footer specified in the
		constructor. This exception object does not use std::exception::what.
		\return The error string. */
	const std::string GetErrorString() const;
	/*!	\brief Appends a formatted error string to the exception.
		\param fmt The format specifier.
		\param ... Extra arguments for the formatted string. */
	template <typename... T>
	void AppendError(std::string fmt, T... args)
	{
		const size_t MAX_ERROR_STRLEN = 256;
		char buf[MAX_ERROR_STRLEN] = { };
		snprintf(buf, MAX_ERROR_STRLEN, fmt.c_str(), args...);
		m_strError.emplace_back(new std::string(buf));
	}
	/*!	\brief Sets the footer string of the error message.
		\param footer The new footer string. */
	void SetFooter(std::string footer);

public:
	/*!	\brief Validates a numerical value so that it lies within the interval [Min, Max].
		\details This method may throw a CModuleException object and automatically supply a suitable
		error message based on the value description. This method handles signed and unsigned types
		properly. Errors may be ignored if the current module error level is low enough.
		\param Value The value to check against.
		\param Min The minimum value permitted, inclusive.
		\param Max The maximum value permitted, inclusive.
		\param Desc A description of the checked value.
		\param fmt Print format specifier for the value type.
		\return The value argument, if the method returns.
	*/
	template <module_error_level_t l = MODULE_ERROR_DEFAULT, typename T, typename U, typename V>
	static T AssertRangeFmt(T Value, U Min, V Max, std::string Desc, const char *fmt)
	{
		if (l > GetCurrentModuleErrorLevel())
			return Value;
		if (!(Value >= Min && Value <= Max)) {
			char Format[128];
			snprintf(Format, sizeof(Format), "%%s out of range: expected [%s,%s], got %s", fmt, fmt, fmt);
			char Buffer[512];
			snprintf(Buffer, sizeof(Buffer), Format, Desc.c_str(), Min, Max, Value);
			CModuleException *e = new CModuleException();
			e->AppendError(std::string(Buffer));
			e->Raise();
		}
		return Value;
	}

private:
	std::vector<std::unique_ptr<std::string>> m_strError;
	std::unique_ptr<std::string> m_strFooter;
};