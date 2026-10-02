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

#include "Common.h"
#include "FTMPlayer.h"
#include "ChannelFactory.h"
#include "ChannelsN163.h"
#include "DetuneTable.h"
#include <cmath>
#include <cstring>
#include <algorithm>

static const int NEW_VIBRATO_DEPTH[] = {
	1, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255
};

static const int OLD_VIBRATO_DEPTH[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

CFTMPlayer::CFTMPlayer() :
	m_pDocument(nullptr),
	m_iSampleRate(44100),
	m_iMachine(NTSC),
	m_iActiveChannels(0),
	m_iPlayTrack(0),
	m_iPlayFrame(0),
	m_iPlayRow(0),
	m_iSpeed(6),
	m_iTempo(150),
	m_iTempoAccum(0),
	m_iTempoDecrement(0),
	m_iTempoRemainder(0),
	m_iGrooveIndex(-1),
	m_iGroovePosition(0),
	m_iSpeedSplitPoint(32),
	m_iJumpToPattern(-1),
	m_iSkipToRow(-1),
	m_iStepRows(0),
	m_iRowTickCount(0),
	m_iUpdateCycles(0),
	m_iConsumedCycles(0),
	m_iFrameRate(60),
	m_iTotalTicks(0),
	m_bPlaying(false),
	m_bPaused(false),
	m_bHaltRequest(false),
	m_bDoHalt(false),
	m_bUpdateRow(false),
	m_bFinished(false),
	m_bLoopReached(false),
	m_audioFifoReadPos(0)
{
	memset(m_iChannelIDs, 0, sizeof(m_iChannelIDs));
	memset(m_bChannelMuted, 0, sizeof(m_bChannelMuted));
	memset(m_bVisitedFrames, 0, sizeof(m_bVisitedFrames));
	memset(m_iVibratoTable, 0, sizeof(m_iVibratoTable));
	memset(m_iNoteLookupTableNTSC, 0, sizeof(m_iNoteLookupTableNTSC));
	memset(m_iNoteLookupTablePAL, 0, sizeof(m_iNoteLookupTablePAL));
	memset(m_iNoteLookupTableSaw, 0, sizeof(m_iNoteLookupTableSaw));
	memset(m_iNoteLookupTableVRC7, 0, sizeof(m_iNoteLookupTableVRC7));
	memset(m_iNoteLookupTableFDS, 0, sizeof(m_iNoteLookupTableFDS));
	memset(m_iNoteLookupTableN163, 0, sizeof(m_iNoteLookupTableN163));
	memset(m_iNoteLookupTableS5B, 0, sizeof(m_iNoteLookupTableS5B));
}

CFTMPlayer::~CFTMPlayer()
{
	Reset();
}

bool CFTMPlayer::LoadDocument(const char* lpszPathName)
{
	auto pDoc = std::make_unique<CFTMDocument>();
	if (!pDoc->LoadDocument(lpszPathName)) {
		return false;
	}
	m_pOwnedDocument = std::move(pDoc);
	return AssignDocument(m_pOwnedDocument.get(), false);
}

bool CFTMPlayer::LoadDocument(const void* pData, size_t nSize)
{
	auto pDoc = std::make_unique<CFTMDocument>();
	if (!pDoc->LoadDocument(pData, nSize)) {
		return false;
	}
	m_pOwnedDocument = std::move(pDoc);
	return AssignDocument(m_pOwnedDocument.get(), false);
}

bool CFTMPlayer::AssignDocument(CFTMDocument* pDoc, bool bTakeOwnership)
{
	if (!bTakeOwnership) {
		m_pOwnedDocument.reset();
	}
	m_pDocument = pDoc;
	if (!m_pDocument) return false;

	m_iMachine = m_pDocument->GetMachine();
	m_iSpeedSplitPoint = m_pDocument->GetSpeedSplitPoint();

	SetupSound(m_iSampleRate, m_iMachine);
	return SelectSubtune(0);
}

int CFTMPlayer::GetSubtuneCount() const
{
	return m_pDocument ? m_pDocument->GetTrackCount() : 0;
}

double CFTMPlayer::GetDuration(int Track) const
{
	return m_pDocument ? m_pDocument->GetStandardLength(Track, 0) : 0.0;
}

bool CFTMPlayer::SetupSound(int SampleRate, machine_t Machine)
{
	m_iSampleRate = SampleRate;
	m_iMachine = Machine;

	m_pAPU = std::make_unique<CAPU>(this);
	if (!m_pAPU->SetupSound(m_iSampleRate, 1, m_iMachine == NTSC ? MACHINE_NTSC : MACHINE_PAL)) {
		return false;
	}

	if (m_pDocument) {
		CAPUConfig config(m_pAPU.get());
		config.SetExternalSound(m_pDocument->GetExpansionChip());

		std::vector<int16_t> offsets(8, 0);
		for (int i = 0; i < 8; ++i)
			offsets[i] = m_pDocument->GetLevelOffset(i);

		bool useSurvey = m_pDocument->GetSurveyMixCheck();

		std::vector<uint8_t> opllBytes(19 * 8, 0);
		std::vector<std::string> opllNames(19, "");
		if (m_pDocument) {
			for (int i = 0; i < 19; ++i) {
				for (int j = 0; j < 8; ++j)
					opllBytes.at((8 * i) + j) = m_pDocument->GetOPLLPatchByte((8 * i) + j);
				opllNames.at(i) = m_pDocument->GetOPLLPatchName(i);
			}
		}
		config.SetupEmulation(
			true, // Disable multiplexing = true by default in Dn-FamiTracker
			0,
			false,
			opllBytes,
			opllNames
		);

		config.SetupMixer(30, 12000, 24, 100, useSurvey, 2000, 12000, offsets);

		static const int16_t DEFAULT_SURVEY_MIX_LEVELS[CHIP_LEVEL_COUNT] = {
			0,     // APU1
			-20,   // APU2
			0,     // VRC6
			1340,  // VRC7
			690,   // FDS
			0,     // MMC5
			1540,  // N163
			-250   // S5B
		};

		if (useSurvey) {
			for (int i = 0; i < CHIP_LEVEL_COUNT; ++i)
				config.SetChipLevel(static_cast<chip_level_t>(i), static_cast<double>(DEFAULT_SURVEY_MIX_LEVELS[i]) / 100.0, true);
		} else {
			for (int i = 0; i < CHIP_LEVEL_COUNT; ++i)
				config.SetChipLevel(static_cast<chip_level_t>(i), 0.0, false);
		}

		m_pAPU->Write(0x4015, 0x0F);
		m_pAPU->Write(0x4017, 0x00);
		if (m_pDocument->GetExpansionChip() & SNDCHIP_FDS) {
			m_pAPU->Write(0x4023, 0x02);
			m_pAPU->Write(0x4023, 0x83);
		}
		if (m_pDocument->GetExpansionChip() & SNDCHIP_N163)
			m_pAPU->Write(0xE7FF, 0x00);
		if (m_pDocument->GetExpansionChip() & SNDCHIP_MMC5)
			m_pAPU->Write(0x5015, 0x03);
		m_pAPU->ClearSample();

		SetupVibratoTable(m_pDocument->GetVibratoStyle());
		SetupNoteTables();
		InitChannels();
	}

	return true;
}

void CFTMPlayer::SetupVibratoTable(vibrato_t Type)
{
	for (int i = 0; i < 16; ++i) {
		for (int j = 0; j < 16; ++j) {
			int value = 0;
			double angle = (double(j) / 16.0) * (3.14159265358979323846 / 2.0);
			if (Type == VIBRATO_NEW)
				value = int(std::sin(angle) * NEW_VIBRATO_DEPTH[i]);
			else
				value = int((double(j * OLD_VIBRATO_DEPTH[i]) / 16.0) + 1);
			m_iVibratoTable[i * 16 + j] = value;
		}
	}
}

void CFTMPlayer::SetupNoteTables()
{
	if (!m_pDocument) return;

	const double A440_NOTE = 45.0 - m_pDocument->GetTuningSemitone() - m_pDocument->GetTuningCent() / 100.0;

	CDetuneNTSC detuneNTSC(A440_NOTE);
	CDetunePAL detunePAL(A440_NOTE);
	CDetuneSaw detuneSaw(A440_NOTE);
	CDetuneVRC7 detuneVRC7(A440_NOTE);
	CDetuneFDS detuneFDS(A440_NOTE);
	CDetuneN163 detuneN163(A440_NOTE);
	CDetuneS5B detuneS5B(A440_NOTE);

	for (int i = 0; i < NOTE_COUNT; ++i) {
		m_iNoteLookupTableNTSC[i] = std::lround(detuneNTSC.FrequencyToPeriod(detuneNTSC.NoteToFreq(i), 1, 0) - m_pDocument->GetDetuneOffset(0, i));
		m_iNoteLookupTablePAL[i]  = std::lround(detunePAL.FrequencyToPeriod(detunePAL.NoteToFreq(i), 1, 0) - m_pDocument->GetDetuneOffset(1, i));
		m_iNoteLookupTableSaw[i]  = std::lround(detuneSaw.FrequencyToPeriod(detuneSaw.NoteToFreq(i), 1, 0) - m_pDocument->GetDetuneOffset(2, i));

		if (i < NOTE_RANGE) {
			m_iNoteLookupTableVRC7[i] = std::lround(detuneVRC7.FrequencyToPeriod(detuneVRC7.NoteToFreq(i), 1, 0) + m_pDocument->GetDetuneOffset(3, i));
		}

		m_iNoteLookupTableFDS[i]  = std::lround(detuneFDS.FrequencyToPeriod(detuneFDS.NoteToFreq(i), 1, 0) + m_pDocument->GetDetuneOffset(4, i));
		m_iNoteLookupTableN163[i] = std::lround(detuneN163.FrequencyToPeriod(detuneN163.NoteToFreq(i), 1, m_pDocument->GetNamcoChannels()) + m_pDocument->GetDetuneOffset(5, i));
		m_iNoteLookupTableS5B[i]  = std::lround(detuneS5B.FrequencyToPeriod(detuneS5B.NoteToFreq(i), 1, 0));
	}
}

void CFTMPlayer::InitChannels()
{
	for (int i = 0; i < MAX_PLAYER_CHANNELS; ++i) {
		m_pChannels[i].reset();
	}
	m_iActiveChannels = 0;

	if (!m_pDocument || !m_pAPU) return;

	CChannelFactory factory;
	auto AddChan = [&](int chanId, const char* name, unsigned int chip = SNDCHIP_NONE) {
		if (m_iActiveChannels >= MAX_PLAYER_CHANNELS) return;
		int idx = m_iActiveChannels++;
		m_iChannelIDs[idx] = chanId;
		m_iChannelChips[idx] = chip;
		m_strChannelNames[idx] = name;
		m_pChannels[idx].reset(factory.Produce(static_cast<chan_id_t>(chanId)));

		if (m_pChannels[idx]) {
			m_pChannels[idx]->SetChannelID(chanId);
			m_pChannels[idx]->InitChannel(m_pAPU.get(), m_iVibratoTable, this);
			m_pChannels[idx]->SetLinearPitch(m_pDocument->GetLinearPitch());
			m_pChannels[idx]->SetVibratoStyle(m_pDocument->GetVibratoStyle());
			if (auto pN163 = dynamic_cast<CChannelHandlerN163*>(m_pChannels[idx].get())) {
				pN163->SetChannelCount(m_pDocument->GetNamcoChannels());
			}

			// Assign appropriate period/freq lookup table
			const unsigned int* pTable = nullptr;
			switch (chanId) {
			case CHANID_SQUARE1:
			case CHANID_SQUARE2:
			case CHANID_TRIANGLE:
				pTable = (m_iMachine == PAL) ? m_iNoteLookupTablePAL : m_iNoteLookupTableNTSC;
				break;
			case CHANID_VRC6_PULSE1:
			case CHANID_VRC6_PULSE2:
			case CHANID_MMC5_SQUARE1:
			case CHANID_MMC5_SQUARE2:
				pTable = m_iNoteLookupTableNTSC;
				break;
			case CHANID_VRC6_SAWTOOTH:
				pTable = m_iNoteLookupTableSaw;
				break;
			case CHANID_VRC7_CH1: case CHANID_VRC7_CH2: case CHANID_VRC7_CH3:
			case CHANID_VRC7_CH4: case CHANID_VRC7_CH5: case CHANID_VRC7_CH6:
				pTable = m_iNoteLookupTableVRC7;
				break;
			case CHANID_FDS:
				pTable = m_iNoteLookupTableFDS;
				break;
			case CHANID_N163_CH1: case CHANID_N163_CH2: case CHANID_N163_CH3: case CHANID_N163_CH4:
			case CHANID_N163_CH5: case CHANID_N163_CH6: case CHANID_N163_CH7: case CHANID_N163_CH8:
				pTable = m_iNoteLookupTableN163;
				break;
			case CHANID_S5B_CH1: case CHANID_S5B_CH2: case CHANID_S5B_CH3:
				pTable = m_iNoteLookupTableS5B;
				break;
			default:
				pTable = nullptr;
				break;
			}
			m_pChannels[idx]->SetNoteTable(pTable);
		}
	};

	// 1. Standard 2A03 channels
	AddChan(CHANID_SQUARE1,  "Pulse 1", SNDCHIP_NONE);
	AddChan(CHANID_SQUARE2,  "Pulse 2", SNDCHIP_NONE);
	AddChan(CHANID_TRIANGLE, "Triangle", SNDCHIP_NONE);
	AddChan(CHANID_NOISE,    "Noise", SNDCHIP_NONE);
	AddChan(CHANID_DPCM,     "DPCM", SNDCHIP_NONE);

	// 2. Expansion chip channels
	unsigned char chip = m_pDocument->GetExpansionChip();
	if (chip & SNDCHIP_VRC6) {
		AddChan(CHANID_VRC6_PULSE1,   "VRC6 Pulse 1", SNDCHIP_VRC6);
		AddChan(CHANID_VRC6_PULSE2,   "VRC6 Pulse 2", SNDCHIP_VRC6);
		AddChan(CHANID_VRC6_SAWTOOTH, "VRC6 Sawtooth", SNDCHIP_VRC6);
	}
	if (chip & SNDCHIP_VRC7) {
		for (int i = 0; i < 6; ++i) {
			char name[32];
			snprintf(name, sizeof(name), "FM Channel %d", i + 1);
			AddChan(CHANID_VRC7_CH1 + i, name, SNDCHIP_VRC7);
		}
	}
	if (chip & SNDCHIP_FDS) {
		AddChan(CHANID_FDS, "FDS", SNDCHIP_FDS);
	}
	if (chip & SNDCHIP_MMC5) {
		AddChan(CHANID_MMC5_SQUARE1, "MMC5 Pulse 1", SNDCHIP_MMC5);
		AddChan(CHANID_MMC5_SQUARE2, "MMC5 Pulse 2", SNDCHIP_MMC5);
	}
	if (chip & SNDCHIP_N163) {
		int namcoChans = m_pDocument->GetNamcoChannels();
		for (int i = 0; i < namcoChans; ++i) {
			char name[32];
			snprintf(name, sizeof(name), "Namco %d", i + 1);
			AddChan(CHANID_N163_CH1 + i, name, SNDCHIP_N163);
		}
	}
	if (chip & SNDCHIP_S5B) {
		AddChan(CHANID_S5B_CH1, "5B Square 1", SNDCHIP_S5B);
		AddChan(CHANID_S5B_CH2, "5B Square 2", SNDCHIP_S5B);
		AddChan(CHANID_S5B_CH3, "5B Square 3", SNDCHIP_S5B);
	}
}

bool CFTMPlayer::SelectSubtune(int Track)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pDocument || Track < 0 || (unsigned int)Track >= m_pDocument->GetTrackCount())
		return false;

	m_iPlayTrack = Track;
	Reset();

	m_iSpeed = m_pDocument->GetSongSpeed(Track);
	m_iTempo = m_pDocument->GetSongTempo(Track);

	m_iGrooveIndex = -1;
	m_iGroovePosition = 0;
	if (m_pDocument->GetSongGroove(Track)) {
		m_iGrooveIndex = m_iSpeed;
		if (m_iGrooveIndex < MAX_GROOVE && m_pDocument->GetGroove(m_iGrooveIndex) != nullptr) {
			const CGroove* pGroove = m_pDocument->GetGroove(m_iGrooveIndex);
			if (pGroove->GetSize() > 0)
				m_iSpeed = pGroove->GetEntry(0);
			m_iGroovePosition = 1;
		}
	}

	SetupSpeed();

	m_iFrameRate = m_pDocument->GetFrameRate();
	uint32_t baseFreq = (m_iMachine == NTSC) ? CAPU::BASE_FREQ_NTSC : CAPU::BASE_FREQ_PAL;
	m_iUpdateCycles = baseFreq / m_iFrameRate;

	m_bPlaying = true;
	m_bHaltRequest = false;
	m_bDoHalt = false;
	m_bFinished = false;
	m_bLoopReached = false;
	m_bUpdateRow = true;
	m_iPlayFrame = 0;
	m_iPlayRow = 0;
	m_iStepRows = 0;
	m_iRowTickCount = 0;
	m_iTempoAccum = 0;
	m_iTotalTicks = 0;
	m_audioFifoReadPos = 0;
	m_audioFifo.clear();

	memset(m_bVisitedFrames, 0, sizeof(m_bVisitedFrames));

	return true;
}

void CFTMPlayer::Reset()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	m_bPlaying = false;
	m_bPaused = false;
	m_bFinished = false;
	m_bHaltRequest = false;
	m_bDoHalt = false;
	m_bLoopReached = false;
	m_audioFifo.clear();
	m_audioFifoReadPos = 0;

	if (m_pAPU) {
		m_pAPU->Reset();
		m_pAPU->Write(0x4015, 0x0F);
		m_pAPU->Write(0x4017, 0x00);
		if (m_pDocument && (m_pDocument->GetExpansionChip() & SNDCHIP_FDS)) {
			m_pAPU->Write(0x4023, 0x02);
			m_pAPU->Write(0x4023, 0x83);
		}
		if (m_pDocument && (m_pDocument->GetExpansionChip() & SNDCHIP_N163))
			m_pAPU->Write(0xE7FF, 0x00);
		if (m_pDocument && (m_pDocument->GetExpansionChip() & SNDCHIP_MMC5))
			m_pAPU->Write(0x5015, 0x03);
		m_pAPU->ClearSample();
	}

	for (int i = 0; i < m_iActiveChannels; ++i) {
		if (m_pChannels[i])
			m_pChannels[i]->ResetChannel();
	}
}

void CFTMPlayer::SetupSpeed()
{
	if (!m_iSpeed) m_iSpeed = 1;
	if (m_iTempo) {
		m_iTempoDecrement = (m_iTempo * 24) / m_iSpeed;
		m_iTempoRemainder = (m_iTempo * 24) % m_iSpeed;
	} else {
		m_iTempoDecrement = 1;
		m_iTempoRemainder = 0;
	}
}

void CFTMPlayer::EvaluateGlobalEffects(stChanNote *NoteData, int EffColumns)
{
	if (!NoteData) return;

	for (int i = 0; i < EffColumns; ++i) {
		unsigned char effNum = NoteData->EffNumber[i];
		unsigned char effParam = NoteData->EffParam[i];

		switch (effNum) {
		case EF_SPEED:
			if (!effParam) ++effParam;
			if (m_iTempo && effParam >= (unsigned char)m_iSpeedSplitPoint) {
				m_iTempo = effParam;
			} else {
				m_iSpeed = effParam;
				m_iGrooveIndex = -1;
			}
			SetupSpeed();
			break;

		case EF_GROOVE:
			if (m_pDocument && m_pDocument->GetGroove(effParam % MAX_GROOVE) != nullptr) {
				m_iGrooveIndex = effParam % MAX_GROOVE;
				const CGroove* pGroove = m_pDocument->GetGroove(m_iGrooveIndex);
				if (pGroove->GetSize() > 0)
					m_iSpeed = pGroove->GetEntry(0);
				m_iGroovePosition = 1;
				SetupSpeed();
			}
			break;

		case EF_JUMP:
			m_iJumpToPattern = effParam;
			break;

		case EF_SKIP:
			m_iSkipToRow = effParam;
			break;

		case EF_HALT:
			m_bDoHalt = true;
			break;

		default:
			continue;
		}

		NoteData->EffNumber[i] = EF_NONE;
		NoteData->EffParam[i] = 0;
	}
}

void CFTMPlayer::AddCyclesUnlessEndOfFrame(int Count)
{
	if (!m_pAPU) return;
	Count = std::min(Count, (int)(m_iUpdateCycles - m_iConsumedCycles));
	m_iConsumedCycles += Count;
	m_pAPU->AddCycles(Count);
}

void CFTMPlayer::FlushBuffer(int16_t const * pBuffer, uint32_t Size)
{
	if (!pBuffer || Size == 0) return;
	// Compact FIFO if needed
	if (m_audioFifoReadPos > 0 && m_audioFifoReadPos == m_audioFifo.size()) {
		m_audioFifo.clear();
		m_audioFifoReadPos = 0;
	}
	m_audioFifo.reserve(m_audioFifo.size() + Size * 2);
	for (uint32_t i = 0; i < Size; ++i) {
		m_audioFifo.push_back(pBuffer[i]);
		m_audioFifo.push_back(pBuffer[i]);
	}
}

void CFTMPlayer::ReadPatternRow()
{
	if (!m_pDocument) return;

	for (int i = 0; i < m_iActiveChannels; ++i) {
		stChanNote note;
		m_pDocument->GetNoteData(m_iPlayTrack, m_iPlayFrame, i, m_iPlayRow, &note);

		int effCols = m_pDocument->GetEffColumns(m_iPlayTrack, i) + 1;
		if (m_pChannels[i]) {
			if (!m_bChannelMuted[i]) {
				m_pChannels[i]->PlayNote(&note, effCols);
			} else {
				static const int PASS_EFFECTS[] = {
					EF_HALT, EF_JUMP, EF_SPEED, EF_SKIP, EF_GROOVE,
					EF_VRC7_PORT, EF_VRC7_WRITE,
					EF_N163_WAVE_BUFFER,
					EF_SUNSOFT_ENV_HI, EF_SUNSOFT_ENV_LO, EF_SUNSOFT_ENV_TYPE, EF_SUNSOFT_NOISE
				};
				note.Note = HALT;
				note.Octave = 0;
				note.Instrument = 0;

				for (int j = 0; j < effCols; ++j) {
					bool pass = false;
					for (int pass_eff : PASS_EFFECTS) {
						if (note.EffNumber[j] == pass_eff) {
							pass = true;
							break;
						}
					}
					if (!pass) {
						note.EffNumber[j] = EF_NONE;
					}
				}
				m_pChannels[i]->PlayNote(&note, effCols);
			}
		}
	}

	if (m_bDoHalt) {
		m_bHaltRequest = true;
	}
}

void CFTMPlayer::CheckControl()
{
	if (!m_bPlaying) return;

	if (m_bDoHalt) {
		m_bPlaying = false;
		m_bFinished = true;
		return;
	}

	if (m_iJumpToPattern != -1) {
		m_iPlayFrame = m_iJumpToPattern;
		m_iPlayRow = 0;
		if (m_bVisitedFrames[m_iPlayFrame % MAX_FRAMES]) {
			m_bLoopReached = true;
		}
		m_bVisitedFrames[m_iPlayFrame % MAX_FRAMES] = true;
	}
	else if (m_iSkipToRow != -1) {
		PlayerStepFrame();
		m_iPlayRow = m_iSkipToRow;
	}
	else {
		while (m_iStepRows--) {
			PlayerStepRow();
		}
	}

	m_iJumpToPattern = -1;
	m_iSkipToRow = -1;
}

void CFTMPlayer::PlayerStepRow()
{
	if (!m_pDocument) return;

	int patternLen = m_pDocument->GetPatternLength(m_iPlayTrack);
	if (++m_iPlayRow >= patternLen) {
		m_iPlayRow = 0;
		PlayerStepFrame();
	}
}

void CFTMPlayer::PlayerStepFrame()
{
	if (!m_pDocument) return;

	int frameCount = m_pDocument->GetFrameCount(m_iPlayTrack);
	if (m_iPlayFrame < MAX_FRAMES) {
		m_bVisitedFrames[m_iPlayFrame] = true;
	}

	if (++m_iPlayFrame >= frameCount) {
		m_iPlayFrame = 0;
		m_bLoopReached = true;
	}
}

void CFTMPlayer::StepTick()
{
	if (!m_bPlaying || !m_pDocument || !m_pAPU) return;

	++m_iTotalTicks;
	++m_iRowTickCount;
	m_iStepRows = 0;

	// 1. Check if tempo accumulator rolled over -> new pattern row
	if (m_iTempoAccum <= 0) {
		if (m_iGrooveIndex != -1 && m_pDocument->GetGroove(m_iGrooveIndex) != nullptr) {
			const CGroove* pGroove = m_pDocument->GetGroove(m_iGrooveIndex);
			if (pGroove->GetSize() > 0) {
				m_iSpeed = pGroove->GetEntry(m_iGroovePosition % pGroove->GetSize());
				SetupSpeed();
				m_iGroovePosition = (m_iGroovePosition + 1) % pGroove->GetSize();
			}
		}
		m_iStepRows++;
		m_bUpdateRow = true;
		ReadPatternRow();
	} else {
		m_bUpdateRow = false;
	}

	// 2. Control handling and tempo decrement
	if (m_bUpdateRow && !m_bHaltRequest) {
		CheckControl();
	}

	if (m_bPlaying) {
		if (m_iTempoAccum <= 0) {
			int ticksPerSec = m_iFrameRate;
			m_iTempoAccum += (m_iTempo ? 60 * ticksPerSec : m_iSpeed) - m_iTempoRemainder;
		}
		m_iTempoAccum -= m_iTempoDecrement;
	}

	// 3. Process channels
	for (int i = 0; i < m_iActiveChannels; ++i) {
		if (m_pChannels[i]) {
			if (m_bHaltRequest)
				m_pChannels[i]->ResetChannel();
			else
				m_pChannels[i]->ProcessChannel();
		}
	}

	// 4. Update APU registers and clock cycles
	m_iConsumedCycles = 0;
	unsigned int prevChip = SNDCHIP_NONE;
	for (int i = 0; i < m_iActiveChannels; ++i) {
		if (m_pChannels[i]) {
			m_pChannels[i]->RefreshChannel();
			m_pChannels[i]->FinishTick();

			unsigned int chip = m_iChannelChips[i];
			if (chip != SNDCHIP_NONE && m_pDocument->ExpansionEnabled(chip)) {
				int delay = (chip == prevChip) ? 150 : 250;
				AddCyclesUnlessEndOfFrame(delay);
				m_pAPU->Process();
				prevChip = chip;
			}
		}
	}

	if (m_iUpdateCycles > (uint32_t)m_iConsumedCycles) {
		m_pAPU->AddCycles(m_iUpdateCycles - m_iConsumedCycles);
	}
	m_pAPU->Process();

	if (m_bHaltRequest) {
		m_bPlaying = false;
		m_bFinished = true;
	}
}

int CFTMPlayer::Render(float* pOutStereo, int NumFrames)
{
	if (!pOutStereo || NumFrames <= 0) return 0;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_bPaused) {
		std::fill(pOutStereo, pOutStereo + NumFrames * 2, 0.0f);
		return NumFrames;
	}

	size_t samplesNeeded = (size_t)NumFrames * 2;

	while ((m_audioFifo.size() - m_audioFifoReadPos) < samplesNeeded && m_bPlaying) {
		StepTick();
	}

	size_t samplesAvailable = m_audioFifo.size() - m_audioFifoReadPos;
	size_t samplesToCopy = std::min(samplesNeeded, samplesAvailable);

	for (size_t i = 0; i < samplesToCopy; ++i) {
		pOutStereo[i] = static_cast<float>(m_audioFifo[m_audioFifoReadPos + i]) / 32768.0f;
	}
	m_audioFifoReadPos += samplesToCopy;

	// Zero-fill remainder if playback finished early
	if (samplesToCopy < samplesNeeded) {
		std::fill(pOutStereo + samplesToCopy, pOutStereo + samplesNeeded, 0.0f);
	}

	return static_cast<int>(samplesToCopy / 2);
}

int CFTMPlayer::Render(int16_t* pOutStereo, int NumFrames)
{
	if (!pOutStereo || NumFrames <= 0) return 0;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_bPaused) {
		memset(pOutStereo, 0, NumFrames * 2 * sizeof(int16_t));
		return NumFrames;
	}

	size_t samplesNeeded = (size_t)NumFrames * 2;

	while ((m_audioFifo.size() - m_audioFifoReadPos) < samplesNeeded && m_bPlaying) {
		StepTick();
	}

	size_t samplesAvailable = m_audioFifo.size() - m_audioFifoReadPos;
	size_t samplesToCopy = std::min(samplesNeeded, samplesAvailable);

	if (samplesToCopy > 0) {
		memcpy(pOutStereo, &m_audioFifo[m_audioFifoReadPos], samplesToCopy * sizeof(int16_t));
		m_audioFifoReadPos += samplesToCopy;
	}

	if (samplesToCopy < samplesNeeded) {
		memset(pOutStereo + samplesToCopy, 0, (samplesNeeded - samplesToCopy) * sizeof(int16_t));
	}

	return static_cast<int>(samplesToCopy / 2);
}

void CFTMPlayer::Seek(double Seconds)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (Seconds <= 0.0) {
		SelectSubtune(m_iPlayTrack);
		return;
	}

	// Calculate target ticks
	uint64_t targetTicks = static_cast<uint64_t>(Seconds * m_iFrameRate);

	// If seeking backward, reset to beginning first
	if (targetTicks < m_iTotalTicks) {
		SelectSubtune(m_iPlayTrack);
	}

	// Fast-forward ticks
	while (m_iTotalTicks < targetTicks && m_bPlaying) {
		StepTick();
	}

	// Discard any audio produced during seek
	m_audioFifo.clear();
	m_audioFifoReadPos = 0;
}

bool CFTMPlayer::IsPlaying() const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_bPlaying;
}

bool CFTMPlayer::IsFinished() const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_bFinished;
}

bool CFTMPlayer::IsPaused() const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_bPaused;
}

void CFTMPlayer::SetPaused(bool bPaused)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	m_bPaused = bPaused;
}

int CFTMPlayer::GetCurrentFrame() const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_iPlayFrame;
}

int CFTMPlayer::GetCurrentRow() const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_iPlayRow;
}

double CFTMPlayer::GetCurrentTimeSeconds() const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_iFrameRate > 0 ? (double)m_iTotalTicks / (double)m_iFrameRate : 0.0;
}

const char* CFTMPlayer::GetChannelName(int Channel) const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (Channel < 0 || Channel >= m_iActiveChannels) return "";
	return m_strChannelNames[Channel].c_str();
}

void CFTMPlayer::SetChannelMuted(int Channel, bool Muted)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (Channel >= 0 && Channel < m_iActiveChannels) {
		if (Muted && !m_bChannelMuted[Channel] && m_pChannels[Channel]) {
			stChanNote note {};
			note.Note = HALT;
			m_pChannels[Channel]->PlayNote(&note, 0);
		}
		m_bChannelMuted[Channel] = Muted;
	}
}

bool CFTMPlayer::IsChannelMuted(int Channel) const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (Channel >= 0 && Channel < m_iActiveChannels) {
		return m_bChannelMuted[Channel];
	}
	return false;
}

float CFTMPlayer::GetChannelLevel(int Channel) const
{
	if (!m_pAPU || Channel < 0 || Channel >= m_iActiveChannels) return 0.0f;
	int vol = m_pAPU->GetVol(m_iChannelIDs[Channel]);
	return static_cast<float>(vol) / 15.0f;
}

void CFTMPlayer::GetChannelLevels(float* pOutLevels, int MaxChannels) const
{
	if (!pOutLevels || MaxChannels <= 0) return;
	int count = std::min(MaxChannels, m_iActiveChannels);
	for (int i = 0; i < count; ++i) {
		pOutLevels[i] = GetChannelLevel(i);
	}
	for (int i = count; i < MaxChannels; ++i) {
		pOutLevels[i] = 0.0f;
	}
}
