#include "stdafx.h"
#include "SrProtocol.h"
#include <format>
#include <random>
#include <cstdint>
#include "util.h"

constexpr size_t SR_BUFFER_LENGTH = 1026;
constexpr size_t SR_DATA_LENGTH = 1024;
constexpr uint8_t SR_SEQ_SIZE = 16;
constexpr uint8_t SR_SEND_WINDOW_SIZE = 8;
constexpr uint8_t SR_RECEIVE_WINDOW_SIZE = 8;
constexpr uint32_t SR_MAX_END_ATTEMPT = 5;
constexpr uint16_t SR_MAX_WAIT_COUNT = 20;

enum class SrStage {
	CHECK_STATUS,
	WAIT_FOR_RESPONSE,
	DATA_TRANSMISSION,
	END_TRANSMISSION,
	CLOSED
};

struct SrStatus {
	uint16_t waitCount[SR_SEQ_SIZE];				// 各个数据包的计时器
	bool ack[SR_SEQ_SIZE], send[SR_SEQ_SIZE];		// 已经确认的序号和发送标志
	uint8_t curSeq;									// 当前的序号
	uint32_t totalSeq;								// 当前窗口的位置
	uint8_t endAttempt;								// 发送结束数据包的次数

	typedef std::function<void(uint8_t seq, uint32_t totalSeq)> SrStatusCallback;

	SrStatus() {
		clear();
	}

	void clear() {
		std::memset(waitCount, 0, sizeof(waitCount));
		std::memset(ack, 0, sizeof(ack));
		std::memset(send, 0, sizeof(send));
		curSeq = 0;
		totalSeq = 0;
		endAttempt = 0;
	}

	void forEachElementInWindow(SrStatusCallback callback) const {
		for (uint8_t i = 0; i < SR_SEND_WINDOW_SIZE; ++i) {
			uint8_t seq = ((uint16_t)i + this->curSeq) % SR_SEQ_SIZE;
			uint32_t totalSeq = this->totalSeq + i;
			callback(seq, totalSeq);
		}
	}

	bool hasData(const std::string& data) {
		return hasData(data, totalSeq);
	}

	bool hasData(const std::string& data, uint32_t totalSeq) {
		size_t offset = totalSeq * SR_DATA_LENGTH;
		if (offset >= data.size()) {
			return false;
		}
		return true;
	}

	// 滑动窗口
	bool moveWindow() {
		uint8_t i = 0;
		for (; i < SR_SEND_WINDOW_SIZE; ++i) {
			uint8_t seq = ((uint16_t)i + this->curSeq) % SR_SEQ_SIZE;
			uint32_t totalSeq = this->totalSeq + i;
			if (ack[seq]) {
				ack[seq] = false;
				waitCount[seq] = 0;
			} else {
				break;
			}
		}

		this->curSeq = ((uint16_t)i + this->curSeq) % SR_SEQ_SIZE;
		this->totalSeq = this->totalSeq + i;
		return i > 0;
	}
};

struct SrReceiveStatus {
	uint8_t seq;									// 当前滑动窗口位置
	bool received[SR_SEQ_SIZE];						// 标记是否收到了数据包
	std::string receivedString[SR_SEQ_SIZE];		// 收到的数据内容
	uint32_t totalSeq;								// 已经接收的数据包数量

	SrReceiveStatus() {
		clear();
	}

	void clear() {
		seq = 0;
		std::memset(received, 0, sizeof(received));
		totalSeq = 0;
	}

	bool isWithinWindow(uint8_t seq) {
		uint16_t start = this->seq, end = (uint16_t)this->seq + SR_RECEIVE_WINDOW_SIZE;
		return (seq >= start && seq < end) || (seq < start && (uint16_t)seq + SR_SEQ_SIZE < end);
	}

	bool accept(std::string& result, uint8_t seq, uint8_t* buffer, int length) {
		received[seq] = true;
		receivedString[seq] = std::string((char*)buffer, length);

		// 合并结果，并且滑动窗口位置
		uint8_t i = 0;
		for (; i < SR_RECEIVE_WINDOW_SIZE; ++i) {
			uint8_t seq = ((uint16_t)i + this->seq) % SR_SEQ_SIZE;
			uint32_t totalSeq = this->totalSeq + i;

			if (received[seq]) {
				received[seq] = false;
				result += this->receivedString[seq];
			} else {
				break;
			}
		}

		this->seq = ((uint16_t)i + this->seq) % SR_SEQ_SIZE;
		this->totalSeq = this->totalSeq + i;
		return i > 0;
	}
};

namespace {
	size_t GetDataBuffer(uint8_t* buffer, const std::string& data, uint32_t totalSeq) {
		size_t offset = totalSeq * SR_DATA_LENGTH;
		if (offset >= data.size()) {
			return 0;
		}
		std::string send = data.substr(offset, SR_DATA_LENGTH);
		std::memcpy(buffer, send.c_str(), send.size() * sizeof(uint8_t));
		return send.size();
	}
}

SrProtocol::SrProtocol(WSAConnection wsaConnection) : UdpReliableProtocol(wsaConnection) {}

void SrProtocol::response(const Socket& socket, const Socket::Address& target, const std::string& data) {
	SrStage stage = SrStage::CHECK_STATUS;
	SrStatus status;
	std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(SR_BUFFER_LENGTH);
	int res = 0;

	while (stage != SrStage::CLOSED) {
		switch (stage) {
		case SrStage::CHECK_STATUS:
			buffer[0] = 205;
			socket.send(buffer.get(), 1, target);
			logger("[Server] Sent handshake request");
			stage = SrStage::WAIT_FOR_RESPONSE;
			break;
		case SrStage::WAIT_FOR_RESPONSE:
			res = socket.receive(buffer.get(), SR_BUFFER_LENGTH);
			if (res <= 0) {
				++status.waitCount[0];
				if (status.waitCount[0] >= SR_MAX_WAIT_COUNT) {
					stage = SrStage::CLOSED;
					logger("[Server] Timeout error");
				}
				util::sleep(500);
			} else {
				if (buffer[0] == 200) {
					status.clear();
					stage = SrStage::DATA_TRANSMISSION;
					logger("[Server] Begin file transmission");
				}
			}
			break;
		case SrStage::DATA_TRANSMISSION:

			// 检查窗口内的数据包，对于发送但超时的重新发送
			status.forEachElementInWindow([&](uint8_t seq, uint32_t totalSeq) {
				if (!status.hasData(data, totalSeq)) {
					return;
				}
				if (!status.ack[seq]) {
					++status.waitCount[seq];
					if (status.waitCount[seq] >= SR_MAX_WAIT_COUNT) {
						logger(std::format("[Server] Data seq {} timeout, reset package", seq));
						status.waitCount[seq] = 0;
						status.send[seq] = false;
					}
				}
			});

			// 发送当前窗口内还没有发送过的数据包
			status.forEachElementInWindow([&](uint8_t seq, uint32_t totalSeq) {
				if (!status.send[seq]) {
					size_t size = GetDataBuffer(&buffer[1], data, totalSeq);
					if (size > 0) {
						status.send[seq] = true;
						status.waitCount[seq] = 0;
						buffer[0] = seq;
						socket.send(buffer.get(), (int)size + 1, target);
						logger(std::format("[Server] Sent data package seq {}", seq));
					}
				}
			});

			// 接收 ACK 并标记数据
			while ((res = socket.receive(buffer.get(), 1)) > 0) {
				uint8_t ack = buffer[0];
				status.ack[ack] = true;
				logger(std::format("[Server] Received ack {}", ack));
			}

			// 滑动窗口
			if (status.moveWindow()) {
				logger(std::format("[Server] Moved send window, current seq is {}", status.curSeq));
			}

			if (!status.hasData(data)) {
				stage = SrStage::END_TRANSMISSION;
				status.endAttempt = 0;
				std::memset(buffer.get(), 0, SR_BUFFER_LENGTH);
				logger("[Server] Transmission success, attempt to end connection");
			}

			util::sleep(500);

			break;
		case SrStage::END_TRANSMISSION:
			if (status.endAttempt >= SR_MAX_END_ATTEMPT) {
				stage = SrStage::CLOSED;
				break;
			}
			++status.endAttempt;
			buffer[0] = status.curSeq + 1;
			socket.send(buffer.get(), 1, target);
			logger(std::format("[Server] Sent end request #{}, {} remaining", status.endAttempt, SR_MAX_END_ATTEMPT - status.endAttempt));

			for (int i = 0; i < SR_MAX_WAIT_COUNT; ++i) {
				res = socket.receive(buffer.get(), SR_BUFFER_LENGTH);
				if (res > 0 && buffer[0] == status.curSeq + 1) {
					logger("[Server] Received end ack, closing connection");
					stage = SrStage::CLOSED;
					break;
				}
				util::sleep(500);
			}

			util::sleep(500);
			break;
		}
	}
	logger("[Server] Test SR protocol end");
}

std::string SrProtocol::receive(const std::string& host, unsigned short port, double loss, double ackLoss) {
	Socket socket(wsaConnection);
	socket.init(Socket::ProtocolType::UDP);
	Socket::Address target(host, port);
	std::random_device randomDevice;
	std::default_random_engine engine(randomDevice());
	std::bernoulli_distribution lossRandom(loss), ackLossRandom(ackLoss);
	
	std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(SR_BUFFER_LENGTH);
	std::string result;
	SrReceiveStatus status;
	SrStage stage = SrStage::CHECK_STATUS;
	int res = 0;
	uint8_t seq = 0;

	socket.send("-testsr", target);

	while (stage != SrStage::CLOSED) {
		res = socket.receive(buffer.get(), SR_BUFFER_LENGTH);
		if (res <= 0) {
			continue;
		}

		switch (stage) {
		case SrStage::CHECK_STATUS:
			if (buffer[0] == 205) {
				buffer[0] = 200;
				socket.send(buffer.get(), 1, target);
				logger("[Client] 200 OK, start receiving data");
				stage = SrStage::DATA_TRANSMISSION;
			}
			break;
		case SrStage::DATA_TRANSMISSION:
			seq = buffer[0];
			if (lossRandom(engine)) {
				logger(std::format("[Client] Lost data package seq {}", seq));
				break;
			}
			logger(std::format("[Client] Received data package seq {}", seq));

			// 确认数据在窗口中
			if (status.isWithinWindow(seq)) {
				if (res == 1) {
					logger(std::format("[Client] Accepted end request {}, closing connection", seq));
					stage = SrStage::CLOSED;
				} else if (status.accept(result, seq, &buffer[1], res - 1)) {
					logger(std::format("[Client] Accepted data package {}, current total seq is {}", seq, status.totalSeq));
				} else {
					logger(std::format("[Client] Saved data package {}, current total seq is {}", seq, status.totalSeq));
				}
			} else {
				logger(std::format("[Client] Data package {} is not in receive window and will be ignored", seq));
			}

			// 发送 ACK
			if (ackLossRandom(engine)) {
				logger(std::format("[Client] Lost ack {}", seq));
				break;
			}
			logger(std::format("[Client] Sent ack {} ", seq));
			socket.send(buffer.get(), 1, target);
			break;
		}
	}

	logger("[Client] Connection closed");

	return result;
}
