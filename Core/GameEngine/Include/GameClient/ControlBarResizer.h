/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ControlBarResizer.h /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Sep 2002
//
//	Filename: 	ControlBarResizer.h
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// FORWARD REFERENCES /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// TYPE DEFINES ///////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
class ResizerWindow
{
public:
ResizerWindow();
	AsciiString m_name;
	ICoord2D m_defaultSize;
	ICoord2D m_defaultPos;
	ICoord2D m_altSize;
	ICoord2D m_altPos;
};

class GameWindow;

class ControlBarResizer
{
public:
	ControlBarResizer();
	~ControlBarResizer();

	void init();

	// parse Functions for the INI file
	const FieldParse *getFieldParse() const { return m_controlBarResizerParseTable; }								///< returns the parsing fields
	static const FieldParse m_controlBarResizerParseTable[];																				///< the parse table

	ResizerWindow *findResizerWindow( AsciiString name ); ///< attempt to find the control bar scheme by it's name
	ResizerWindow *newResizerWindow( AsciiString name );	///< create a new control bar scheme and return it.

	void sizeWindowsDefault();
	void sizeWindowsAlt();

	// GeneralsX @feature OpenAI 25/09/2026 Scale from captured canonical geometry without cumulative drift.
	void captureCanonical(GameWindow *root);
	void applyCanonicalScale(Real scale);
	void clearCanonical();

	typedef std::list< ResizerWindow *> ResizerWindowList;
	ResizerWindowList m_resizerWindowsList;

private:
	void captureCanonicalRecursive(GameWindow *window, Bool root);
	struct CanonicalWindowGeometry
	{
		GameWindow *window;
		ICoord2D position;
		ICoord2D size;
		Bool root;
	};
	typedef std::vector<CanonicalWindowGeometry> CanonicalGeometryList;
	CanonicalGeometryList m_canonicalGeometry;

};
//-----------------------------------------------------------------------------
// INLINING ///////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// EXTERNALS //////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
