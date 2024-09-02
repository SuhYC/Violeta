#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h> // AcceptEx()
#include <mutex>
#include <vector>
#include <queue>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib") // acceptEx()

const unsigned short MAX_SOCKBUF = 100;
const unsigned short MAX_WORKERTHREAD = 12;

// ������ ����� �� �ٽ� ����� �� ���� �������� �ð� (���� : ��)
const unsigned long long RE_USE_SESSION_WAIT_TIMESEC = 3;

std::mutex debugMutex;

enum class eIOOperation
{
	RECV,
	SEND,
	ACCEPT
};

struct stOverlappedEx
{
	WSAOVERLAPPED m_overlapped;
	unsigned short m_userIndex;
	WSABUF m_wsaBuf;
	eIOOperation m_eOperation;
};