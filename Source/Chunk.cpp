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

#include <map>
#include "Common.h"
#include "Chunk.h"
#include "ChunkRenderText.h"

#if !defined(_AFX) && !defined(BUILD_GUI)
const char CChunkRenderText::LABEL_SONG_LIST[]			= "ft_song_list";
const char CChunkRenderText::LABEL_INSTRUMENT_LIST[]	= "ft_instrument_list";
const char CChunkRenderText::LABEL_SAMPLES_LIST[]		= "ft_sample_list";
const char CChunkRenderText::LABEL_SAMPLES[]			= "ft_samples";
const char CChunkRenderText::LABEL_GROOVE_LIST[]		= "ft_groove_list";
const char CChunkRenderText::LABEL_GROOVE[]				= "ft_groove_%i";
const char CChunkRenderText::LABEL_WAVETABLE[]			= "ft_wave_table";
const char CChunkRenderText::LABEL_SAMPLE[]				= "ft_sample_%i";
const char CChunkRenderText::LABEL_WAVES[]				= "ft_waves_%i";
const char CChunkRenderText::LABEL_SEQ_2A03[]			= "ft_seq_2a03_%i";
const char CChunkRenderText::LABEL_SEQ_VRC6[]			= "ft_seq_vrc6_%i";
const char CChunkRenderText::LABEL_SEQ_FDS[]			= "ft_seq_fds_%i";
const char CChunkRenderText::LABEL_SEQ_N163[]			= "ft_seq_n163_%i";
const char CChunkRenderText::LABEL_SEQ_S5B[]			= "ft_seq_s5b_%i";
const char CChunkRenderText::LABEL_INSTRUMENT[]			= "ft_inst_%i";
const char CChunkRenderText::LABEL_SONG[]				= "ft_song_%i";
const char CChunkRenderText::LABEL_SONG_FRAMES[]		= "ft_s%i_frames";
const char CChunkRenderText::LABEL_SONG_FRAME[]			= "ft_s%if%i";
const char CChunkRenderText::LABEL_PATTERN[]			= "ft_s%ip%ic%i";
#endif

/**
 * CChunk - Stores NSF data
 *
 */

CChunk::CChunk(chunk_type_t Type, CStringA label) : m_iType(Type), m_strLabel(label), m_iBank(0)
{
}

CChunk::~CChunk()
{
	Clear();
}

void CChunk::Clear()
{
	for (auto x : m_vChunkData)
		delete x;

	m_vChunkData.clear();
}

chunk_type_t CChunk::GetType() const
{
	return m_iType;
}

LPCSTR CChunk::GetLabel() const
{
	return m_strLabel;
}

void CChunk::SetBank(unsigned char Bank)
{
	m_iBank = Bank;
}

unsigned char CChunk::GetBank() const
{
	return m_iBank;
}

int CChunk::GetLength() const
{
	// Return number of data items in the collection
	return static_cast<int>(m_vChunkData.size());
}

unsigned short CChunk::GetData(int index) const
{
	return m_vChunkData[index]->GetData();
}

unsigned short CChunk::GetDataSize(int index) const
{
	return m_vChunkData[index]->GetSize();
}

void CChunk::StoreByte(unsigned char data)
{
	m_vChunkData.push_back(new CChunkDataByte(data));
}

void CChunk::StoreWord(unsigned short data)
{
	m_vChunkData.push_back(new CChunkDataWord(data));
}

void CChunk::StoreReference(CStringA refName)
{
	m_vChunkData.push_back(new CChunkDataReference(refName));
}

void CChunk::StoreBankReference(CStringA refName, int bank)
{
	m_vChunkData.push_back(new CChunkDataBank(refName, bank));
}

void CChunk::StoreString(const std::vector<char> &data)
{
	m_vChunkData.push_back(new CChunkDataString(data));
}

void CChunk::ChangeByte(int index, unsigned char data)
{
	ASSERT(index < (int)m_vChunkData.size());
	static_cast<CChunkDataByte*>(m_vChunkData[index])->m_data = data;
}

void CChunk::SetupBankData(int index, unsigned char bank)
{
	ASSERT(index < (int)m_vChunkData.size());
	static_cast<CChunkDataBank*>(m_vChunkData[index])->m_bank = bank;
}

unsigned char CChunk::GetStringData(int index, int pos) const
{
	return static_cast<CChunkDataString*>(m_vChunkData[index])->m_vData[pos];
}

const std::vector<char> &CChunk::GetStringData(int index) const
{
	return (static_cast<CChunkDataString*>(m_vChunkData[index]))->m_vData;
}

LPCSTR CChunk::GetDataRefName(int index) const
{	
	CChunkDataReference *pChunkData = dynamic_cast<CChunkDataReference*>(m_vChunkData[index]);

	if (pChunkData != NULL)
		return pChunkData->m_refName;

	return "";
}

void CChunk::UpdateDataRefName(int index, CStringA &name)
{
	CChunkDataReference *pChunkData = dynamic_cast<CChunkDataReference*>(m_vChunkData[index]);

	if (pChunkData != NULL)
		pChunkData->m_refName = name;
}

bool CChunk::IsDataReference(int index) const 
{
	return dynamic_cast<CChunkDataReference*>(m_vChunkData[index]) != NULL;
}

bool CChunk::IsDataBank(int index) const
{
	return dynamic_cast<CChunkDataBank*>(m_vChunkData[index]) != NULL;
}

unsigned int CChunk::CountDataSize() const
{
	// Count sizes of all data items
	int Size = 0;

	for (const auto pChunk : m_vChunkData)
		Size += pChunk->GetSize();

	return Size;
}

void CChunk::AssignLabels(CMap<CStringA, LPCSTR, int, int> &labelMap)
{
	for (auto x : m_vChunkData) if (auto pChunkData = dynamic_cast<CChunkDataReference*>(x))
		pChunkData->ref = labelMap[pChunkData->m_refName];
}
