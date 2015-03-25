#ifndef AMSHEADER_H
#define AMSHEADER_H

#include "NetId.h"
#include "wrap_endian.h"

#include "AdsDef.h"
#include <array>
#include <cstddef>

#pragma pack (push, 1)
struct AmsTcpHeader
{
	AmsTcpHeader(const uint8_t *frame)
	{
		memcpy(this, frame, sizeof(*this));
	}

	AmsTcpHeader(const uint32_t numBytes = 0)
		: reserved(0),
		leLength(qToLittleEndian<uint32_t>(numBytes))
	{}

	uint32_t length() const
	{
		return qFromLittleEndian<uint32_t>((const uint8_t*)&leLength);
	}
private:
	uint16_t reserved;
	uint32_t leLength;
};

struct AoERequestHeader
{
	static const uint32_t SDO_UPLOAD = 0xF302;

	AoERequestHeader(uint16_t sdoIndex, uint8_t sdoSubIndex, uint32_t dataLength)
		: AoERequestHeader(SDO_UPLOAD, ((uint32_t)sdoIndex) << 16 | sdoSubIndex, dataLength)
	{}

	AoERequestHeader(uint32_t indexGroup, uint32_t indexOffset, uint32_t dataLength)
		: leGroup(qToLittleEndian<uint32_t>(indexGroup)),
		leOffset(qToLittleEndian<uint32_t>(indexOffset)),
		leLength(qToLittleEndian<uint32_t>(dataLength))
	{}

private:
	uint32_t leGroup;
	uint32_t leOffset;
	uint32_t leLength;
};

struct AdsWriteCtrlRequest
{
	AdsWriteCtrlRequest(uint16_t ads, uint16_t dev, uint32_t dataLength)
		: leAdsState(qToLittleEndian<uint16_t>(ads)),
		leDevState(qToLittleEndian<uint16_t>(dev)),
		leLength(qToLittleEndian<uint32_t>(dataLength))
	{}

private:
	uint16_t leAdsState;
	uint16_t leDevState;
	uint32_t leLength;
};

struct AdsAddDeviceNotificationRequest
{
	AdsAddDeviceNotificationRequest(uint32_t __group, uint32_t __offset, uint32_t __length, uint32_t __mode, uint32_t __maxDelay, uint32_t __cycleTime)
		: leGroup(qToLittleEndian<uint32_t>(__group)),
		leOffset(qToLittleEndian<uint32_t>(__offset)),
		leLength(qToLittleEndian<uint32_t>(__length)),
		leMode(qToLittleEndian<uint32_t>(__mode)),
		leMaxDelay(qToLittleEndian<uint32_t>(__maxDelay)),
		leCycleTime(qToLittleEndian<uint32_t>(__cycleTime)),
		reserved()
	{}

private:
	uint32_t leGroup;
	uint32_t leOffset;
	uint32_t leLength;
	uint32_t leMode;
	uint32_t leMaxDelay;
	uint32_t leCycleTime;
	const std::array<uint8_t, 16> reserved;
};

using AdsDelDeviceNotificationRequest = uint32_t;

struct AoEHeader
{
    static const uint16_t AMS_REQUEST = 0x0004;
    static const uint16_t AMS_RESPONSE = 0x0005;
    static const uint16_t AMS_UDP = 0x0040;
	static const uint16_t INVALID = 0x0000;
    static const uint16_t READ_DEVICE_INFO = 0x0001;
	static const uint16_t READ = 0x0002;
	static const uint16_t WRITE = 0x0003;
	static const uint16_t READ_STATE = 0x0004;
	static const uint16_t WRITE_CONTROL = 0x0005;
	static const uint16_t ADD_DEVICE_NOTIFICATION = 0x0006;
	static const uint16_t DEL_DEVICE_NOTIFICATION = 0x0007;
	static const uint16_t DEVICE_NOTIFICATION = 0x0008;
	static const uint16_t READ_WRITE = 0x0009;


	//TODO fix endianess!!!
    AmsAddr targetAddr;
    AmsAddr sourceAddr;
    const uint16_t cmdId;
    const uint16_t stateFlags;
    const uint32_t length;
    const uint32_t errorCode;
	const uint32_t invokeId;

	AoEHeader()
		: targetAddr({ { 0, 0, 0, 0, 0, 0 }, 0 }),
		sourceAddr({ { 0, 0, 0, 0, 0, 0 }, 0 }),
		cmdId(0),
		stateFlags(0),
		length(0),
		errorCode(0),
		invokeId(0)
	{
	}

	AoEHeader(const AmsAddr &__targetAddr, const AmsAddr &__sourceAddr, uint16_t __cmdId, uint32_t __length, uint32_t __invokeId)
		: targetAddr(__targetAddr),
		sourceAddr(__sourceAddr),
		cmdId(__cmdId),
		stateFlags(AMS_REQUEST),
		length(__length),
		errorCode(0),
		invokeId(__invokeId)
	{
	}

    AoEHeader(const uint8_t *frame)
        : targetAddr(frame),
          sourceAddr(frame + offsetof(AoEHeader, sourceAddr)),
          cmdId     (qFromLittleEndian<uint16_t>(frame + offsetof(AoEHeader, cmdId))),
          stateFlags(qFromLittleEndian<uint16_t>(frame + offsetof(AoEHeader, stateFlags))),
          length    (qFromLittleEndian<uint32_t>(frame + offsetof(AoEHeader, length))),
          errorCode (qFromLittleEndian<uint32_t>(frame + offsetof(AoEHeader, errorCode))),
          invokeId  (qFromLittleEndian<uint32_t>(frame + offsetof(AoEHeader, invokeId)))
    {
		memcpy(&targetAddr, frame, sizeof(targetAddr));
		memcpy(&targetAddr, frame, sizeof(targetAddr));
    }
};

using AoEWriteResponseHeader = uint32_t;

struct AoEResponseHeader
{
	const uint32_t result;

	AoEResponseHeader()
		: result(0)
	{}

	AoEResponseHeader(const uint8_t *frame)
		: result(qFromLittleEndian<uint32_t>(frame))
	{}

};

struct AoEReadResponseHeader : AoEResponseHeader
{
    const uint32_t readLength;

	AoEReadResponseHeader()
		: readLength(0)
	{}

    AoEReadResponseHeader(const uint8_t *frame)
        : AoEResponseHeader(frame),
		readLength(qFromLittleEndian<uint32_t>(frame + sizeof(AoEResponseHeader)))
	{}
};
#pragma pack (pop)
#endif // AMSHEADER_H
