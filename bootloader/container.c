#include "common.h"
#include "container.h"
#include "chsum.h"
#include "log.h"
LOG_DEF("CONTAINER");

bool container_valid(u8 *data, int len)
{
	container_hdr_t *container;
	u32 chsum = CHSUM32_START_VALUE;

	container = (container_hdr_t *)data;

	if (sizeof(container_hdr_t) != len)
	{
		LOG_ERROR("size");
		return (false);
	}
	if (container->hdr_chsum != chsum32(data, len-sizeof(u32), chsum))
	{
		LOG_ERROR("chsum failed");
		return (false);
	}
	return (true);
}

bool container_compatible (container_hdr_t *container)
{
  #warning "missing compatibility check"
	// container->device_id;     
	// container->hw_version_min; 
	// container->hw_version_max; 
	return (true);
}
