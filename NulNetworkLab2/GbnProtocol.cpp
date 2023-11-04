#include "stdafx.h"
#include "GbnProtocol.h"
#include <memory>
#include <format>
#include <cstdint>
#include "util.h"
#include <random>

constexpr size_t GBN_BUFFER_LENGTH = 1026;
constexpr size_t GBN_SEQ_SIZE = 20;
constexpr size_t GBN_DATA_LENGTH = 1024;
constexpr size_t GBN_MAX_END_ATTEMPT = 5;
constexpr int SEND_WINDOW_SIZE = 10;

enum class GbnStage {
	CHECK_STATUS,
	WAIT_FOR_RESPONSE,
	DATA_TRANSMISSION,
	CLOSED
};

struct GbnStatus {
	uint8_t curSeq, curAck;			// ��ǰ����ź��Ѿ�ȷ�ϵ����к�
	uint32_t totalSeq, waitCount;	// �Ѿ�������ϵ����к������Լ���ʱʱ��
	bool end;						// �Ƿ��Ѿ�������������е�����
	uint8_t endAttempt;				// ���ͽ������ݰ��Ĵ���
	
	GbnStatus() {
		clear();
	}

	// ��鵱ǰ���ݱ����к��Ƿ����
	bool isSeqAvailable() const {
		// 1. ��鵱ǰ���кź�����ȷ�ϵ����ݰ����к�֮��Ĳ�ֵ
		int step = (int)curSeq - curAck;
		if (step < 0) {
			step += GBN_SEQ_SIZE;
		}

		// 2. ����Ƿ�����˶������
		return step < SEND_WINDOW_SIZE;
	}

	void clear() {
		curSeq = 0;
		curAck = 0;
		totalSeq = 0;
		waitCount = 0;
		end = false;
		endAttempt = 0;
	}
};

GbnProtocol::GbnProtocol(WSAConnection wsaConnection) : UdpReliableProtocol(wsaConnection) {}

void GbnProtocol::response(const Socket& socket, const Socket::Address& target, const std::string& data) {
	GbnStatus status;
	GbnStage stage = GbnStage::CHECK_STATUS;
	Socket::Address sender;
	std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(GBN_BUFFER_LENGTH);
	int res = 0;

	while (stage != GbnStage::CLOSED) {
		switch (stage) {
		case GbnStage::CHECK_STATUS:
			buffer[0] = 205;
			socket.send(buffer.get(), 1, target);
			logger("[Server] Sent handshake request.");
			stage = GbnStage::WAIT_FOR_RESPONSE;
			break;
		case GbnStage::WAIT_FOR_RESPONSE:
			res = socket.receive(buffer.get(), GBN_BUFFER_LENGTH);
			if (res <= 0) {
				++status.waitCount;
				if (status.waitCount > 20) {
					stage = GbnStage::CLOSED;
					logger("[Server] Timeout error.");
				}
				util::sleep(500);
			} else if (buffer[0] == 200) {
				logger("[Server] Begin file transmission.");
				status.clear();
				stage = GbnStage::DATA_TRANSMISSION;
			}
			break;
		case GbnStage::DATA_TRANSMISSION:
			if (status.isSeqAvailable()) {

				// ��ȡ��ǰ��ȡ���ݵ�ƫ��
				size_t offset = GBN_DATA_LENGTH * status.totalSeq;
				
				// ����Ƿ�����ϣ����������ϣ����Ϳ����ݰ�
				if (offset < data.size()) {
					// �� Seq ��ŷ�װ�����ݰ���
					std::memset(buffer.get(), 0, GBN_BUFFER_LENGTH);
					buffer[0] = (uint8_t)++status.curSeq;
					if (status.curSeq > GBN_SEQ_SIZE) {
						status.curSeq -= GBN_SEQ_SIZE;
					}
					std::string send = data.substr(offset, GBN_DATA_LENGTH);
					std::memcpy(&buffer[1], send.c_str(), send.size());
					++status.totalSeq;
					logger(std::format("[Server] Sent data package seq {}", status.curSeq));
					socket.send(buffer.get(), GBN_BUFFER_LENGTH, target);
				} else {
					if (!status.end) {
						std::memset(buffer.get(), 0, GBN_BUFFER_LENGTH);
						status.end = true;
						buffer[0] = ++status.curSeq;
						++status.totalSeq;
						logger(std::format("[Server] Sent end package seq {}", status.curSeq));
						socket.send(buffer.get(), GBN_BUFFER_LENGTH, target);
					}
				}
			}

			// ���տͻ��˷����� Ack ���ݰ�
			res = socket.receive(buffer.get(), GBN_BUFFER_LENGTH, sender);

			if (res <= 0) {
				++status.waitCount;
				if (status.waitCount >= 10) {
					// ��ʱ�����˵��ϸ� Ack ����һ֡
					logger("[Server] Timeout error, go back to last ack");
					int step = (int)status.curSeq - status.curAck;
					if (step < 0) {
						step += GBN_SEQ_SIZE;
					}
					status.curSeq = status.curAck;
					if (status.curSeq > GBN_SEQ_SIZE) {
						status.curSeq -= GBN_SEQ_SIZE;
					}
					status.totalSeq = status.totalSeq - step;
					status.waitCount = 0;
					if (status.end && step == 1) {
						++status.endAttempt;
						if (status.endAttempt > GBN_MAX_END_ATTEMPT) {
							stage = GbnStage::CLOSED;
							logger(std::format("[Server] Attempt failed for {} times, terminating connection...", 
								GBN_MAX_END_ATTEMPT));
							break;
						}
						logger(std::format("[Server] End attempt #{}, {} remaining", status.endAttempt, 
							GBN_MAX_END_ATTEMPT - status.endAttempt));
					}
					status.end = false;
				}
			} else {
				// �����յ��� Ack ��
				uint8_t ack = buffer[0];
				status.curAck = ack;
				status.waitCount = 0;
				logger(std::format("[Server] Received ack {}", ack));
				if (status.curAck == status.curSeq && status.end) {
					stage = GbnStage::CLOSED;
				}
			}

			util::sleep(500);

			break;
		}
	}

	logger("[Server] Test GBN protocol end");
}

std::string GbnProtocol::receive(const std::string& host, unsigned short port, double loss, double ackLoss) {
	Socket socket(wsaConnection);
	socket.init(Socket::ProtocolType::UDP);
	Socket::Address sender, target(host, port);
	std::random_device randomDevice;
	std::default_random_engine engine(randomDevice());
	std::bernoulli_distribution randomLoss(loss), randomAckLoss(ackLoss);
	GbnStage stage = GbnStage::CHECK_STATUS;
	std::string result;
	std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(GBN_BUFFER_LENGTH);
	int res = 0;
	uint8_t seq = 0, ack = 0;

	socket.send("-testgbn", target);

	while (stage != GbnStage::CLOSED) {
		res = socket.receive(buffer.get(), GBN_BUFFER_LENGTH);
		if (res <= 0) {
			continue;
		}

		switch (stage) {
		case GbnStage::CHECK_STATUS:
			if (buffer[0] == 205) {
				buffer[0] = 200;
				socket.send(buffer.get(), 1, target);
				stage = GbnStage::DATA_TRANSMISSION;
			}
			break;
		case GbnStage::DATA_TRANSMISSION:
			seq = buffer[0];
			logger(std::format("[Client] Received data package seq {}", seq));
			if (randomLoss(engine)) {
				logger(std::format("[Client] Lost package {}", seq));
				break;
			}

			// ��������
			if (seq == ack + 1 || (ack == GBN_SEQ_SIZE && seq == 1)) {
				ack = seq;
				std::string received((char*)&buffer[1]);
				result += received;
				logger(std::format("[Client] Accepted package seq {}, length {}", 
					seq, received.size()));

				if (received.empty()) {
					logger("[Client] End file transmission");
					stage = GbnStage::CLOSED;
				}
			}
			if (randomAckLoss(engine)) {
				logger(std::format("[Client] Lost ack {}", ack));
				break;
			}
			std::memset(buffer.get(), 0, GBN_BUFFER_LENGTH);
			buffer[0] = ack;
			socket.send(buffer.get(), GBN_BUFFER_LENGTH, target);
			logger(std::format("[Client] Sent ack {}", ack));
			break;
		}
	}

	return result;
}


