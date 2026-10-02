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

#include <vector>
#include <string>
#include <memory>
#include <array>
#include <unordered_map>
#include <type_traits>

#include "Common.h"
#include "FamiTrackerTypes.h"
#include "PatternData.h"
#include "FTMComponentInterface.h"
#include "InstrumentManager.h"
#include "OldSequence.h"
#include "Groove.h"
#include "DocumentFile.h"
#include "ModuleException.h"
#include "json/json.hpp"

using json = nlohmann::json;

static const unsigned int OLD_SPEED_SPLIT_POINT = 21;

class CFTMDocument : public CFTMComponentInterface
{
public:
	CFTMDocument();
	virtual ~CFTMDocument();

	// Component interface implementation
	CSequenceManager *const GetSequenceManager(int InstType) const override;
	CInstrumentManager *const GetInstrumentManager() const override;
	CDSampleManager *const GetDSampleManager() const override;
	CBookmarkManager *const GetBookmarkManager() const override { return nullptr; }
	void Modify(bool) override {}
	void ModifyIrreversible() override {}

	// Document loading
	bool LoadDocument(const char* lpszPathName);
	bool LoadDocument(const void* pData, size_t nSize);
	bool OpenDocumentNew(CDocumentFile &DocumentFile);
	void DeleteContents();
	void CreateEmpty();

	// Track operations
	void AllocateTrack(unsigned int Song);
	CPatternData* GetTrack(unsigned int Track) const;
	unsigned int GetTrackCount() const { return m_iTrackCount; }
	unsigned int GetChannelCount() const { return m_iChannelsAvailable; }
	unsigned char GetExpansionChip() const { return m_iExpansionChip; }
	bool ExpansionEnabled(int Chip) const { return (m_iExpansionChip & Chip) == Chip; }
	machine_t GetMachine() const { return m_iMachine; }
	unsigned int GetEngineSpeed() const { return m_iEngineSpeed; }
	int GetFrameRate() const { return m_iEngineSpeed ? m_iEngineSpeed : (m_iMachine == NTSC ? 60 : 50); }
	unsigned int GetSpeedSplitPoint() const { return m_iSpeedSplitPoint; }
	vibrato_t GetVibratoStyle() const { return m_iVibratoStyle; }
	bool GetLinearPitch() const { return m_bLinearPitch; }
	unsigned int GetNamcoChannels() const { return m_iNamcoChannels; }
	unsigned int GetFileVersion() const { return m_iFileVersion; }
	bool IsDnModule() const { return m_bFileDnModule; }
	const CString& GetLastError() const { return m_strLastError; }

	// Song metadata
	const char* GetSongName() const { return m_strName; }
	const char* GetSongArtist() const { return m_strArtist; }
	const char* GetSongCopyright() const { return m_strCopyright; }
	const CString& GetSongComment() const { return m_strComment; }

	// Per-track helpers
	int GetPatternLength(int Track) const;
	int GetFrameCount(int Track) const;
	int GetSongSpeed(int Track) const;
	int GetSongTempo(int Track) const;
	bool GetSongGroove(int Track) const;
	int GetEffColumns(int Track, int Channel) const;
	void GetNoteData(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Row, stChanNote *Data) const;

	// Groove & tuning
	const CGroove* GetGroove(int Index) const;
	void SetGroove(int Index, const CGroove* Groove);
	int GetTuningSemitone() const { return m_iDetuneSemitone; }
	int GetTuningCent() const { return m_iDetuneCent; }
	int GetDetuneOffset(int Chip, int Note) const;

	// Level offset & survey mixing
	int16_t GetLevelOffset(int Device) const { return (Device >= 0 && Device < 8) ? m_iDeviceLevelOffset[Device] : 0; }
	void SetLevelOffset(int Device, int16_t Offset) { if (Device >= 0 && Device < 8) m_iDeviceLevelOffset[Device] = Offset; }
	bool GetSurveyMixCheck() const { return m_bUseSurveyMixing; }
	void SetSurveyMixCheck(bool Check) { m_bUseSurveyMixing = Check; }

	// OPLL patches
	uint8_t GetOPLLPatchByte(int index) const { return (index >= 0 && index < 19 * 8) ? m_iOPLLPatchBytes[index] : 0; }
	std::string GetOPLLPatchName(int index) const { return (index >= 0 && index < 19) ? m_strOPLLPatchNames[index] : ""; }

	// Samples
	void SetSample(unsigned int Index, CDSample *pSamp);

	// Song length / duration calculation (in seconds)
	double GetStandardLength(int Track, unsigned int ExtraLoops = 1) const;

	// Validation helpers
	template <module_error_level_t l = MODULE_ERROR_DEFAULT>
	void AssertFileData(bool Cond, std::string Msg) const
	{
		if (l <= MODULE_ERROR_DEFAULT && !Cond) {
			CModuleException *e = m_pCurrentDocument ? m_pCurrentDocument->GetException() : new CModuleException();
			e->AppendError(Msg);
			e->Raise();
		}
	}

	template <module_error_level_t l = MODULE_ERROR_DEFAULT, typename T, typename U, typename V>
	typename std::enable_if<std::is_unsigned<T>::value, T>::type
	AssertRange(T Value, U Min, V Max, std::string Desc) const
	{
		try {
			return CModuleException::AssertRangeFmt<l>(Value, Min, Max, Desc, "%u");
		}
		catch (CModuleException *e) {
			if (m_pCurrentDocument)
				m_pCurrentDocument->SetDefaultFooter(e);
			throw;
		}
	}

	template <module_error_level_t l = MODULE_ERROR_DEFAULT, typename T, typename U, typename V>
	typename std::enable_if<std::is_signed<T>::value, T>::type
	AssertRange(T Value, U Min, V Max, std::string Desc) const
	{
		try {
			return CModuleException::AssertRangeFmt<l>(Value, Min, Max, Desc, "%i");
		}
		catch (CModuleException *e) {
			if (m_pCurrentDocument)
				m_pCurrentDocument->SetDefaultFooter(e);
			throw;
		}
	}

private:
	// Block reading methods
	void ReadBlock_Parameters(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_SongInfo(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Tuning(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Header(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Instruments(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Sequences(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_SequencesVRC6(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_SequencesN163(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_SequencesS5B(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Frames(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Patterns(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_DSamples(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Comments(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_ChannelLayout(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_DetuneTables(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Grooves(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_Bookmarks(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_ParamsExtra(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_JSON(CDocumentFile *pDocFile, const int Version);
	void ReadBlock_ParamsEmu(CDocumentFile *pDocFile, const int Version);

	void ReorderSequences();
	void SetupChannels(unsigned char Chip);
	void ResetDetuneTables();
	void ResetOPLLPatches();
	void ResetLevelOffset();
	void OptionalJSONToInterface(const json& j);

private:
	// State
	bool			m_bFileLoaded;
	unsigned int	m_iFileVersion;
	bool			m_bFileDnModule;

	// Document data
	CPatternData	*m_pTracks[MAX_TRACKS];
	unsigned int	m_iTrackCount;
	unsigned int	m_iChannelsAvailable;

	CInstrumentManager *m_pInstrumentManager;
	CGroove			*m_pGrooveTable[MAX_GROOVE];

	// Module properties
	unsigned char	m_iExpansionChip;
	unsigned int	m_iNamcoChannels;
	vibrato_t		m_iVibratoStyle;
	bool			m_bLinearPitch;
	machine_t		m_iMachine;
	unsigned int	m_iEngineSpeed;
	unsigned int	m_iSpeedSplitPoint;
	int				m_iDetuneTable[6][96];
	int				m_iDetuneSemitone;
	int				m_iDetuneCent;
	unsigned int	m_iPlaybackRate;
	unsigned int	m_iPlaybackRateType;

	// Emulation properties
	bool			m_bUseExternalOPLLChip;
	uint8_t			m_iOPLLPatchBytes[19 * 8];
	std::string		m_strOPLLPatchNames[19];

	// JSON optional data
	int16_t			m_iDeviceLevelOffset[8];
	bool			m_bUseSurveyMixing;

	// NSF info
	char			m_strName[32];
	char			m_strArtist[32];
	char			m_strCopyright[32];
	CString			m_strComment;
	bool			m_bDisplayComment;
	stHighlight		m_vHighlight;

	std::vector<COldSequence> m_vTmpSequences;
	mutable CDocumentFile *m_pCurrentDocument;
	CString			m_strLastError;
};
