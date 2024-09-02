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

		// 소켓 생성
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

			if (errcode != WSA_IO_PENDING) // WSA_IO_PENDING은 성공적으로 overlapped operation을 시작함, 나중에 완료
			{
				std::cerr << "[에러] AcceptEx : " << errcode << "\n";
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

		//Overlapped I/O을 위해 각 정보를 셋팅해 준다.
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

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cerr << "[에러] ClientInfo::BindRecv : WSARecv()함수 실패" << WSAGetLastError() << '\n';
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
			std::cerr << "[에러] ClientInfo::BindIOCompletionPort : CreateIoCompletionPort()함수 실패" << GetLastError() << '\n';
			return false;
		}

		return true;
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER로 설정

		// bIsForce가 true이면 SO_LINGER, timeout = 0으로 설정하여 강제 종료 시킨다. 주의 : 데이터 손실이 있을수 있음 
		if (bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		mIsConnected = false;

		mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		//socketClose소켓의 데이터 송수신을 모두 중단 시킨다.
		shutdown(mSocket, SD_BOTH);

		//소켓 옵션을 설정한다.
		setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		//소켓 연결을 종료 시킨다. 
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

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cerr << "[에러] WSASend()함수 실패 : " << WSAGetLastError() << "\n";
			return;
		}

		return;
	}

	void SendCompleted()
	{
		std::lock_guard<std::mutex> guard(mSendLock);

		delete mSendQueue.front();
		mSendQueue.pop();

		// 해당 클라이언트에 전송완료작업을 하던 스레드가 이어서 계속 전송한다.
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

