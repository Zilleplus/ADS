#ifndef _AMS_ROUTER_H_
#define _AMS_ROUTER_H_

#include "AdsDef.h"
#include "AdsConnection.h"
#include "AdsNotification.h"

#include <array>
#include <bitset>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

template <class T> struct AmsResponse
{
	AmsResponse(long __status)
		: status(__status)
	{}

	AmsResponse(T header)
		: status(0),
		response(header)
	{}

	long result() const
	{
		return status ? status : response.result;
	}

private:
	const long status;
	T response;
};

struct AmsRouter : NotificationDispatcher
{
	AmsRouter();

	uint16_t OpenPort();
	long ClosePort(uint16_t port);
	long GetLocalAddress(uint16_t port, AmsAddr* pAddr);
	long GetTimeout(uint16_t port, uint32_t& timeout);
	long SetTimeout(uint16_t port, uint32_t timeout);
	long Read(uint16_t port, const AmsAddr* pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t bufferLength, void* buffer, uint32_t *bytesRead);
	long ReadDeviceInfo(uint16_t port, const AmsAddr* pAddr, char* devName, AdsVersion* version);
	long ReadState(uint16_t port, const AmsAddr* pAddr, uint16_t* adsState, uint16_t* deviceState);
	long Write(uint16_t port, const AmsAddr* pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t bufferLength, const void* buffer);
	long WriteControl(uint16_t port, const AmsAddr* pAddr, uint16_t adsState, uint16_t devState, uint32_t bufferLength, const void* buffer);
	long AddNotification(uint16_t port, const AmsAddr* pAddr, uint32_t indexGroup, uint32_t indexOffset, const AdsNotificationAttrib* pAttrib, PAdsNotificationFuncEx pFunc, uint32_t hUser, uint32_t *pNotification);
	long DelNotification(uint16_t port, const AmsAddr* pAddr, uint32_t hNotification);

	bool AddRoute(AmsNetId ams, const IpV4& ip);
	void DelRoute(const AmsNetId& ams);
	AdsConnection* GetConnection(const AmsNetId& pAddr);

	void Dispatch(Frame &frame, AmsAddr amsAddr) const;

private:
	static const uint32_t DEFAULT_TIMEOUT = 5000;
	static const size_t NUM_PORTS_MAX = 8;
	static const uint16_t PORT_BASE = 30000;
	static_assert(PORT_BASE + NUM_PORTS_MAX <= UINT16_MAX, "Port limit is out of range");

	std::bitset<NUM_PORTS_MAX> ports;
	std::array<uint32_t, NUM_PORTS_MAX> portTimeout;
	const AmsAddr localAddr;
	std::mutex mutex;
	std::map<IpV4, std::unique_ptr<AdsConnection>> connections;
	std::map<AmsNetId, AdsConnection*> mapping;

	std::map<IpV4, std::unique_ptr<AdsConnection>>::iterator __GetConnection(const AmsNetId& pAddr);
	void DeleteIfLastConnection(const AdsConnection* conn);
	void Recv();

	template<class T>
	AmsResponse<T> AdsRequest(Frame& request, const AmsAddr& destAddr, uint16_t port, uint16_t cmdId, uint32_t bufferLength = 0, void* buffer = nullptr, uint32_t *bytesRead = nullptr);

	using NotifyTable = std::map < uint32_t, Notification >;
	using NotifyPair = std::pair < AmsAddr, uint32_t >;
	using TableRef = std::unique_ptr<NotifyTable>;
	std::map<AmsAddr, TableRef> tableMapping;
	std::mutex notificationLock;
	void CreateNotifyMapping(uint16_t port, AmsAddr destAddr, PAdsNotificationFuncEx pFunc, uint32_t hUser, uint32_t length, uint32_t hNotify);
	void DeleteNotifyMapping(const AmsAddr &addr, uint32_t hNotify);
	std::vector<NotifyPair> CollectOrphanedNotifications(uint16_t port);

};
#endif /* #ifndef _AMS_ROUTER_H_ */
