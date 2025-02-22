// SPDX-License-Identifier: MIT
/**
    Copyright (C) 2021 Beckhoff Automation GmbH & Co. KG
    Author: Patrick Bruenn <p.bruenn@beckhoff.com>
 */

#include "AdsDevice.h"
#include "AdsFile.h"
#include "AdsLib.h"
#include "LicenseAccess.h"
#include "Log.h"
#include "RouterAccess.h"
#include "RTimeAccess.h"
#include "ParameterList.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <unistd.h>
#include <vector>

static int version()
{
    std::cout << "0.0.8-1\n";
    return 0;
}

[[ noreturn ]] static void usage(const std::string& errorMessage = {})
{
    /*
     * "--help" is the only case we are called with an empty errorMessage. That
     * seems the only case we should really print to stdout instead of stderr.
     */
    errorMessage.empty() ? std::cout : std::cerr << errorMessage <<
        R"(
USAGE:
	[<target[:port]>] [OPTIONS...] <command> [CMD_OPTIONS...] [<command_parameter>...]

	target: AmsNetId, hostname or IP address of your target
	port: AmsPort if omitted the default is command specific

OPTIONS:
	--gw=<hostname> or IP address of your AmsNetId target (mandatory in standalone mode)
	--help Show this message on stdout
	--localams=<netid> Specify your own AmsNetId (by default derived from local IP + ".1.1")
	--log-level=<verbosity> Messages will be shown if their own level is equal or less to verbosity.
		0 verbose | Show all messages, even if they are only useful to developers
		1 info    | (DEFAULT) Show everything, but the verbose stuff
		2 warn    | Don't show informational messages, just warnings and errors
		3 error   | Don't care about warnigs, show errors only
		4 silent  | Stay silent, don't log anything
	--version Show version on stdout

COMMANDS:
	addroute [CMD_OPTIONS...]
		Add an ADS route to a remote TwinCAT system. CMD_OPTIONS are:
		--addr=<hostname> or IP address of the routes destination
		--netid=<AmsNetId> of the routes destination
		--password=<password> for the user on the remote TwinCAT system
		--username=<user> on the remote TwinCAT system (optional, defaults to Administrator)
		--routename=<name> of the new route on the remote TwinCAT system (optional, defaults to --addr)
	examples:
		Use Administrator account to add a route with the same name as destinations address
		$ adstool 192.168.0.231 addroute --addr=192.168.0.1 --netid=192.168.0.1.1.1 --password=1

		Use 'guest' account to add a route with a selfdefined name
		$ adstool 192.168.0.231 addroute --addr=192.168.0.1 --netid=192.168.0.1.1.1 --password=1 --username=guest --routename=Testroute

	file read <path>
		Dump content of the file from <path> to stdout
	examples:
		Make a local backup of explorer.exe:
		$ adstool 5.24.37.144.1.1 file read 'C:\Windows\explorer.exe' > ./explorer.exe

		Show content of a text file:
		$ adstool 5.24.37.144.1.1 file read 'C:\Temp\hello world.txt'
		Hello World!

	file delete <path>
		Delete a file from <path>.
	examples:
		Delete a file over ADS and check if it still exists
		$ adstool 5.24.37.144.1.1 file delete 'C:\Temp\hello world.txt'
		$ adstool 5.24.37.144.1.1 file read 'C:\Temp\hello world.txt'
		$ echo \$?
		1804

	file write [--append] <path>
		Read data from stdin write to the file at <path>.
	examples:
		Write text directly into a file:
		$ printf 'Hello World!' | adstool 5.24.37.144.1.1 file write 'C:\Temp\hello world.txt'

		Copy local file to remote:
		$ adstool 5.24.37.144.1.1 file write 'C:\Windows\explorer.exe' < ./explorer.exe

	license < platformid | systemid | volumeno>
		Read license information from device.
	examples:
		Read platformid from device
		$ adstool 5.24.37.144.1.1 license platformid
		50

		Read systemid from device
		$ adstool 5.24.37.144.1.1 license systemid
		95EEFDE0-0392-1452-275F-1BF9ACCB924E
		50

		Read volume licence number from device
		$ adstool 5.24.37.144.1.1 license volumeno
		123456

	netid
		Read the AmsNetId from a remote TwinCAT router
		$ adstool 192.168.0.231 netid

	pciscan <pci_id>
		Show PCI devices with <pci_id>
	examples:
		List PCI CCAT devices:
		$ adstool 5.24.37.144.1.1 pciscan 0x15EC5000
		PCI devices found: 2
		3:0 @ 0x4028629004
		7:0 @ 0x4026531852

	raw [--read=<number_of_bytes>] <IndexGroup> <IndexOffset>
		This command gives low level access to:
		- AdsSyncReadReqEx2()
		- AdsSyncReadWriteReqEx2()
		- AdsSyncWriteReqEx()
		Read/write binary data at every offset with every length. Data
		to write is provided through stdin. Length of the data to write
		is determined through the number of bytes provided. If --read
		is not provided, the underlying method used will be pure write
		request (AdsSyncWriteReqEx()). If no data is provided on stdin,
		--read is mandatory and a pure read request (AdsSyncReadReqEx2())
		is send. If both, data through stdin and --read, are available,
		a readwrite request will be send (AdsSyncReadWriteReqEx2()).

                Read 10 bytes from TC3 PLC index group 0x4040 offset 0x1 into a file:
		$ adstool 5.24.37.144.1.1:851 raw --read=10 "0x4040" "0x1" > read.bin

		Write data from file to TC3 PLC index group 0x4040 offset 0x1:
		$ adstool 5.24.37.144.1.1 raw "0x4040" "0x1" < read.bin

		Write data from write.bin to TC3 PLC index group 0xF003 offset 0x0
		and read result into read.bin:
		$ adstool 5.24.37.144.1.1 raw --read=4 "0xF003" "0x0" < write.bin > read.bin

	rtime < read-latency | reset-latency >
		Access rtime latency information
	examples:
		Read maximum rtime latency
		$ adstool 5.24.37.144.1.1 rtime read-latency
		6

		Read maximum rtime latency and reset:
		$ adstool 5.24.37.144.1.1 rtime reset-latency
		6
		$ adstool 5.24.37.144.1.1 rtime read-latency
		1

	state [<value>]
		Read or write the ADS state of the device at AmsPort (default 10000).
		ADS states are documented here:
		https://infosys.beckhoff.com/index.php?content=../content/1031/tcadswcf/html/tcadswcf.tcadsservice.enumerations.adsstate.html
	examples:
		Check if TwinCAT is in RUN:
		$ adstool 5.24.37.144.1.1 state
		5

		Set TwinCAT to CONFIG mode:
		$ adstool 5.24.37.144.1.1 state 16

	var [--type=<DATATYPE>] <variable name> [<value>]
		Reads/Write from/to a given PLC variable.
		If value is not set, a read operation will be executed. Otherwise 'value' will
		be written to the variable.

		On read, the content of a given PLC variable is written to stdout. Format of the
		output depends on DATATYPE.

		On write, <value> is written to the given PLC variable in an appropriate manner for
		that datatype. For strings, <value> will be written as-is. For integers
		value will be interpreted as decimal unless it starts with "0x". In that
		case it will be interpreted as hex.
	DATATYPE:
		BOOL -> default output as decimal
		BYTE -> default output as decimal
		WORD -> default output as decimal
		DWORD -> default output as decimal
		LWORD -> default output as decimal
		STRING -> default output as raw bytes
		...
	examples:
		Read number as decimal:
		$ adstool 5.24.37.144.1.1 var --type=DWORD "MAIN.nNum1"
		10

		Read string:
		$ adstool 5.24.37.144.1.1 var --type=STRING "MAIN.sString1"
		Hello World!

		Write a number:
		$ adstool 5.24.37.144.1.1 var --type=DWORD "MAIN.nNum1" "100"

		Write a hexvalue:
		$ adstool 5.24.37.144.1.1 var --type=DWORD "MAIN.nNum1" "0x64"

		Write string:
		$ adstool 5.24.37.144.1.1 var --type=STRING "MAIN.sString1" "Hello World!"
		$ adstool 5.24.37.144.1.1 var --type=STRING "MAIN.sString1"
		Hello World!

		Use quotes to write special characters:
		$ adstool 5.24.37.144.1.1 var "MAIN.sString1" "STRING" "\"Hello World\""
		$ adstool 5.24.37.144.1.1 var "MAIN.sString1" "STRING"
		"Hello World!"
)";
    exit(!errorMessage.empty());
}

typedef int (* CommandFunc)(const AmsNetId, const uint16_t, const std::string&, bhf::Commandline&);
using CommandMap = std::map<const std::string, CommandFunc>;

int RunAddRoute(const IpV4 remote, bhf::Commandline& args)
{
    bhf::ParameterList params = {
        {"--addr"},
        {"--netid"},
        {"--password"},
        {"--username", false, "Administrator"},
        {"--routename"},
    };
    args.Parse(params);

    return bhf::ads::AddRemoteRoute(remote,
                                    make_AmsNetId(params.Get<std::string>("--netid")),
                                    params.Get<std::string>("--addr"),
                                    params.Get<std::string>("--routename"),
                                    params.Get<std::string>("--username"),
                                    params.Get<std::string>("--password")
                                    );
}

int RunFile(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    const auto command = args.Pop<std::string>("file command is missing");
    const auto next = args.Pop<std::string>("path is missing");
    auto device = AdsDevice { gw, netid, port ? port : uint16_t(10000) };

    if (!command.compare("read")) {
        const AdsFile adsFile { device, next,
                                bhf::ads::SYSTEMSERVICE_OPENGENERIC | bhf::ads::FOPEN::READ | bhf::ads::FOPEN::BINARY |
                                bhf::ads::FOPEN::ENSURE_DIR};
        uint32_t bytesRead;
        do {
            char buf[1024];

            adsFile.Read(sizeof(buf), buf, bytesRead);
            std::cout.write(buf, bytesRead);
        } while (bytesRead > 0);
    } else if (!command.compare("write")) {
        bool append = !next.compare("--append");
        const auto flags = (append ? bhf::ads::FOPEN::APPEND : bhf::ads::FOPEN::WRITE) |
                           bhf::ads::FOPEN::BINARY |
                           bhf::ads::FOPEN::PLUS |
                           bhf::ads::FOPEN::ENSURE_DIR
        ;

        const auto path = append ? args.Pop<std::string>("path is missing") : next;
        const AdsFile adsFile { device, path, flags};
        char buf[1024];
        auto length = read(0, buf, sizeof(buf));
        while (length > 0) {
            adsFile.Write(length, buf);
            length = read(0, buf, sizeof(buf));
        }
    } else if (!command.compare("delete")) {
        AdsFile::Delete(device, next, bhf::ads::SYSTEMSERVICE_OPENGENERIC | bhf::ads::FOPEN::ENABLE_DIR);
    } else {
        LOG_ERROR(__FUNCTION__ << "(): Unknown file command '" << command << "'\n");
        return -1;
    }
    return 0;
}

int RunLicense(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    auto device = bhf::ads::LicenseAccess{ gw, netid, port };
    const auto command = args.Pop<std::string>();

    if (!command.compare("platformid")) {
        return device.ShowPlatformId(std::cout);
    } else if (!command.compare("systemid")) {
        return device.ShowSystemId(std::cout);
    } else if (!command.compare("volumeno")) {
        return device.ShowVolumeNo(std::cout);
    } else {
        LOG_ERROR(__FUNCTION__ << "(): Unknown license command '" << command << "'\n");
        return -1;
    }
}

int RunNetId(const IpV4 remote)
{
    AmsNetId netId;
    bhf::ads::GetRemoteAddress(remote, netId);
    std::cout << netId << '\n';
    return 0;
}

int RunPCIScan(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    const auto device = bhf::ads::RouterAccess{ gw, netid, port };
    auto pciId = args.Pop<uint64_t>("pciscan pci_id is missing");

    /* allow subVendorId/SystemId to be omitted from cmd */
    if (std::numeric_limits<uint32_t>::max() >= pciId) {
        pciId <<= 32;
    }
    return device.PciScan(pciId, std::cout);
}

int RunRTime(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    const auto command = args.Pop<std::string>("rtime command is missing");
    auto device = bhf::ads::RTimeAccess{ gw, netid, port };

    if (!command.compare("read-latency")) {
        return device.ShowLatency(RTIME_READ_LATENCY);
    } else if (!command.compare("reset-latency")) {
        return device.ShowLatency(RTIME_RESET_LATENCY);
    } else {
        LOG_ERROR(__FUNCTION__ << "(): Unknown rtime command'" << command << "'\n");
        return -1;
    }
}

int RunRaw(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    bhf::ParameterList params = {
        {"--read"},
    };
    args.Parse(params);

    const auto group = args.Pop<uint32_t>("IndexGroup is missing");
    const auto offset = args.Pop<uint32_t>("IndexOffset is missing");
    const auto readLen = params.Get<uint64_t>("--read");

    LOG_VERBOSE("read: >" << readLen << "< group: >" << std::hex << group << "<offset:>" << offset << "<");

    std::vector<uint8_t> readBuffer(readLen);
    std::vector<uint8_t> writeBuffer;

    if (!isatty(fileno(stdin))) {
        char next_byte;
        while (std::cin.read(&next_byte, 1)) {
            writeBuffer.push_back(next_byte);
        }
    }

    if (!readBuffer.size() && !writeBuffer.size()) {
        LOG_ERROR("write- and read-size is zero!\n");
        return -1;
    }

    auto device = AdsDevice { gw, netid, port ? port : uint16_t(AMSPORT_R0_PLC_TC3) };
    long status = -1;
    uint32_t bytesRead = 0;
    if (!writeBuffer.size()) {
        status = device.ReadReqEx2(group,
                                   offset,
                                   readBuffer.size(),
                                   readBuffer.data(),
                                   &bytesRead);
    } else if (!readBuffer.size()) {
        status = device.WriteReqEx(group,
                                   offset,
                                   writeBuffer.size(),
                                   writeBuffer.data());
    } else {
        status = device.ReadWriteReqEx2(group,
                                        offset,
                                        readBuffer.size(),
                                        readBuffer.data(),
                                        writeBuffer.size(),
                                        writeBuffer.data(),
                                        &bytesRead);
    }

    if (ADSERR_NOERR != status) {
        LOG_ERROR(__FUNCTION__ << "(): failed with: 0x" << std::hex << status << '\n');
        return status;
    }
    std::cout.write((const char*)readBuffer.data(), readBuffer.size());
    return !std::cout.good();
}

int RunState(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    auto device = AdsDevice { gw, netid, port ? port : uint16_t(10000) };
    const auto oldState = device.GetState();
    const auto value = args.Pop<const char*>();
    if (value) {
        const auto requestedState = std::stoi(value);
        if (requestedState >= ADSSTATE::ADSSTATE_MAXSTATES) {
            LOG_ERROR(
                "Requested state '" << std::dec << requestedState << "' exceeds max (" <<
                    uint16_t(ADSSTATE::ADSSTATE_MAXSTATES) <<
                    ")\n");
            return ADSERR_CLIENT_INVALIDPARM;
        }
        try {
            device.SetState(static_cast<ADSSTATE>(requestedState), oldState.device);
        } catch (const AdsException& ex) {
            // ignore AdsError 1861 after RUN/CONFIG mode change
            if (ex.errorCode != 1861) {
                throw;
            }
        }
    } else {
        std::cout << std::dec << (int)oldState.ads << '\n';
    }
    return 0;
}

int RunVar(const AmsNetId netid, const uint16_t port, const std::string& gw, bhf::Commandline& args)
{
    bhf::ParameterList params = {
        {"--type"},
    };
    args.Parse(params);

    const auto name = args.Pop<std::string>("Variable name is missing");
    const auto value = args.Pop<const char*>();
    static const std::map<const std::string, size_t> typeMap = {
        {"BOOL", 1},
        {"BYTE", 1},
        {"WORD", 2},
        {"DWORD", 4},
        {"LWORD", 8},
        {"STRING", 255},
    };
    const auto type = params.Get<std::string>("--type");
    const auto it = typeMap.find(type);
    if (typeMap.end() == it) {
        LOG_ERROR(__FUNCTION__ << "(): Unknown TwinCAT type '" << type << "'\n");
        return -1;
    }
    const auto size = it->second;

    auto device = AdsDevice { gw, netid, port ? port : uint16_t(AMSPORT_R0_PLC_TC3) };
    const auto handle = device.GetHandle(name);

    if (!value) {
        std::vector<uint8_t> readBuffer(size);
        uint32_t bytesRead = 0;
        const auto status = device.ReadReqEx2(ADSIGRP_SYM_VALBYHND,
                                              *handle,
                                              readBuffer.size(),
                                              readBuffer.data(),
                                              &bytesRead);
        if (ADSERR_NOERR != status) {
            LOG_ERROR(__FUNCTION__ << "(): failed with: 0x" << std::hex << status << '\n');
            return status;
        }

        switch (bytesRead) {
        case sizeof(uint8_t):
            {
                const auto value = *(reinterpret_cast<uint8_t*>(readBuffer.data()));
                std::cout << std::dec << (int)value << '\n';
                return !std::cout.good();
            }

        case sizeof(uint16_t):
            {
                const auto value = *(reinterpret_cast<uint16_t*>(readBuffer.data()));
                std::cout << std::dec << bhf::ads::letoh(value) << '\n';
                return !std::cout.good();
            }

        case sizeof(uint32_t):
            {
                const auto value = *(reinterpret_cast<uint32_t*>(readBuffer.data()));
                std::cout << std::dec << bhf::ads::letoh(value) << '\n';
                return !std::cout.good();
            }

        case sizeof(uint64_t):
            {
                const auto value = *(reinterpret_cast<uint64_t*>(readBuffer.data()));
                std::cout << std::dec << bhf::ads::letoh(value) << '\n';
                return !std::cout.good();
            }
        }

        std::cout.write((const char*)readBuffer.data(), bytesRead);
        return !std::cout.good();
    }

    LOG_VERBOSE("name>" << name << "< value>" << value << "<\n");
    LOG_VERBOSE("size>" << size << "< value>" << value << "<\n");

    switch (size) {
    case sizeof(uint8_t):
        {
            const auto writeBuffer = bhf::StringTo<uint8_t>(value);
            LOG_VERBOSE("name>" << name << "< value>0x" << std::hex << (uint32_t)writeBuffer << "<\n");
            const auto status = device.WriteReqEx(ADSIGRP_SYM_VALBYHND,
                                                  *handle,
                                                  sizeof(writeBuffer),
                                                  &writeBuffer);
            return status;
        }

    case sizeof(uint16_t):
        {
            const auto writeBuffer = bhf::StringTo<uint16_t>(value);
            LOG_VERBOSE("name>" << name << "< value>0x" << std::hex << (uint16_t)writeBuffer << "<\n");
            const auto status = device.WriteReqEx(ADSIGRP_SYM_VALBYHND,
                                                  *handle,
                                                  sizeof(writeBuffer),
                                                  &writeBuffer);
            return status;
        }

    case sizeof(uint32_t):
        {
            const auto writeBuffer = bhf::StringTo<uint32_t>(value);
            LOG_VERBOSE("name>" << name << "< value>0x" << std::hex << (uint32_t)writeBuffer << "<\n");
            const auto status = device.WriteReqEx(ADSIGRP_SYM_VALBYHND,
                                                  *handle,
                                                  sizeof(writeBuffer),
                                                  &writeBuffer);
            return status;
        }

    case sizeof(uint64_t):
        {
            const auto writeBuffer = bhf::StringTo<uint32_t>(value);
            LOG_VERBOSE("name>" << name << "< value>0x" << std::hex << (uint64_t)writeBuffer << "<\n");
            const auto status = device.WriteReqEx(ADSIGRP_SYM_VALBYHND,
                                                  *handle,
                                                  sizeof(writeBuffer),
                                                  &writeBuffer);
            return status;
        }

    default:
        {
            auto writeBuffer = std::vector<char>(size);
            strncpy(writeBuffer.data(), value, writeBuffer.size());
            return device.WriteReqEx(ADSIGRP_SYM_VALBYHND,
                                     *handle,
                                     writeBuffer.size(),
                                     writeBuffer.data());
        }
    }
}

template<typename T>
static T try_stoi(const char* str, const T defaultValue = 0)
{
    try {
        if (str && *str) {
            return static_cast<T>(std::stoi(++str));
        }
    } catch (...) {}
    return defaultValue;
}

int ParseCommand(int argc, const char* argv[])
{
    auto args = bhf::Commandline {usage, argc, argv};

    // drop argv[0] program name
    args.Pop<const char*>();
    const auto str = args.Pop<const char*>("Target is missing");
    if (!strcmp("--help", str)) {
        usage();
    } else if (!strcmp("--version", str)) {
        return version();
    }
    const auto split = std::strcspn(str, ":");
    const auto netId = std::string {str, split};
    const auto port = try_stoi<uint16_t>(str + split);
    LOG_VERBOSE("NetId>" << netId << "< port>" << port << "<\n");

    bhf::ParameterList global = {
        {"--gw"},
        {"--localams"},
        {"--log-level"},
    };
    args.Parse(global);
    const auto localNetId = global.Get<std::string>("--localams");
    if (!localNetId.empty()) {
        bhf::ads::SetLocalAddress(make_AmsNetId(localNetId));
    }

    const auto logLevel = global.Get<size_t>("--log-level");
    if (!localNetId.empty()) {
        // highest loglevel is error==3, we allow 4 to disable all messages
        Logger::logLevel = std::max(logLevel, (size_t)4);
    }

    const auto cmd = args.Pop<const char*>("Command is missing");
    if (!strcmp("addroute", cmd)) {
        return RunAddRoute(netId, args);
    } else if (!strcmp("netid", cmd)) {
        return RunNetId(netId);
    }

    const auto commands = CommandMap {
        {"file", RunFile},
        {"license", RunLicense},
        {"pciscan", RunPCIScan},
        {"raw", RunRaw},
        {"rtime", RunRTime},
        {"state", RunState},
        {"var", RunVar},
    };
    const auto it = commands.find(cmd);
    if (it != commands.end()) {
        return it->second(make_AmsNetId(netId), port, global.Get<std::string>("--gw"), args);
    }
    usage(std::string {"Unknown command >"} + cmd + "<\n");
}

int main(int argc, const char* argv[])
{
    try {
        return ParseCommand(argc, argv);
    } catch (const AdsException& ex) {
        LOG_ERROR("AdsException message: " << ex.what() << '\n');
        return ex.errorCode;
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception: " << ex.what() << '\n');
        return -2;
    } catch (...) {
        LOG_ERROR("Unknown exception\n");
        return -1;
    }
}
