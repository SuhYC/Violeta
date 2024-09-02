#pragma once

#include "Define.hpp"
#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

/*
*/


class SendPacketData
{
public:
	const unsigned short SessionIndex;
	long DataSize;
	char* pPacketData;
	stOverlappedEx sendOverlapped;

	// ���ο� ��Ŷ�� ����.
	SendPacketData(const unsigned short sessionIndex_, long dataSize_, char* pData_) : SessionIndex(sessionIndex_)
	{
		DataSize = dataSize_;

		pPacketData = new char[dataSize_];
		CopyMemory(pPacketData, pData_, dataSize_);
	}

	~SendPacketData()
	{
		delete pPacketData;
	}

	// overlapped ����ü�� �ʱ�ȭ�Ͽ� �ٷ� �۽ſ� ����� �� �ֵ��� ��.
	void SetOverlapped()
	{
		ZeroMemory(&sendOverlapped, sizeof(stOverlappedEx));

		sendOverlapped.m_wsaBuf.len = DataSize;
		sendOverlapped.m_wsaBuf.buf = pPacketData;

		sendOverlapped.m_eOperation = eIOOperation::SEND;
		sendOverlapped.m_userIndex = SessionIndex;
	}
};