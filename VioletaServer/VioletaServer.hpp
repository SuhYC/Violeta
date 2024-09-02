#pragma once

#include "Iocp.hpp"
#include "SHA.hpp"
#include <cstring>
#include <vector>

/*
* 1. Ŭ���̾�Ʈ�� ����Ǹ� ������ �����Ѵ�.
* 2. Ŭ���̾�Ʈ�κ��� ������ ���� ���� ���� �����Ѵ�.
* 
* �����ϰ� �̷��Ը� �ϵ��� ����.
* 
* �ϴ� ��Ʈ��ũ �޽������� �߿��� ������ ���� �ʵ��� �Ѵ�.
* 
* 
* ������ ������ ������ ����.
* 
* 1. �������� ���������� ������ �����Ѵ�.
* 2. Ŭ���̾�Ʈ���� ���������� �������� �õ尪�� ���Ѵ�.
* 3. Ŭ���̾�Ʈ������ �õ尪�� �������� �������� �ùķ��̼��� ����ϸ� ������ Ǯ���� �Ѵ�.
* 4. �õ尪�� ������ ���� �ѹ��� ������� �����Ѵ�.
* 5. ������ �õ尪�� �������� �����Ѵ�.
* 
* ������ ������ ������ ����.
* 
* 1. ������ ����ð��� ������� �� ���������� �Ǵ�. (Ŭ���̾�Ʈ�� ������ ���Ͻð��� ����Ǹ� ���� ������ ������ �ʾƵ� �ȴ�.)
*  - Ŭ���̾�Ʈ�� ����ð� ��ó�� �õ尪�� ���Ѵ�.
*  * �����ϴٸ� Ŭ���̾�Ʈ�� ������ �ð��� ����� ���� ����. ��Ʈ��ũ�ð������� ��Ȯ������ �ʴ���
*  * ��������ð� ������ ������ ������ ������ Ǯ����Ѵ�.
* 2. ������ �õ尪�� SHA-256 �ؽ��Լ��� �����Ѵ�.
*  - �ð��� ������� �ϱ� ������ ������ �õ尪�� ��� �� ���ɼ��� ����.
*  - �̼��� ���̷ε� ū ��ȭ�� �ֱ� ���� �ؽ��Լ��� Avalanche Effect�� �̿��Ѵ�.
*  * �̸����� ������ ��ũ�� ���α׷��� ����Ѵٸ� ������ �õ带 ����Ϸ��� �� ���� �ִ�.
*  * �����ð��� ������������ ������ ���? (To Do)
*  * ������ ������ ���ÿ� �õ尪�� ���ϴ� ���� �ƴϰ� Ư���ð��� ����� �� �õ尪�� ���ϵ���!
* 3. ������ ����� �ٽ� Sbox ġȯ�� ����Ѵ�.
*  - ���� Ŭ���̾�Ʈ �ڵ带 ��������� �м��Ѵٸ�?
*  * ���� ������ �� �ִ� ġȯ�ڵ带 ������ ��������μ� ������� ������ ì����!
*  * Sbox�� �ϴ� AES�� Sbox�� ����������, �ʿ��� ������ �����ؼ� ����� �� �ִ�. (��, Ŭ��� ������ Sbox�� �����ؾ��Ѵ�.)
* 
* �� ������ ���� ��Ʈ��ũ ��Ŷ���� ������ ���� ������ ���� ������ �� �� �ִ�.
*  * �ٸ� �̰͵� Ŭ���̾�Ʈ �ڵ�м��� ���� �Ϻ��� ������ ������ �ʴ´�..
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

		// ���� ����
		CreateQuestion(clientIndex_);

		return;
	}

	virtual void OnReceive(const unsigned short clientIndex_, const unsigned long size_, char* pData_)
	{
		// ����
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

	// �۽���Ŷ ó��.
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