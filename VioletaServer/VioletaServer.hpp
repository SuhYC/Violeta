#pragma once

#include "Iocp.hpp"
#include "SHA.hpp"
#include <cstring>
#include <vector>

/*
* 1. 클라이언트와 연결되면 문제를 제시한다.
* 2. 클라이언트로부터 문제에 대한 답이 오면 검증한다.
* 
* 간단하게 이렇게만 하도록 하자.
* 
* 일단 네트워크 메시지에는 중요한 내용을 담지 않도록 한다.
* 
* 
* 검증의 과정은 다음과 같다.
* 
* 1. 서버에서 제약조건을 문제로 제시한다.
* 2. 클라이언트에서 제약조건을 바탕으로 시드값을 정한다.
* 3. 클라이언트에서는 시드값을 바탕으로 유저에게 시뮬레이션을 재생하며 문제를 풀도록 한다.
* 4. 시드값과 유저의 답을 한번의 통신으로 제출한다.
* 5. 서버는 시드값을 바탕으로 검증한다.
* 
* 검증의 원리는 다음과 같다.
* 
* 1. 서버가 현재시간을 기반으로 한 제약조건을 건다. (클라이언트와 서버의 동일시간이 보장되면 굳이 서버가 보내지 않아도 된다.)
*  - 클라이언트는 현재시간 근처의 시드값을 정한다.
*  * 가능하다면 클라이언트와 서버의 시간이 비슷한 것이 좋다. 네트워크시간때문에 정확하지는 않더라도
*  * 통신지연시간 오차를 감안한 범위의 문제를 풀어야한다.
* 2. 정해진 시드값을 SHA-256 해시함수로 가공한다.
*  - 시간을 기반으로 하기 때문에 근접한 시드값을 잡게 될 가능성이 높다.
*  - 미세한 차이로도 큰 변화를 주기 위해 해시함수의 Avalanche Effect를 이용한다.
*  * 이마저도 동일한 매크로 프로그램을 사용한다면 동일한 시드를 사용하려고 할 수도 있다.
*  * 지연시간도 제약조건으로 넣으면 어떨까? (To Do)
*  * 문제의 도착과 동시에 시드값을 정하는 것이 아니고 특정시간이 경과한 후 시드값을 정하도록!
* 3. 가공한 결과를 다시 Sbox 치환을 사용한다.
*  - 만약 클라이언트 코드를 역어셈블리로 분석한다면?
*  * 쉽게 수정할 수 있는 치환코드를 과정에 사용함으로서 어느정도 내성을 챙기자!
*  * Sbox는 일단 AES의 Sbox를 가져왔지만, 필요할 때마다 셔플해서 사용할 수 있다. (단, 클라와 서버의 Sbox는 동일해야한다.)
* 
* 위 과정을 거쳐 네트워크 패킷에는 문제에 대한 정보가 없는 검증을 할 수 있다.
*  * 다만 이것도 클라이언트 코드분석에 대한 완벽한 내성을 갖지는 않는다..
*/

const bool SERVER_DEBUG = true;

const short VIOLETA_EPOCH = 4;

class VioletaServer : public IOCPServer
{
public:
	VioletaServer() {};
	virtual ~VioletaServer() = default;

	void Init(int bindPort_)
	{
		InitSocket(bindPort_);

		return;
	}

	void Run(const unsigned short maxClient_)
	{
		mProcessThread = std::thread([this]() {ProcessSendPacket(); });

		clientQuestion.resize(maxClient_);

		this->mIsRun = true;

		StartServer(maxClient_);

		return;
	}

	void End()
	{
		DestroyThread();

		VioletaServer::mIsRun = false;

		if (mProcessThread.joinable())
		{
			mProcessThread.join();
		}

		return;
	}

private:
	virtual void OnConnect(const unsigned short clientIndex_) override
	{
		if (SERVER_DEBUG)
		{
			std::lock_guard<std::mutex> guard(debugMutex);
			std::cout << "[" << clientIndex_ << "] : Connected\n";
		}

		// 문제 제시
		CreateQuestion(clientIndex_);

		return;
	}

	virtual void OnReceive(const unsigned short clientIndex_, const unsigned long size_, char* pData_)
	{
		// 검증
		std::string str(pData_);

		if (SERVER_DEBUG)
		{
			std::lock_guard<std::mutex> guard(debugMutex);

			std::cout << "[" << clientIndex_ << "] : " << str << "\n";
		}

		if (str.size() < 4)
		{
			std::cerr << "strsizeErr\n";
			return;
		}

		std::vector<short> ans(VIOLETA_EPOCH);

		for (int ansidx = 0; ansidx < VIOLETA_EPOCH; ansidx++)
		{
			ans[ansidx] = str[ansidx] - '0';
		}

		str = str.substr(VIOLETA_EPOCH);
		long long seed = 0;

		try
		{
			seed = std::stoll(str);

			bool bRet = CheckQuestion(seed, ans, clientQuestion[clientIndex_]);

			if (!bRet)
			{
				std::cout << "YOU GOT BUSTED!\n";
			}
			else if(SERVER_DEBUG)
			{
				std::lock_guard<std::mutex> guard(debugMutex);
				std::cout << "[" << clientIndex_ << "]'s Answer is Correct.\n";
			}
		}
		catch (const std::invalid_argument& e)
		{
			if (SERVER_DEBUG)
			{
				std::lock_guard<std::mutex> guard(debugMutex);
				std::cout << "[" << clientIndex_ << "] : Invalid Argument.\n";
			}
		}
		catch (const std::out_of_range& e)
		{
			if (SERVER_DEBUG)
			{
				std::lock_guard<std::mutex> guard(debugMutex);
				std::cout << "[" << clientIndex_ << "] : Out Of Range.\n";
			}
		}

		return;
	}

	virtual void OnClose(const unsigned short clientIndex_)
	{
		ClientInfo* client = GetClientInfo(clientIndex_);

		if (SERVER_DEBUG)
		{
			std::lock_guard<std::mutex> guard(debugMutex);
			std::cout << "[" << clientIndex_ << "] : Close.\n";
		}

		return;
	}

	// 송신패킷 처리.
	void ProcessSendPacket()
	{
		while (mIsRun)
		{
			SendPacketData* packetData = DequePacketData();

			if (packetData != nullptr)
			{
				PushSendPacket(packetData);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	void PushSendPacket(SendPacketData* pData_)
	{
		IOCPServer::SendMsg(pData_);
		return;
	}

	SendPacketData* DequePacketData()
	{
		std::lock_guard<std::mutex> guard(mLock);

		if (SendQueue.empty())
		{
			return nullptr;
		}

		SendPacketData* pkt = SendQueue.front();
		SendQueue.pop();

		return pkt;
	}

	void EnquePacketData(SendPacketData* pkt_)
	{
		std::lock_guard<std::mutex> guard(mLock);

		SendQueue.push(pkt_);

		return;
	}

	void CreateQuestion(const unsigned short sessionIndex_)
	{
		std::string str = "1";

		char* pData = new char[str.size() + 1];
		strcpy_s(pData, str.size() + 1, str.c_str());

		SendPacketData* pkt = new SendPacketData(sessionIndex_, str.size() + 1, pData);

		delete[] pData;

		EnquePacketData(pkt);

		try
		{
			clientQuestion[sessionIndex_] = std::stoi(str);
		}
		catch (const std::invalid_argument& e)
		{
			if (SERVER_DEBUG)
			{
				std::lock_guard<std::mutex> guard(debugMutex);
				std::cout << "CreateQuestion Exception : invalid argu.\n";
			}
		}
		catch (const std::out_of_range& e)
		{
			if (SERVER_DEBUG)
			{
				std::lock_guard<std::mutex> guard(debugMutex);
				std::cout << "CreateQuestion Exception : outofRange.\n";
			}
		}

		return;
	}

	bool CheckQuestion(long long seed_, std::vector<short> ans, short first_idx)
	{
		std::string seedstr = std::to_string(seed_);

		if (SERVER_DEBUG)
		{
			std::lock_guard<std::mutex> guard(debugMutex);

			std::cout << "Seed : " << seed_ << ".\n";
		}

		seedstr = sha256(seedstr);

		if (SERVER_DEBUG)
		{
			std::lock_guard<std::mutex> guard(debugMutex);

			std::cout << "Hashed Seed : " << seedstr << "\n";
		}

		unsigned char cstr[33] = { 0 };

		for (int epoch = 0; epoch * 2 < seedstr.size(); epoch++)
		{
			unsigned char tmpchar = 0x00;

			for (int subidx = 0; subidx < 2; subidx++)
			{
				tmpchar *= 16;

				if (seedstr[epoch * 2 + subidx] < 'A')
				{
					tmpchar += seedstr[epoch * 2 + subidx] - '0';
				}
				else
				{
					tmpchar += seedstr[epoch * 2 + subidx] - 'a' + 10;
				}
			}

			cstr[epoch] = Sbox[tmpchar];
		}

		seedstr = std::string(reinterpret_cast<char*>(cstr));

		std::vector<short> res;
		res.resize(4);

		for (int epoch = 0; epoch < seedstr.size(); epoch += 8)
		{
			for (int lhs = 0; lhs < 3; lhs++)
			{
				for (int rhs = lhs + 1; rhs < 4; rhs++)
				{
					byte lhsVal = (byte)(seedstr[epoch + lhs] ^ seedstr[epoch + lhs + 4]), rhsVal = (byte)(seedstr[epoch + rhs] ^ seedstr[epoch + rhs + 4]);

					int a = ((lhsVal % 16) * 16) + (lhsVal / 16);
					int b = ((rhsVal % 16) * 16) + (rhsVal / 16);

					if (a < b)
					{
						res[rhs]++;
					}
					else
					{
						res[lhs]++;
					}
				}
			}

			if (res[first_idx] != ans[epoch / 8] - 1)
			{
				if (SERVER_DEBUG)
				{
					std::lock_guard<std::mutex> guard(debugMutex);

					std::cout << "Wrong Answer on Epoch " << epoch / 8 << ".\n";
				}

				return false;
			}

			res[0] = res[1] = res[2] = res[3] = 0;
		}

		return true;
	}

	bool mIsRun = false;
	std::thread mProcessThread;
	std::queue<SendPacketData*> SendQueue;
	std::mutex mLock;

	std::vector<short> clientQuestion;
};