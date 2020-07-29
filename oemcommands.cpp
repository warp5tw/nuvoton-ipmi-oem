/*
// Copyright (c) 2020 Nuvoton Technology Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "xyz/openbmc_project/Common/error.hpp"

#include <systemd/sd-journal.h>


#include <array>
#include <boost/container/flat_map.hpp>
#include <boost/process/child.hpp>
#include <boost/process/io.hpp>
#include <filesystem>
#include <iostream>
#include <ipmid/api.hpp>
#include <ipmid/utils.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>

#include "nuvoton_defs.hpp"
#include "oemcommands.hpp"

uint64_t devmem_read(off_t target, unsigned width, bool &ret)
{
	void *map_base, *virt_addr;
	uint64_t read_result;
	unsigned page_size, mapped_size, offset_in_page;
	int fd;

	ret = true;
	fd = open("/dev/mem", (O_RDONLY | O_SYNC));
	if(-1 == fd)
	{
		ret = false;
		return 0;
	}
	mapped_size = page_size = getpagesize();
	offset_in_page = (unsigned)target & (page_size - 1);
	if (offset_in_page + width > page_size) {
		mapped_size *= 2;
	}
	map_base = mmap(NULL, mapped_size, PROT_READ, MAP_SHARED, fd, target & ~(off_t)(page_size - 1));
	if (MAP_FAILED == map_base)
	{
		ret = false;
		close(fd);
		return 0;
	}

	virt_addr = (char*)map_base + offset_in_page;

	switch (width) {
		case 8:
		  read_result = *(volatile uint8_t*)virt_addr;
		  break;
		case 16:
		  read_result = *(volatile uint16_t*)virt_addr;
		  break;
		case 32:
		  read_result = *(volatile uint32_t*)virt_addr;
		  break;
		case 64:
		  read_result = *(volatile uint64_t*)virt_addr;
		default:
		  read_result = 0;
		  ret = false;
	}

	if (-1 == munmap(map_base, mapped_size))
	{
		ret = false;
		close(fd);
		return 0;
	}
	close(fd);
	return read_result;
}

ipmi::RspType<uint8_t,
              uint8_t,
			  uint8_t,
			  uint8_t> ipmiOEMGetStrapPinStatus(void)
{
	bool ret;
	uint32_t mask = 0xff000000;
	uint64_t read_result = devmem_read(PWRON, LENGTH_32BIT, ret);
	return (ret)? ipmi::responseSuccess( static_cast<uint8_t> (((uint32_t)read_result & mask)>>24),
	                                     static_cast<uint8_t> (((uint32_t)read_result & (mask>>8))>>16),
										 static_cast<uint8_t> (((uint32_t)read_result & (mask>>16))>>8),
										 static_cast<uint8_t> (((uint32_t)read_result & (mask>>24)))
	                                   ): ipmi::responseInvalidFieldRequest();
}


namespace ipmi
{
static void registerOEMFunctions() __attribute__((constructor));

static void registerOEMFunctions(void)
{
    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Registering Nuvoton IPMI OEM commands");
  // <Get Strap Pin Status command>
	registerHandler(prioOemBase, nuvoton::netFnGeneral,
                    nuvoton::general::cmdGetStrapPinStatus, Privilege::Callback,
                    ipmiOEMGetStrapPinStatus);
}

} // namespace ipmi