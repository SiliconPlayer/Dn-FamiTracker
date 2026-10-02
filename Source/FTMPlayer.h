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

#include "Common.h"
#include "FamiTrackerTypes.h"
#include "SoundGenInterface.h"
#include "FTMDocument.h"
#include "APU/APU.h"
#include "ChannelHandler.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>

/*!
	\brief Headless, deterministic synchronous audio playback engine.
	\details Decoupled from Win32 threads, window messages, and GUI callbacks.
	Generates audio on-demand via synchronous Render calls.
*/
class CFTMPlayer : public ISoundGen, public IAudioCallback {
public:
	CFTMPlayer();
	virtual ~CFTMPlayer();

	// Document Management
	bool LoadDocument(const char* lpszPathName);
	bool LoadDocument(const void* pData, size_t nSize);
	bool AssignDocument(CFTMDocument* pDoc, bool bTakeOwnership = false);
	CFTMDocument* GetDocument() const { return m_pDocument; }

	// Subsong / Track Selection
	int GetSubtuneCount() const;
	bool SelectSubtune(int Track);
	int GetCurrentSubtune() const { return m_iPlayTrack; }
	double GetDuration(int Track) const;

	// Audio Synthesis & Control
	bool SetupSound(int SampleRate = 44100, machine_t Machine = NTSC);
	int Render(float* pOutStereo, int NumFrames);
	int Render(int16_t* pOutStereo, int NumFrames);
	void Seek(double Seconds);
	void Reset();

	// Channel inspection & muting
	int GetChannelCount() const { return m_iActiveChannels; }
	const char* GetChannelName(int Channel) const;
	void SetChannelMuted(int Channel, bool Muted);
	bool IsChannelMuted(int Channel) const;
	float GetChannelLevel(int Channel) const;
	void GetChannelLevels(float* pOutLevels, int MaxChannels) const;

	// Playback State
	bool IsPlaying() const;
	bool IsFinished() const;
	bool IsPaused() const;
	void SetPaused(bool bPaused);
	int GetCurrentFrame() const;
	int GetCurrentRow() const;
	double GetCurrentTimeSeconds() const;

	// ISoundGen implementation
	void EvaluateGlobalEffects(stChanNote *NoteData, int EffColumns) override;
	CFTMComponentInterface *GetDocumentInterface() const override { return m_pDocument; }
	void AddCyclesUnlessEndOfFrame(int Count) override;
	void WriteRegister(uint16_t Reg, uint8_t Value) override {}
	void RegisterKeyState(int Channel, int Note) override {}

	// IAudioCallback implementation
	void FlushBuffer(int16_t const * pBuffer, uint32_t Size) override;

private:
	void InitChannels();
	void SetupNoteTables();
	void SetupVibratoTable(vibrato_t Type);
	void SetupSpeed();
	void ReadPatternRow();
	void CheckControl();
	void PlayerStepRow();
	void PlayerStepFrame();
	void StepTick();

private:
	std::unique_ptr<CFTMDocument> m_pOwnedDocument;
	CFTMDocument* m_pDocument;

	std::unique_ptr<CAPU> m_pAPU;
	int m_iSampleRate;
	machine_t m_iMachine;

	// Channels
	static const int MAX_PLAYER_CHANNELS = 32;
	std::unique_ptr<CChannelHandler> m_pChannels[MAX_PLAYER_CHANNELS];
	int m_iChannelIDs[MAX_PLAYER_CHANNELS];
	unsigned int m_iChannelChips[MAX_PLAYER_CHANNELS];
	std::string m_strChannelNames[MAX_PLAYER_CHANNELS];
	int m_iActiveChannels;
	bool m_bChannelMuted[MAX_PLAYER_CHANNELS];

	// Note and vibrato tables
	int m_iVibratoTable[256];
	unsigned int m_iNoteLookupTableNTSC[NOTE_COUNT];
	unsigned int m_iNoteLookupTablePAL[NOTE_COUNT];
	unsigned int m_iNoteLookupTableSaw[NOTE_COUNT];
	unsigned int m_iNoteLookupTableVRC7[NOTE_COUNT];
	unsigned int m_iNoteLookupTableFDS[NOTE_COUNT];
	unsigned int m_iNoteLookupTableN163[NOTE_COUNT];
	unsigned int m_iNoteLookupTableS5B[NOTE_COUNT];

	// Playback tracking state
	int m_iPlayTrack;
	int m_iPlayFrame;
	int m_iPlayRow;
	int m_iSpeed;
	int m_iTempo;
	int m_iTempoAccum;
	int m_iTempoDecrement;
	int m_iTempoRemainder;
	int m_iGrooveIndex;
	int m_iGroovePosition;
	int m_iSpeedSplitPoint;
	int m_iJumpToPattern;
	int m_iSkipToRow;
	int m_iStepRows;
	int m_iRowTickCount;
	uint32_t m_iUpdateCycles;
	int m_iConsumedCycles;
	int m_iFrameRate;
	uint64_t m_iTotalTicks;

	bool m_bPlaying;
	bool m_bPaused;
	bool m_bHaltRequest;
	bool m_bDoHalt;
	bool m_bUpdateRow;
	bool m_bFinished;
	bool m_bLoopReached;
	bool m_bVisitedFrames[MAX_FRAMES];

	// Sample buffer FIFO
	std::vector<int16_t> m_audioFifo;
	size_t m_audioFifoReadPos;

	mutable std::recursive_mutex m_mutex;
};
