#pragma once

#include "Define.hpp"
#include "Packet.hpp"
#include <iostream>

class ClientInfo
{
public:
	ClientInfo(const unsigned short index_, HANDLE IOCPHandle_) : mClientIndex(index_), mIOCPHandle(IOCPHandle_)
	{
		ZeroMemory(mAcceptBuf, 64);
		ZeroMemory(mRecvBuf, MAX_SOCKBUF);
		ZeroMemory(&mAcceptOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));

		mIsConnected = false;
		mSocket = INVALID_SOCKET;
		mLatestClosedTimeSec = 0;
	}

	unsigned short GetIndex() const { return mClientIndex; }
	SOCKET GetSocket() const { return mSocket; }
	char* RecvBuffer() { return mRecvBuf; }
	bool IsConnected() const { return mIsConnected; }
	unsigned long long GetLatestClosedTimeSec() const { return mLatestClosedTimeSec; }

	void SetUserCode(long code_)
	{
		std::lock_guard<std::mutex> guard(mCodeLock);

		return;
	}

	bool PostAccept(SOCKET listenSock_, const unsigned long long curTimeSec_)
	{
		mLatestClosedTimeSec = curTimeSec_;

		// ���� ����
		mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

		if (mSocket == INVALID_SOCKET)
		{
			return false;
		}

		ZeroMemory(&mAcceptOverlappedEx, sizeof(stOverlappedEx));

		DWORD bytes = 0;
		DWORD flags = 0;
		mAcceptOverlappedEx.m_wsaBuf.len = 0;
		mAcceptOverlappedEx.m_wsaBuf.buf = nullptr;
		mAcceptOverlappedEx.m_eOperation = eIOOperation::ACCEPT;
		mAcceptOverlappedEx.m_userIndex = mClientIndex;

		GetAcceptFunc(mSocket);

		if (myAccept(listenSock_, mSocket, mAcceptBuf, 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptOverlappedEx)) == FALSE)
		{
			int errcode = WSAGetLastError();

			if (errcode != WSA_IO_PENDING) // WSA_IO_PENDING�� ���������� overlapped operation�� ������, ���߿� �Ϸ�
			{
				std::cerr << "[����] AcceptEx : " << errcode << "\n";
				return false;
			}
		}

		setsockopt(mSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&listenSock_), sizeof(listenSock_));

		return true;
	}

	bool AcceptCompletion()
	{
		std::cout << "AcceptCompletion : SessionIndex(" << mClientIndex << ")\n";

		if (!OnConnect(mIOCPHandle, mSocket))
		{
			return false;
		}

		SOCKADDR_IN stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);
		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);

		std::cout << "Connected : IP(" << clientIP << ") SOCKET(" << (int)mSocket << ")\n";

		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		mRecvOverlappedEx.m_userIndex = mClientIndex;
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = eIOOperation::RECV;

		int nRet = WSARecv(mSocket,
			&(mRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cerr << "[����] ClientInfo::BindRecv : WSARecv()�Լ� ����" << WSAGetLastError() << '\n';
			return false;
		}

		return true;
	}

	bool OnConnect(HANDLE IOCPHandle_, SOCKET socket_)
	{
		mSocket = socket_;
		mIsConnected = true;

		if (!BindIOCP(IOCPHandle_))
		{
			return false;
		}
		return BindRecv();
	}
	bool BindIOCP(HANDLE IOCPHandle_)
	{
		auto hIOCP = CreateIoCompletionPort(reinterpret_cast<HANDLE>(mSocket)
			, IOCPHandle_
			, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			std::cerr << "[����] ClientInfo::BindIOCompletionPort : CreateIoCompletionPort()�Լ� ����" << GetLastError() << '\n';
			return false;
		}

		return true;
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER�� ����

		// bIsForce�� true�̸� SO_LINGER, timeout = 0���� �����Ͽ� ���� ���� ��Ų��. ���� : ������ �ս��� ������ ���� 
		if (bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		mIsConnected = false;

		mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		//socketClose������ ������ �ۼ����� ��� �ߴ� ��Ų��.
		shutdown(mSocket, SD_BOTH);

		//���� �ɼ��� �����Ѵ�.
		setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		//���� ������ ���� ��Ų��. 
		closesocket(mSocket);

		mSocket = INVALID_SOCKET;

		return;
	}

	bool SendMsg(SendPacketData* packet_)
	{
		packet_->SetOverlapped();

		std::lock_guard<std::mutex> guard(mSendLock);

		mSendQueue.push(packet_);

		// 1-send
		if (mSendQueue.size() == 1)
		{
			SendIO();
		}

		return true;
	}

	void SendIO()
	{
		SendPacketData* packet = mSendQueue.front();

		DWORD dwRecvNumBytes = 0;

		int nRet = WSASend(mSocket,
			&(packet->sendOverlapped.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED) & (packet->sendOverlapped),
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cerr << "[����] WSASend()�Լ� ���� : " << WSAGetLastError() << "\n";
			return;
		}

		return;
	}

	void SendCompleted()
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		delete mSendQueue.front();
		mSendQueue.pop();

		// �ش� Ŭ���̾�Ʈ�� ���ۿϷ��۾��� �ϴ� �����尡 �̾ ��� �����Ѵ�.
		if (!mSendQueue.empty())
		{
			SendIO();
		}

		return;
	}
private:
	HANDLE mIOCPHandle;
	SOCKET mSocket;
	stOverlappedEx mRecvOverlappedEx;
	stOverlappedEx mAcceptOverlappedEx;
	bool mIsConnected;
	unsigned short mClientIndex;
	unsigned long long mLatestClosedTimeSec;

	char mAcceptBuf[64];
	char mRecvBuf[MAX_SOCKBUF];

	std::mutex mSendLock;
	std::mutex mCodeLock; // for modifying mClientUserID;

	std::queue<SendPacketData*> mSendQueue;

	void GetAcceptFunc(SOCKET& socket_)
	{
		GUID guid = WSAID_ACCEPTEX;
		DWORD dwbyte{ 0 };
		WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&myAccept, sizeof(myAccept),
			&dwbyte, NULL, NULL);
	}
	LPFN_ACCEPTEX myAccept;
};

