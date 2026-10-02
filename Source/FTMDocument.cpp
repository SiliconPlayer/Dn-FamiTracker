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

#include <iostream>
#include "Common.h"
#include "FamiTrackerTypes.h"
#include "Sequence.h"
#include "Instrument.h"
#include "FTMDocument.h"
#include "SeqInstrument.h"
#include "Instrument2A03.h"
#include "InstrumentVRC6.h"
#include "InstrumentN163.h"
#include "InstrumentS5B.h"
#include "SequenceCollection.h"
#include "SequenceManager.h"
#include "DSampleManager.h"
#include "APU/APU.h"

// File I/O constants
static const char *FILE_BLOCK_PARAMS		= "PARAMS";
static const char *FILE_BLOCK_TUNING		= "TUNING";
static const char *FILE_BLOCK_INFO			= "INFO";
static const char *FILE_BLOCK_INSTRUMENTS	= "INSTRUMENTS";
static const char *FILE_BLOCK_SEQUENCES		= "SEQUENCES";
static const char *FILE_BLOCK_FRAMES		= "FRAMES";
static const char *FILE_BLOCK_PATTERNS		= "PATTERNS";
static const char *FILE_BLOCK_DSAMPLES		= "DPCM SAMPLES";
static const char *FILE_BLOCK_HEADER		= "HEADER";
static const char *FILE_BLOCK_COMMENTS		= "COMMENTS";

static const char *FILE_BLOCK_SEQUENCES_VRC6 = "SEQUENCES_VRC6";
static const char *FILE_BLOCK_SEQUENCES_N163 = "SEQUENCES_N163";
static const char *FILE_BLOCK_SEQUENCES_N106 = "SEQUENCES_N106";
static const char *FILE_BLOCK_SEQUENCES_S5B  = "SEQUENCES_S5B";

static const char *FILE_BLOCK_DETUNETABLES	= "DETUNETABLES";
static const char *FILE_BLOCK_GROOVES		= "GROOVES";
static const char *FILE_BLOCK_BOOKMARKS		= "BOOKMARKS";
static const char *FILE_BLOCK_PARAMS_EXTRA	= "PARAMS_EXTRA";

static const char *FILE_BLOCK_JSON			= "JSON";
static const char *FILE_BLOCK_PARAMS_EMU	= "PARAMS_EMU";

// JSON key names
static const char* APU1_OFFSET = "apu1-offset";
static const char* APU2_OFFSET = "apu2-offset";
static const char* VRC6_OFFSET = "vrc6-offset";
static const char* VRC7_OFFSET = "vrc7-offset";
static const char* FDS_OFFSET  = "fds-offset";
static const char* MMC5_OFFSET = "mmc5-offset";
static const char* N163_OFFSET = "n163-offset";
static const char* S5B_OFFSET  = "s5b-offset";
static const char* USE_SURVEY_MIX = "use-survey-mix";

struct stJSONOptionalData {
	int16_t APU1_OFFSET = 0;
	int16_t APU2_OFFSET = 0;
	int16_t VRC6_OFFSET = 0;
	int16_t VRC7_OFFSET = 0;
	int16_t FDS_OFFSET  = 0;
	int16_t MMC5_OFFSET = 0;
	int16_t N163_OFFSET = 0;
	int16_t S5B_OFFSET  = 0;
	bool USE_SURVEY_MIX = false;
};

[[maybe_unused]] static void from_json(const json& j, stJSONOptionalData& d) {
	if (j.contains(APU1_OFFSET)) j.at(APU1_OFFSET).get_to(d.APU1_OFFSET);
	if (j.contains(APU2_OFFSET)) j.at(APU2_OFFSET).get_to(d.APU2_OFFSET);
	if (j.contains(VRC6_OFFSET)) j.at(VRC6_OFFSET).get_to(d.VRC6_OFFSET);
	if (j.contains(VRC7_OFFSET)) j.at(VRC7_OFFSET).get_to(d.VRC7_OFFSET);
	if (j.contains(FDS_OFFSET))  j.at(FDS_OFFSET).get_to(d.FDS_OFFSET);
	if (j.contains(MMC5_OFFSET)) j.at(MMC5_OFFSET).get_to(d.MMC5_OFFSET);
	if (j.contains(N163_OFFSET)) j.at(N163_OFFSET).get_to(d.N163_OFFSET);
	if (j.contains(S5B_OFFSET))  j.at(S5B_OFFSET).get_to(d.S5B_OFFSET);
	if (j.contains(USE_SURVEY_MIX)) j.at(USE_SURVEY_MIX).get_to(d.USE_SURVEY_MIX);
}

static void to_json(json& j, const stJSONOptionalData& d) {
	j = json{
		{ APU1_OFFSET, d.APU1_OFFSET },
		{ APU2_OFFSET, d.APU2_OFFSET },
		{ VRC6_OFFSET, d.VRC6_OFFSET },
		{ VRC7_OFFSET, d.VRC7_OFFSET },
		{ FDS_OFFSET, d.FDS_OFFSET },
		{ MMC5_OFFSET, d.MMC5_OFFSET },
		{ N163_OFFSET, d.N163_OFFSET },
		{ S5B_OFFSET, d.S5B_OFFSET },
		{ USE_SURVEY_MIX, d.USE_SURVEY_MIX }
	};
}

CFTMDocument::CFTMDocument() :
	m_bFileLoaded(false),
	m_iFileVersion(0),
	m_bFileDnModule(false),
	m_iTrackCount(0),
	m_iChannelsAvailable(5),
	m_pInstrumentManager(new CInstrumentManager(this)),
	m_iExpansionChip(SNDCHIP_NONE),
	m_iNamcoChannels(1),
	m_iVibratoStyle(VIBRATO_NEW),
	m_bLinearPitch(false),
	m_iMachine(NTSC),
	m_iEngineSpeed(0),
	m_iSpeedSplitPoint(32),
	m_iDetuneSemitone(0),
	m_iDetuneCent(0),
	m_iPlaybackRate(0),
	m_iPlaybackRateType(0),
	m_bUseExternalOPLLChip(false),
	m_bUseSurveyMixing(false),
	m_bDisplayComment(false),
	m_vHighlight(CPatternData::DEFAULT_HIGHLIGHT),
	m_pCurrentDocument(nullptr)
{
	memset(m_pTracks, 0, sizeof(m_pTracks));
	memset(m_pGrooveTable, 0, sizeof(m_pGrooveTable));
	memset(m_strName, 0, sizeof(m_strName));
	memset(m_strArtist, 0, sizeof(m_strArtist));
	memset(m_strCopyright, 0, sizeof(m_strCopyright));

	ResetDetuneTables();
	ResetOPLLPatches();
	ResetLevelOffset();
}

CFTMDocument::~CFTMDocument()
{
	DeleteContents();
	SAFE_RELEASE(m_pInstrumentManager);
}

void CFTMDocument::DeleteContents()
{
	for (unsigned int i = 0; i < MAX_TRACKS; ++i) {
		SAFE_RELEASE(m_pTracks[i]);
	}
	m_iTrackCount = 0;

	for (int i = 0; i < MAX_GROOVE; ++i) {
		SAFE_RELEASE(m_pGrooveTable[i]);
	}

	if (m_pInstrumentManager) {
		m_pInstrumentManager->ClearAll();
	}

	m_vTmpSequences.clear();
	ResetDetuneTables();
	ResetOPLLPatches();
	ResetLevelOffset();

	m_strName[0] = 0;
	m_strArtist[0] = 0;
	m_strCopyright[0] = 0;
	m_strComment.Empty();

	m_bFileLoaded = false;
	m_iFileVersion = 0;
	m_bFileDnModule = false;
}

void CFTMDocument::CreateEmpty()
{
	DeleteContents();
	m_iExpansionChip = SNDCHIP_NONE;
	m_iChannelsAvailable = 5;
	m_iMachine = NTSC;
	m_iEngineSpeed = 0;
	m_iSpeedSplitPoint = 32;
	m_vHighlight = CPatternData::DEFAULT_HIGHLIGHT;
	AllocateTrack(0);
}

void CFTMDocument::AllocateTrack(unsigned int Song)
{
	if (Song >= MAX_TRACKS) return;
	if (m_pTracks[Song] == nullptr) {
		m_pTracks[Song] = new CPatternData();
		if (Song >= m_iTrackCount)
			m_iTrackCount = Song + 1;
	}
}

CPatternData* CFTMDocument::GetTrack(unsigned int Track) const
{
	if (Track >= MAX_TRACKS) return nullptr;
	return m_pTracks[Track];
}

CSequenceManager *const CFTMDocument::GetSequenceManager(int InstType) const
{
	return m_pInstrumentManager ? m_pInstrumentManager->GetSequenceManager(InstType) : nullptr;
}

CInstrumentManager *const CFTMDocument::GetInstrumentManager() const
{
	return m_pInstrumentManager;
}

CDSampleManager *const CFTMDocument::GetDSampleManager() const
{
	return m_pInstrumentManager ? m_pInstrumentManager->GetDSampleManager() : nullptr;
}

const CGroove* CFTMDocument::GetGroove(int Index) const
{
	if (Index < 0 || Index >= MAX_GROOVE) return nullptr;
	return m_pGrooveTable[Index];
}

void CFTMDocument::SetGroove(int Index, const CGroove* Groove)
{
	if (Index < 0 || Index >= MAX_GROOVE) return;
	SAFE_RELEASE(m_pGrooveTable[Index]);
	if (Groove != nullptr)
		m_pGrooveTable[Index] = new CGroove(*Groove);
}

int CFTMDocument::GetDetuneOffset(int Chip, int Note) const
{
	if (Chip < 0 || Chip >= 6 || Note < 0 || Note >= NOTE_COUNT) return 0;
	return m_iDetuneTable[Chip][Note];
}

void CFTMDocument::ResetDetuneTables()
{
	for (int i = 0; i < 6; i++)
		for (int j = 0; j < NOTE_COUNT; j++)
			m_iDetuneTable[i][j] = 0;
}

void CFTMDocument::ResetLevelOffset()
{
	for (int i = 0; i < 8; i++)
		m_iDeviceLevelOffset[i] = 0;
}

void CFTMDocument::ResetOPLLPatches()
{
	m_bUseExternalOPLLChip = false;
	for (int i = 0; i < 19; i++) {
		for (int j = 0; j < 8; j++) {
			m_iOPLLPatchBytes[(8 * i) + j] = CAPU::OPLL_DEFAULT_PATCHES[0][(8 * i) + j];
			if (i == 0)
				m_iOPLLPatchBytes[(8 * i) + j] = 0;
		}
		m_strOPLLPatchNames[i] = CAPU::OPLL_PATCHNAME_VRC7[i];
	}
}

void CFTMDocument::SetSample(unsigned int Index, CDSample *pSamp)
{
	if (Index < CDSampleManager::MAX_DSAMPLES && m_pInstrumentManager) {
		m_pInstrumentManager->GetDSampleManager()->SetDSample(Index, pSamp);
	}
}

int CFTMDocument::GetPatternLength(int Track) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track]) return 64;
	return m_pTracks[Track]->GetPatternLength();
}

int CFTMDocument::GetFrameCount(int Track) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track]) return 0;
	return m_pTracks[Track]->GetFrameCount();
}

int CFTMDocument::GetSongSpeed(int Track) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track]) return 6;
	return m_pTracks[Track]->GetSongSpeed();
}

int CFTMDocument::GetSongTempo(int Track) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track]) return 150;
	return m_pTracks[Track]->GetSongTempo();
}

bool CFTMDocument::GetSongGroove(int Track) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track]) return false;
	return m_pTracks[Track]->GetSongGroove();
}

int CFTMDocument::GetEffColumns(int Track, int Channel) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track]) return 0;
	return m_pTracks[Track]->GetEffectColumnCount(Channel);
}

void CFTMDocument::GetNoteData(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Row, stChanNote *Data) const
{
	if (Track >= m_iTrackCount || Channel >= m_iChannelsAvailable || !m_pTracks[Track] || !Data)
		return;
	if (Frame >= (unsigned int)GetFrameCount(Track) || Row >= (unsigned int)GetPatternLength(Track))
		return;
	unsigned int Pattern = m_pTracks[Track]->GetFramePattern(Frame, Channel);
	stChanNote *Note = m_pTracks[Track]->GetPatternData(Channel, Pattern, Row);
	if (Note) {
		*Data = *Note;
	}
}

bool CFTMDocument::LoadDocument(const char* lpszPathName)
{
	CDocumentFile docFile;
	CFileException ex;
	if (!docFile.Open(lpszPathName, CFile::modeRead | CFile::shareDenyWrite, &ex)) {
		m_strLastError = "Failed to open file";
		return false;
	}
	if (docFile.GetLength() == 0) {
		CreateEmpty();
		return true;
	}

	m_strLastError.Empty();
	m_pCurrentDocument = &docFile;
	try {
		DeleteContents();
		docFile.ValidateFile();
		m_iFileVersion = docFile.GetFileVersion();
		m_bFileDnModule = docFile.GetModuleType();

		if (m_iFileVersion < 0x0200U) {
			m_pCurrentDocument = nullptr;
			m_strLastError = "File version < 0x0200 is not supported";
			return false;
		}

		if (!OpenDocumentNew(docFile)) {
			m_pCurrentDocument = nullptr;
			m_strLastError = "Failed to parse document blocks";
			return false;
		}
	}
	catch (CModuleException *e) {
		m_pCurrentDocument = nullptr;
		m_strLastError = e ? e->GetErrorString().c_str() : "Unknown ModuleException";
		delete e;
		return false;
	}
	catch (const std::exception& ex) {
		m_pCurrentDocument = nullptr;
		m_strLastError = ex.what();
		return false;
	}
	catch (...) {
		m_pCurrentDocument = nullptr;
		m_strLastError = "Unknown exception during document load";
		return false;
	}

	m_pCurrentDocument = nullptr;
	m_bFileLoaded = true;
	return true;
}

bool CFTMDocument::LoadDocument(const void* pData, size_t nSize)
{
	if (!pData || nSize == 0) {
		m_strLastError = "Invalid memory buffer";
		return false;
	}
	CDocumentFile docFile;
	if (!docFile.OpenMemory(pData, nSize)) {
		m_strLastError = "Failed to open memory stream";
		return false;
	}

	m_strLastError.Empty();
	m_pCurrentDocument = &docFile;
	try {
		DeleteContents();
		docFile.ValidateFile();
		m_iFileVersion = docFile.GetFileVersion();
		m_bFileDnModule = docFile.GetModuleType();

		if (m_iFileVersion < 0x0200U) {
			m_pCurrentDocument = nullptr;
			m_strLastError = "File version < 0x0200 is not supported";
			return false;
		}

		if (!OpenDocumentNew(docFile)) {
			m_pCurrentDocument = nullptr;
			m_strLastError = "Failed to parse document blocks";
			return false;
		}
	}
	catch (CModuleException *e) {
		m_pCurrentDocument = nullptr;
		m_strLastError = e ? e->GetErrorString().c_str() : "Unknown ModuleException";
		delete e;
		return false;
	}
	catch (const std::exception& ex) {
		m_pCurrentDocument = nullptr;
		m_strLastError = ex.what();
		return false;
	}
	catch (...) {
		m_pCurrentDocument = nullptr;
		m_strLastError = "Unknown exception during document load";
		return false;
	}

	m_pCurrentDocument = nullptr;
	m_bFileLoaded = true;
	return true;
}

bool CFTMDocument::OpenDocumentNew(CDocumentFile &DocumentFile)
{
	static std::unordered_map<std::string, void (CFTMDocument::*)(CDocumentFile*, const int)> FTM_READ_FUNC;
	if (FTM_READ_FUNC.empty()) {
		FTM_READ_FUNC[FILE_BLOCK_PARAMS]			= &CFTMDocument::ReadBlock_Parameters;
		FTM_READ_FUNC[FILE_BLOCK_INFO]				= &CFTMDocument::ReadBlock_SongInfo;
		FTM_READ_FUNC[FILE_BLOCK_TUNING]			= &CFTMDocument::ReadBlock_Tuning;
		FTM_READ_FUNC[FILE_BLOCK_INSTRUMENTS]		= &CFTMDocument::ReadBlock_Instruments;
		FTM_READ_FUNC[FILE_BLOCK_SEQUENCES]			= &CFTMDocument::ReadBlock_Sequences;
		FTM_READ_FUNC[FILE_BLOCK_FRAMES]			= &CFTMDocument::ReadBlock_Frames;
		FTM_READ_FUNC[FILE_BLOCK_PATTERNS]			= &CFTMDocument::ReadBlock_Patterns;
		FTM_READ_FUNC[FILE_BLOCK_DSAMPLES]			= &CFTMDocument::ReadBlock_DSamples;
		FTM_READ_FUNC[FILE_BLOCK_HEADER]			= &CFTMDocument::ReadBlock_Header;
		FTM_READ_FUNC[FILE_BLOCK_COMMENTS]			= &CFTMDocument::ReadBlock_Comments;
		FTM_READ_FUNC[FILE_BLOCK_SEQUENCES_VRC6]	= &CFTMDocument::ReadBlock_SequencesVRC6;
		FTM_READ_FUNC[FILE_BLOCK_SEQUENCES_N163]	= &CFTMDocument::ReadBlock_SequencesN163;
		FTM_READ_FUNC[FILE_BLOCK_SEQUENCES_N106]	= &CFTMDocument::ReadBlock_SequencesN163;
		FTM_READ_FUNC[FILE_BLOCK_SEQUENCES_S5B]		= &CFTMDocument::ReadBlock_SequencesS5B;
		FTM_READ_FUNC[FILE_BLOCK_DETUNETABLES]		= &CFTMDocument::ReadBlock_DetuneTables;
		FTM_READ_FUNC[FILE_BLOCK_GROOVES]			= &CFTMDocument::ReadBlock_Grooves;
		FTM_READ_FUNC[FILE_BLOCK_BOOKMARKS]			= &CFTMDocument::ReadBlock_Bookmarks;
		FTM_READ_FUNC[FILE_BLOCK_PARAMS_EXTRA]		= &CFTMDocument::ReadBlock_ParamsExtra;
		FTM_READ_FUNC[FILE_BLOCK_JSON]				= &CFTMDocument::ReadBlock_JSON;
		FTM_READ_FUNC[FILE_BLOCK_PARAMS_EMU]		= &CFTMDocument::ReadBlock_ParamsEmu;
	}

	const char *BlockID;
	bool ErrorFlag = false;

	if (m_iFileVersion < 0x0210) {
		AllocateTrack(0);
	}

	while (!DocumentFile.Finished() && !ErrorFlag) {
		ErrorFlag = DocumentFile.ReadBlock();
		BlockID = DocumentFile.GetBlockHeaderID();
		if (!strcmp(BlockID, "END")) break;

		try {
			auto it = FTM_READ_FUNC.find(BlockID);
			if (it != FTM_READ_FUNC.end()) {
				CALL_MEMBER_FN(this, it->second)(&DocumentFile, DocumentFile.GetBlockVersion());
			} else if (DocumentFile.IsFileIncomplete()) {
				ErrorFlag = true;
			}
		}
		catch (const std::out_of_range&) {
			if (DocumentFile.IsFileIncomplete())
				ErrorFlag = true;
		}
	}

	DocumentFile.Close();

	if (ErrorFlag) {
		DeleteContents();
		return false;
	}

	if (m_iFileVersion <= 0x0201)
		ReorderSequences();

	return true;
}

void CFTMDocument::ReadBlock_Parameters(CDocumentFile *pDocFile, const int Version)
{
	CPatternData *pTrack = GetTrack(0);

	if (Version == 1) {
		if (pTrack) pTrack->SetSongSpeed(pDocFile->GetBlockInt());
	}
	else
		m_iExpansionChip = pDocFile->GetBlockChar();

	m_iChannelsAvailable = AssertRange(pDocFile->GetBlockInt(), 1, MAX_CHANNELS, "Channel count");

	m_iMachine = static_cast<machine_t>(pDocFile->GetBlockInt());
	AssertFileData(m_iMachine == NTSC || m_iMachine == PAL, "Unknown machine");

	if (Version >= 7) {
		m_iPlaybackRateType = AssertRange(pDocFile->GetBlockInt(), 0, 2, "Playback rate type");
		m_iPlaybackRate = AssertRange(pDocFile->GetBlockInt(), 0, 0xFFFF, "Playback rate");
		switch (m_iPlaybackRateType) {
		case 1:
			m_iEngineSpeed = static_cast<unsigned int>(1000000. / m_iPlaybackRate + .5);
			break;
		case 0: case 2:
		default:
			m_iEngineSpeed = 0;
		}
	}
	else
		m_iEngineSpeed = pDocFile->GetBlockInt();

	if (Version > 2)
		m_iVibratoStyle = (vibrato_t)pDocFile->GetBlockInt();
	else
		m_iVibratoStyle = VIBRATO_OLD;

	if (Version >= 9) {
		(void)pDocFile->GetBlockInt();
	}

	m_vHighlight = CPatternData::DEFAULT_HIGHLIGHT;

	if (Version > 3 && Version <= 6) {
		m_vHighlight.First = pDocFile->GetBlockInt();
		m_vHighlight.Second = pDocFile->GetBlockInt();
	}

	if (m_iChannelsAvailable == 5)
		m_iExpansionChip = 0;

	if (m_iFileVersion == 0x0200 && pTrack) {
		int Speed = pTrack->GetSongSpeed();
		if (Speed < 20)
			pTrack->SetSongSpeed(Speed + 1);
	}

	if (Version == 1 && pTrack) {
		if (pTrack->GetSongSpeed() > 19) {
			pTrack->SetSongTempo(pTrack->GetSongSpeed());
			pTrack->SetSongSpeed(6);
		}
		else {
			pTrack->SetSongTempo(m_iMachine == NTSC ? DEFAULT_TEMPO_NTSC : DEFAULT_TEMPO_PAL);
		}
	}

	if (Version >= 5 && (m_iExpansionChip & SNDCHIP_N163))
		m_iNamcoChannels = AssertRange(pDocFile->GetBlockInt(), 1, 8, "Namco 163 channel count");
	else
		m_iNamcoChannels = 0;

	if (Version >= 6) {
		m_iSpeedSplitPoint = pDocFile->GetBlockInt();
	}
	else {
		m_iSpeedSplitPoint = OLD_SPEED_SPLIT_POINT;
	}

	AssertRange<MODULE_ERROR_STRICT>(m_iExpansionChip, 0, 0x3F, "Expansion chip flag");

	if (Version == 8) {
		m_iDetuneSemitone = pDocFile->GetBlockChar();
		m_iDetuneCent = pDocFile->GetBlockChar();
	}

	SetupChannels(m_iExpansionChip);
}

void CFTMDocument::SetupChannels(unsigned char Chip)
{
	m_iExpansionChip = Chip;
	// Compute channels available based on expansion chip
	int count = 5; // Standard 2A03
	if (Chip & SNDCHIP_VRC6) count += 3;
	if (Chip & SNDCHIP_VRC7) count += 6;
	if (Chip & SNDCHIP_FDS)  count += 1;
	if (Chip & SNDCHIP_MMC5) count += 2;
	if (Chip & SNDCHIP_N163) count += m_iNamcoChannels;
	if (Chip & SNDCHIP_S5B)  count += 3;
	m_iChannelsAvailable = count;
}

void CFTMDocument::ReadBlock_SongInfo(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	pDocFile->GetBlock(m_strName, 32);
	pDocFile->GetBlock(m_strArtist, 32);
	pDocFile->GetBlock(m_strCopyright, 32);
}

void CFTMDocument::ReadBlock_Tuning(CDocumentFile* pDocFile, const int Version)
{
	if (Version == 1) {
		m_iDetuneSemitone = AssertRange(pDocFile->GetBlockChar(), -12, 12, "Global semitone tuning");
		m_iDetuneCent = AssertRange(pDocFile->GetBlockChar(), -100, 100, "Global cent tuning");
	}
}

void CFTMDocument::ReadBlock_Header(CDocumentFile *pDocFile, const int Version)
{
	if (Version == 1) {
		m_iTrackCount = 1;
		AllocateTrack(0);
		CPatternData *pTrack = GetTrack(0);
		for (unsigned int i = 0; i < m_iChannelsAvailable; ++i) try {
			(void)pDocFile->GetBlockChar();
			pTrack->SetEffectColumnCount(i, AssertRange<MODULE_ERROR_STRICT>(
				pDocFile->GetBlockChar(), 0, MAX_EFFECT_COLUMNS - 1, "Effect column count"));
		}
		catch (CModuleException *e) {
			e->AppendError("At channel %d", i + 1);
			throw;
		}
	}
	else if (Version >= 2) {
		m_iTrackCount = AssertRange(pDocFile->GetBlockChar() + 1, 1, static_cast<int>(MAX_TRACKS), "Track count");

		for (unsigned i = 0; i < m_iTrackCount; ++i)
			AllocateTrack(i);

		if (Version >= 3)
			for (unsigned i = 0; i < m_iTrackCount; ++i)
				m_pTracks[i]->SetTitle(pDocFile->ReadString());

		for (unsigned i = 0; i < m_iChannelsAvailable; ++i) try {
			(void)pDocFile->GetBlockChar();
			for (unsigned j = 0; j < m_iTrackCount; ++j) try {
				GetTrack(j)->SetEffectColumnCount(i, AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockChar(), 0, MAX_EFFECT_COLUMNS - 1, "Effect column count"));
			}
			catch (CModuleException *e) {
				e->AppendError("At effect column fx%d,", j + 1);
				throw;
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At channel %d,", i + 1);
			throw;
		}

		if (Version >= 4) {
			for (unsigned int i = 0; i < m_iTrackCount; ++i) {
				int First = static_cast<unsigned char>(pDocFile->GetBlockChar());
				int Second = static_cast<unsigned char>(pDocFile->GetBlockChar());
				if (i == 0) {
					m_vHighlight.First = First;
					m_vHighlight.Second = Second;
				}
			}
		}
		for (unsigned int i = 0; i < m_iTrackCount; ++i)
			GetTrack(i)->SetHighlight(m_vHighlight);
	}
}

void CFTMDocument::ReadBlock_Comments(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	m_bDisplayComment = (pDocFile->GetBlockInt() == 1);
	m_strComment = pDocFile->ReadString();
}

void CFTMDocument::ReadBlock_ChannelLayout(CDocumentFile *pDocFile, const int Version)
{
	(void)pDocFile; (void)Version;
}

void CFTMDocument::ReadBlock_Instruments(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	const int Count = AssertRange(pDocFile->GetBlockInt(), 0, CInstrumentManager::MAX_INSTRUMENTS, "Instrument count");

	for (int i = 0; i < Count; ++i) {
		int index = AssertRange(pDocFile->GetBlockInt(), 0, CInstrumentManager::MAX_INSTRUMENTS - 1, "Instrument index");
		inst_type_t Type = (inst_type_t)pDocFile->GetBlockChar();
		auto pInstrument = CInstrumentManager::CreateNew(Type);
		m_pInstrumentManager->InsertInstrument(index, pInstrument);

		try {
			AssertFileData(pInstrument.get() != nullptr, "Failed to create instrument");
			pInstrument->Load(pDocFile);
			int size = AssertRange(pDocFile->GetBlockInt(), 0, CInstrument::INST_NAME_MAX, "Instrument name length");
			char Name[CInstrument::INST_NAME_MAX + 1];
			pDocFile->GetBlock(Name, size);
			Name[size] = 0;
			pInstrument->SetName(Name);
		}
		catch (CModuleException *e) {
			pDocFile->SetDefaultFooter(e);
			e->AppendError("At instrument %02X,", index);
			m_pInstrumentManager->RemoveInstrument(index);
			throw;
		}
	}
}

void CFTMDocument::ReadBlock_Sequences(CDocumentFile *pDocFile, const int Version)
{
	unsigned int Count = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES * SEQ_COUNT, "2A03 sequence count");

	if (Version == 1) {
		for (unsigned int i = 0; i < Count; ++i) {
			COldSequence Seq;
			(void)AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
			unsigned int SeqCount = static_cast<unsigned char>(pDocFile->GetBlockChar());
			AssertRange(SeqCount, 0U, static_cast<unsigned>(MAX_SEQUENCE_ITEMS - 1), "Sequence item count");
			for (unsigned int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				Seq.AddItem(pDocFile->GetBlockChar(), Value);
			}
			m_vTmpSequences.push_back(Seq);
		}
	}
	else if (Version == 2) {
		for (unsigned int i = 0; i < Count; ++i) {
			COldSequence Seq;
			unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
			unsigned int Type = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
			unsigned int SeqCount = static_cast<unsigned char>(pDocFile->GetBlockChar());
			AssertRange(SeqCount, 0U, static_cast<unsigned>(MAX_SEQUENCE_ITEMS - 1), "Sequence item count");
			for (unsigned int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				Seq.AddItem(pDocFile->GetBlockChar(), Value);
			}
			m_pInstrumentManager->SetSequence(INST_2A03, Type, Index, Seq.Convert(Type));
		}
	}
	else if (Version >= 3) {
		CSequenceManager *pManager = GetSequenceManager(INST_2A03);

		for (unsigned int i = 0; i < Count; ++i) {
			unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
			unsigned int Type = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
			try {
				unsigned char SeqCount = pDocFile->GetBlockChar();
				CSequence *pSeq = pManager->GetCollection(Type)->GetSequence(Index);
				pSeq->Clear();
				pSeq->SetItemCount(SeqCount < MAX_SEQUENCE_ITEMS ? SeqCount : MAX_SEQUENCE_ITEMS);

				unsigned int LoopPoint = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount), "Sequence loop point");
				if (LoopPoint != SeqCount)
					pSeq->SetLoopPoint(LoopPoint);

				if (Version == 4) {
					int ReleasePoint = pDocFile->GetBlockInt();
					int Settings = pDocFile->GetBlockInt();
					pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
						ReleasePoint, -1, static_cast<int>(SeqCount) - 1, "Sequence release point"));
					pSeq->SetSetting(static_cast<seq_setting_t>(Settings));
				}

				for (int j = 0; j < SeqCount; ++j) {
					char Value = pDocFile->GetBlockChar();
					if (j < MAX_SEQUENCE_ITEMS)
						pSeq->SetItem(j, Value);
				}
			}
			catch (CModuleException *e) {
				e->AppendError("At 2A03 %s sequence %d,", CInstrument2A03::SEQUENCE_NAME[Type], Index);
				throw;
			}
		}
	}
}

void CFTMDocument::ReadBlock_SequencesVRC6(CDocumentFile *pDocFile, const int Version)
{
	unsigned int Count = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES * SEQ_COUNT, "VRC6 sequence count");
	CSequenceManager *pManager = GetSequenceManager(INST_VRC6);

	for (unsigned int i = 0; i < Count; ++i) {
		unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
		unsigned int Type = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
		try {
			unsigned char SeqCount = pDocFile->GetBlockChar();
			CSequence *pSeq = pManager->GetCollection(Type)->GetSequence(Index);
			pSeq->Clear();
			pSeq->SetItemCount(SeqCount < MAX_SEQUENCE_ITEMS ? SeqCount : MAX_SEQUENCE_ITEMS);

			unsigned int LoopPoint = AssertRange<MODULE_ERROR_STRICT>(
				pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount), "Sequence loop point");
			if (LoopPoint != SeqCount)
				pSeq->SetLoopPoint(LoopPoint);

			if (Version == 2) {
				int ReleasePoint = pDocFile->GetBlockInt();
				int Settings = pDocFile->GetBlockInt();
				pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
					ReleasePoint, -1, static_cast<int>(SeqCount) - 1, "Sequence release point"));
				pSeq->SetSetting(static_cast<seq_setting_t>(Settings));
			}

			for (int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				if (j < MAX_SEQUENCE_ITEMS)
					pSeq->SetItem(j, Value);
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At VRC6 %s sequence %d,", CInstrumentVRC6::SEQUENCE_NAME[Type], Index);
			throw;
		}
	}
}

void CFTMDocument::ReadBlock_SequencesN163(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	unsigned int Count = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES * SEQ_COUNT, "N163 sequence count");
	CSequenceManager *pManager = GetSequenceManager(INST_N163);

	for (unsigned int i = 0; i < Count; ++i) {
		unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
		unsigned int Type = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
		try {
			unsigned char SeqCount = pDocFile->GetBlockChar();
			CSequence *pSeq = pManager->GetCollection(Type)->GetSequence(Index);
			pSeq->Clear();
			pSeq->SetItemCount(SeqCount < MAX_SEQUENCE_ITEMS ? SeqCount : MAX_SEQUENCE_ITEMS);

			pSeq->SetLoopPoint(AssertRange<MODULE_ERROR_STRICT>(
				pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount) - 1, "Sequence loop point"));
			pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
				pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount) - 1, "Sequence release point"));
			pSeq->SetSetting(static_cast<seq_setting_t>(pDocFile->GetBlockInt()));

			for (int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				if (j < MAX_SEQUENCE_ITEMS)
					pSeq->SetItem(j, Value);
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At N163 sequence %d,", Index);
			throw;
		}
	}
}

void CFTMDocument::ReadBlock_SequencesS5B(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	unsigned int Count = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES * SEQ_COUNT, "5B sequence count");
	CSequenceManager *pManager = GetSequenceManager(INST_S5B);

	for (unsigned int i = 0; i < Count; ++i) {
		unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
		unsigned int Type = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
		try {
			unsigned char SeqCount = pDocFile->GetBlockChar();
			CSequence *pSeq = pManager->GetCollection(Type)->GetSequence(Index);
			pSeq->Clear();
			pSeq->SetItemCount(SeqCount < MAX_SEQUENCE_ITEMS ? SeqCount : MAX_SEQUENCE_ITEMS);

			pSeq->SetLoopPoint(AssertRange<MODULE_ERROR_STRICT>(
				pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount) - 1, "Sequence loop point"));
			pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
				pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount) - 1, "Sequence release point"));
			pSeq->SetSetting(static_cast<seq_setting_t>(pDocFile->GetBlockInt()));

			for (int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				if (j < MAX_SEQUENCE_ITEMS)
					pSeq->SetItem(j, Value);
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At 5B %s sequence %d,", CInstrumentS5B::SEQUENCE_NAME[Type], Index);
			throw;
		}
	}
}

void CFTMDocument::ReadBlock_Frames(CDocumentFile *pDocFile, const int Version)
{
	if (Version == 1) {
		unsigned int FrameCount = AssertRange(pDocFile->GetBlockInt(), 1, MAX_FRAMES, "Track frame count");
		m_iChannelsAvailable = AssertRange(pDocFile->GetBlockInt(), 0, MAX_CHANNELS, "Channel count");
		AllocateTrack(0);
		CPatternData *pTrack = GetTrack(0);
		pTrack->SetFrameCount(FrameCount);
		for (unsigned i = 0; i < FrameCount; ++i) {
			for (unsigned j = 0; j < m_iChannelsAvailable; ++j) {
				unsigned Pattern = static_cast<unsigned char>(pDocFile->GetBlockChar());
				AssertRange(Pattern, 0U, static_cast<unsigned>(MAX_PATTERN - 1), "Pattern index");
				pTrack->SetFramePattern(i, j, Pattern);
			}
		}
	}
	else if (Version > 1) {
		for (unsigned y = 0; y < m_iTrackCount; ++y) {
			unsigned int FrameCount = AssertRange(pDocFile->GetBlockInt(), 1, MAX_FRAMES, "Track frame count");
			unsigned int Speed = AssertRange<MODULE_ERROR_STRICT>(pDocFile->GetBlockInt(), 0, MAX_TEMPO, "Track default speed");

			AllocateTrack(y);
			CPatternData *pTrack = GetTrack(y);
			pTrack->SetFrameCount(FrameCount);

			if (Version >= 3) {
				unsigned int Tempo = AssertRange<MODULE_ERROR_STRICT>(pDocFile->GetBlockInt(), 0, MAX_TEMPO, "Track default tempo");
				pTrack->SetSongTempo(Tempo);
				pTrack->SetSongSpeed(Speed);
			}
			else {
				if (Speed < 20) {
					pTrack->SetSongTempo(m_iMachine == NTSC ? DEFAULT_TEMPO_NTSC : DEFAULT_TEMPO_PAL);
					pTrack->SetSongSpeed(Speed);
				}
				else {
					pTrack->SetSongTempo(Speed);
					pTrack->SetSongSpeed(DEFAULT_SPEED);
				}
			}

			unsigned PatternLength = AssertRange(pDocFile->GetBlockInt(), 1, MAX_PATTERN_LENGTH, "Track default row count");
			pTrack->SetPatternLength(PatternLength);

			for (unsigned i = 0; i < FrameCount; ++i) {
				for (unsigned j = 0; j < m_iChannelsAvailable; ++j) {
					int Pattern = static_cast<unsigned char>(pDocFile->GetBlockChar());
					pTrack->SetFramePattern(i, j, AssertRange(Pattern, 0, MAX_PATTERN - 1, "Pattern index"));
				}
			}
		}
	}
}

void CFTMDocument::ReadBlock_Patterns(CDocumentFile *pDocFile, const int Version)
{
	if (Version == 1) {
		int PatternLen = AssertRange(pDocFile->GetBlockInt(), 0, MAX_PATTERN_LENGTH, "Pattern data count");
		AllocateTrack(0);
		CPatternData *pTrack = GetTrack(0);
		pTrack->SetPatternLength(PatternLen);
	}

	while (!pDocFile->BlockDone()) {
		unsigned Track = 0;
		if (Version > 1)
			Track = AssertRange(pDocFile->GetBlockInt(), 0, static_cast<int>(MAX_TRACKS) - 1, "Pattern track index");

		unsigned Channel = AssertRange(pDocFile->GetBlockInt(), 0, MAX_CHANNELS - 1, "Pattern channel index");
		unsigned Pattern = AssertRange(pDocFile->GetBlockInt(), 0, MAX_PATTERN - 1, "Pattern index");
		unsigned Items   = AssertRange(pDocFile->GetBlockInt(), 0, MAX_PATTERN_LENGTH, "Pattern data count");

		AllocateTrack(Track);
		CPatternData *pTrack = GetTrack(Track);

		for (unsigned i = 0; i < Items; ++i) try {
			unsigned char Row;
			if (m_iFileVersion == 0x0200 || Version >= 6)
				Row = pDocFile->GetBlockChar();
			else
				Row = AssertRange(pDocFile->GetBlockInt(), 0, 0xFF, "Row index");

			try {
				stChanNote *Note = pTrack->GetPatternData(Channel, Pattern, Row);
				*Note = stChanNote { };

				Note->Note = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockChar(), NONE, ECHO, "Note value");
				Note->Octave = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockChar(), 0, OCTAVE_RANGE - 1, "Octave value");
				int Inst = pDocFile->GetBlockChar();
				if (Inst != HOLD_INSTRUMENT)
					AssertRange<MODULE_ERROR_STRICT>(Inst, 0, CInstrumentManager::MAX_INSTRUMENTS, "Instrument index");
				Note->Instrument = Inst;
				Note->Vol = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockChar(), 0, MAX_VOLUME, "Channel volume");

				int FX = m_iFileVersion == 0x200 ? 1 : Version >= 6 ? MAX_EFFECT_COLUMNS :
						 (pTrack->GetEffectColumnCount(Channel) + 1);
				for (int n = 0; n < FX; ++n) try {
					unsigned char EffectNumber = pDocFile->GetBlockChar();
					if ((Note->EffNumber[n] = static_cast<effect_t>(EffectNumber))) {
						AssertRange<MODULE_ERROR_STRICT>(EffectNumber, EF_NONE, EF_COUNT - 1, "Effect index");
						unsigned char EffectParam = pDocFile->GetBlockChar();
						if (Version < 3) {
							if (EffectNumber == EF_PORTAOFF) {
								EffectNumber = EF_PORTAMENTO;
								EffectParam = 0;
							}
							else if (EffectNumber == EF_PORTAMENTO) {
								if (EffectParam < 0xFF)
									EffectParam++;
							}
						}
						Note->EffNumber[n] = static_cast<effect_t>(EffectNumber);
						Note->EffParam[n] = EffectParam;
					}
					else {
						(void)pDocFile->GetBlockChar();
					}
				}
				catch (CModuleException *e) {
					e->AppendError("At effect column %d,", n + 1);
					throw;
				}
			}
			catch (CModuleException *e) {
				e->AppendError("At row %02X,", Row);
				throw;
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At pattern %02X,", Pattern);
			throw;
		}
	}
}

void CFTMDocument::ReadBlock_DSamples(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	unsigned int Count = AssertRange(
		static_cast<unsigned char>(pDocFile->GetBlockChar()), 0U, CDSampleManager::MAX_DSAMPLES, "DPCM sample count");

	for (unsigned int i = 0; i < Count; ++i) {
		unsigned int Index = AssertRange(
			static_cast<unsigned char>(pDocFile->GetBlockChar()), 0U, CDSampleManager::MAX_DSAMPLES - 1, "DPCM sample index");
		CDSample *pSample = nullptr;
		try {
			pSample = new CDSample();
			unsigned int Len = AssertRange(pDocFile->GetBlockInt(), 0, CDSample::MAX_NAME_SIZE - 1, "DPCM sample name length");
			char Name[CDSample::MAX_NAME_SIZE] = {};
			pDocFile->GetBlock(Name, Len);
			pSample->SetName(Name);
			int Size = AssertRange(pDocFile->GetBlockInt(), 0, 0x7FFF, "DPCM sample size");
			int TrueSize = Size + ((1 - Size) & 0x0F);
			char *pData = new char[TrueSize];
			pDocFile->GetBlock(pData, Size);
			memset(pData + Size, 0xAA, TrueSize - Size);
			pSample->SetData(TrueSize, pData);
		}
		catch (CModuleException *e) {
			e->AppendError("At DPCM sample %d,", Index);
			throw;
		}
		SetSample(Index, pSample);
	}
}

void CFTMDocument::ReadBlock_DetuneTables(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	static const char* CHIP_NAMES[] = {"2A03", "VRC6", "VRC7", "FDS", "MMC5", "N163"};
	int Count = AssertRange(pDocFile->GetBlockChar(), 0, 6, "Detune table count");
	for (int i = 0; i < Count; i++) {
		int Chip = AssertRange(pDocFile->GetBlockChar(), 0, 5, "Detune table index");
		try {
			int Item = AssertRange(pDocFile->GetBlockChar(), 0, NOTE_COUNT, "Detune table note count");
			for (int j = 0; j < Item; j++) {
				int Note = AssertRange(pDocFile->GetBlockChar(), 0, NOTE_COUNT - 1, "Detune table note index");
				int Offset = pDocFile->GetBlockInt();
				m_iDetuneTable[Chip][Note] = Offset;
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At %s detune table,", CHIP_NAMES[Chip]);
			throw;
		}
	}
}

void CFTMDocument::ReadBlock_Grooves(CDocumentFile *pDocFile, const int Version)
{
	(void)Version;
	const int Count = AssertRange(pDocFile->GetBlockChar(), 0, MAX_GROOVE, "Groove count");

	for (int i = 0; i < Count; i++) {
		int Index = AssertRange(pDocFile->GetBlockChar(), 0, MAX_GROOVE - 1, "Groove index");
		try {
			int Size = AssertRange(pDocFile->GetBlockChar(), 1, MAX_GROOVE_SIZE, "Groove size");
			if (m_pGrooveTable[Index] == nullptr)
				m_pGrooveTable[Index] = new CGroove();
			m_pGrooveTable[Index]->SetSize(Size);
			for (int j = 0; j < Size; j++) try {
				m_pGrooveTable[Index]->SetEntry(j, AssertRange(
					static_cast<unsigned char>(pDocFile->GetBlockChar()), 1U, 0xFFU, "Groove item"));
			}
			catch (CModuleException *e) {
				e->AppendError("At position %i,", j);
				throw;
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At groove %i,", Index);
			throw;
		}
	}

	unsigned int Tracks = pDocFile->GetBlockChar();
	for (unsigned i = 0; i < Tracks; ++i) try {
		int Use = pDocFile->GetBlockChar();
		if (i >= m_iTrackCount) continue;
		CPatternData *pTrack = GetTrack(i);
		if (pTrack) {
			pTrack->SetSongGroove(Use == 1);
			int Speed = pTrack->GetSongSpeed();
			if (pTrack->GetSongGroove())
				AssertRange(Speed, 0, MAX_GROOVE - 1, "Track default groove index");
			else
				AssertRange(Speed, 1, MAX_TEMPO, "Track default speed");
		}
	}
	catch (CModuleException *e) {
		e->AppendError("At track %d,", i + 1);
		throw;
	}
}

void CFTMDocument::ReadBlock_Bookmarks(CDocumentFile *pDocFile, const int Version)
{
	(void)pDocFile; (void)Version;
	// Headless engine does not need UI bookmarks
}

void CFTMDocument::ReadBlock_ParamsExtra(CDocumentFile *pDocFile, const int Version)
{
	m_bLinearPitch = pDocFile->GetBlockInt() != 0;
	if (Version >= 2) {
		m_iDetuneSemitone = AssertRange(pDocFile->GetBlockChar(), -12, 12, "Global semitone tuning");
		m_iDetuneCent = AssertRange(pDocFile->GetBlockChar(), -100, 100, "Global cent tuning");
	}
}

void CFTMDocument::ReadBlock_JSON(CDocumentFile* pDocFile, const int Version)
{
	(void)Version;
	const stJSONOptionalData DnDefaultJSONData;
	const json DEFAULT = DnDefaultJSONData;
	json out = DEFAULT;

	CT2A fileData(pDocFile->ReadString());
	try {
		json in = json::parse(static_cast<char*>(fileData));
		for (auto it : in.items()) {
			out[it.key()] = it.value();
		}
		if (out == DEFAULT) return;
		OptionalJSONToInterface(out);
	}
	catch (...) {
		// Ignore malformed JSON
	}
}

void CFTMDocument::OptionalJSONToInterface(const json& j)
{
	if (j.contains(APU1_OFFSET)) SetLevelOffset(0, j.at(APU1_OFFSET));
	if (j.contains(APU2_OFFSET)) SetLevelOffset(1, j.at(APU2_OFFSET));
	if (j.contains(VRC6_OFFSET)) SetLevelOffset(2, j.at(VRC6_OFFSET));
	if (j.contains(VRC7_OFFSET)) SetLevelOffset(3, j.at(VRC7_OFFSET));
	if (j.contains(FDS_OFFSET))  SetLevelOffset(4, j.at(FDS_OFFSET));
	if (j.contains(MMC5_OFFSET)) SetLevelOffset(5, j.at(MMC5_OFFSET));
	if (j.contains(N163_OFFSET)) SetLevelOffset(6, j.at(N163_OFFSET));
	if (j.contains(S5B_OFFSET))  SetLevelOffset(7, j.at(S5B_OFFSET));
	if (j.contains(USE_SURVEY_MIX)) SetSurveyMixCheck(j.at(USE_SURVEY_MIX));
}

void CFTMDocument::ReadBlock_ParamsEmu(CDocumentFile* pDocFile, const int Version)
{
	(void)Version;
	m_bUseExternalOPLLChip = (pDocFile->GetBlockInt() != 0);

	if (m_bUseExternalOPLLChip) {
		for (int i = 0; i < 19; i++) {
			for (int j = 0; j < 8; j++)
				m_iOPLLPatchBytes[(8 * i) + j] = static_cast<uint8_t>(pDocFile->GetBlockChar());
			m_strOPLLPatchNames[i] = std::string(pDocFile->ReadString());
		}
	}
}

void CFTMDocument::ReorderSequences()
{
	int Slots[SEQ_COUNT] = {0, 0, 0, 0, 0};
	int Indices[MAX_SEQUENCES][SEQ_COUNT];

	memset(Indices, 0xFF, MAX_SEQUENCES * SEQ_COUNT * sizeof(int));

	for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
		if (auto pInst = std::dynamic_pointer_cast<CInstrument2A03>(m_pInstrumentManager->GetInstrument(i))) {
			for (int j = 0; j < SEQ_COUNT; ++j) {
				if (pInst->GetSeqEnable(j)) {
					int Index = pInst->GetSeqIndex(j);
					if (Indices[Index][j] >= 0 && Indices[Index][j] != -1) {
						pInst->SetSeqIndex(j, Indices[Index][j]);
					}
					else {
						COldSequence &Seq = m_vTmpSequences[Index];
						if (j == SEQ_VOLUME)
							for (unsigned int k = 0; k < Seq.GetLength(); ++k)
								Seq.Value[k] = std::max(std::min<int>(Seq.Value[k], 15), 0);
						else if (j == SEQ_DUTYCYCLE)
							for (unsigned int k = 0; k < Seq.GetLength(); ++k)
								Seq.Value[k] = std::max(std::min<int>(Seq.Value[k], 3), 0);
						Indices[Index][j] = Slots[j];
						pInst->SetSeqIndex(j, Slots[j]);
						m_pInstrumentManager->SetSequence(INST_2A03, j, Slots[j]++, Seq.Convert(j));
					}
				}
				else
					pInst->SetSeqIndex(j, 0);
			}
		}
	}

	m_vTmpSequences.clear();
}

double CFTMDocument::GetStandardLength(int Track, unsigned int ExtraLoops) const
{
	if (Track < 0 || (unsigned int)Track >= m_iTrackCount || !m_pTracks[Track])
		return 0.0;

	char RowVisited[MAX_FRAMES][MAX_PATTERN_LENGTH];
	int JumpTo = -1;
	int SkipTo = -1;
	double FirstLoop = 0.0;
	double SecondLoop = 0.0;
	bool IsGroove = GetSongGroove(Track);
	double Tempo = GetSongTempo(Track);
	double Speed = GetSongSpeed(Track);
	if (!GetSongTempo(Track))
		Tempo = 2.5 * GetFrameRate();
	int GrooveIndex = GetSongSpeed(Track) * (m_pGrooveTable[GetSongSpeed(Track)] != nullptr), GroovePointer = 0;
	bool bScanning = true;
	unsigned int FrameCount = GetFrameCount(Track);

	if (IsGroove && GetGroove(GetSongSpeed(Track)) == nullptr) {
		IsGroove = false;
		Speed = DEFAULT_SPEED;
	}

	memset(RowVisited, 0, MAX_FRAMES * MAX_PATTERN_LENGTH);

	unsigned int f = 0;
	unsigned int r = 0;
	while (bScanning) {
		bool hasJump = false;
		for (unsigned int j = 0; j < m_iChannelsAvailable; ++j) {
			stChanNote* Note;
			Note = m_pTracks[Track]->GetPatternData(j, m_pTracks[Track]->GetFramePattern(f, j), r);
			for (unsigned l = 0; l < (unsigned)GetEffColumns(Track, j) + 1; ++l) {
				switch (Note->EffNumber[l]) {
				case EF_JUMP:
					JumpTo = Note->EffParam[l];
					SkipTo = 0;
					hasJump = true;
					break;
				case EF_SKIP:
					if (hasJump) break;
					JumpTo = (f + 1) % FrameCount;
					SkipTo = Note->EffParam[l];
					break;
				case EF_HALT:
					ExtraLoops = 0;
					bScanning = false;
					break;
				case EF_SPEED:
					if (GetSongTempo(Track) && Note->EffParam[l] >= m_iSpeedSplitPoint)
						Tempo = Note->EffParam[l];
					else {
						IsGroove = false;
						Speed = Note->EffParam[l];
					}
					break;
				case EF_GROOVE:
					if (m_pGrooveTable[Note->EffParam[l]] == nullptr) break;
					IsGroove = true;
					GrooveIndex = Note->EffParam[l];
					GroovePointer = 0;
					break;
				default:
					break;
				}
			}
		}
		if (IsGroove && m_pGrooveTable[GrooveIndex])
			Speed = m_pGrooveTable[GrooveIndex]->GetEntry(GroovePointer++);

		switch (RowVisited[f][r]) {
		case 0: FirstLoop += Speed / Tempo; break;
		case 1: SecondLoop += Speed / Tempo; break;
		case 2: bScanning = false; break;
		}

		++RowVisited[f][r++];

		if (JumpTo > -1) {
			f = std::min(static_cast<unsigned int>(JumpTo), FrameCount - 1);
			JumpTo = -1;
		}
		if (SkipTo > -1) {
			r = std::min(static_cast<unsigned int>(SkipTo), (unsigned int)GetPatternLength(Track) - 1);
			SkipTo = -1;
		}
		if (r >= (unsigned int)GetPatternLength(Track)) {
			++f;
			r = 0;
		}
		if (f >= FrameCount)
			f = 0;
	}

	return (2.5 * (FirstLoop + SecondLoop * ExtraLoops));
}
