#include "common.h"
#include "ihex.h"
#include "ext_storage.h"

#define	_PREFIX "IHEX: "


u8 gethex (ascii **s, u8 *sum) 
{
	u8 d=0;
	ascii ch;
	u8 i;
	
	for (i=0; i<2; i++)
	{	// process 2 characters == 1 byte
		d<<=4;
		ch=**s;
		// upcase
		if ((ch>='a') && (ch<='f'))
			ch-=('a'-'A');	

		if ((ch>='0') && (ch<='9'))
			d+=(ch-'0');
		else if ((ch>='A') && (ch<='F'))
			d+=(ch-'A'+10);
		else
			return (0);
		(*s)++;
	}
	*sum+=d;
	return (d);
}


bool ihex_parse (char *src)
{	
	static u32 addr_hiword=0;
	u32 addr;
	u8  *pdata, *data;
	u8  l, sum, llen, type;

	data = (u8 *)src; // hex->bin encoding to the same place 

	if (*src != ':')
		return (false);

	src++; // skip the ":"
	if ((l=strlen(src)) < 8)
		return (false);

	sum=0;
	llen= gethex (&src, &sum);
	addr= (gethex (&src, &sum)<<8);
	addr|=gethex (&src, &sum);
	type= gethex (&src, &sum);
	
	if (l-8 < (llen<<1))
	{
		OS_ERROR("IHEX: len");
		return (false);
	}

	pdata = data;
	
	l=llen;
	while (l) 
	{
		l--;
		*(pdata++)=gethex (&src, &sum);
	}

	gethex (&src, &sum);
	if (sum) 
	{
		OS_ERROR("IHEX: chsum");
		OS_PRINTF("SUM = 0x%02x" NL, sum);
		return (false);
	}
	switch (type)
	{
	case 0x00: // data
        if (ext_storage_write_data(addr_hiword+addr, data, llen))
		{
			// OS_PRINTF(":00%04lx" NL, addr);
			return (true);
		}
		OS_ERROR(_PREFIX "write");
		break;

	case 0x01: // end of data
		addr_hiword = 0;
		// OS_PRINTF(":01" NL);
		ext_storage_flush_cache();
		return (true);

	case 0x02: // address segment (addr<<4)
		addr_hiword=*((u8 *)data);
		addr_hiword <<= 8;
		addr_hiword+=*((u8 *)data + 1);
		addr_hiword <<= 4;
		// OS_PRINTF(_PREFIX "HI ADDR 0x%lx",addr_hiword);
		return (true);

	case 0x03: // Start Segment Address Record
		break;

	case 0x04: // Extended Linear Address Record (HI 16b)
		addr_hiword=*((u8 *)data);
		addr_hiword <<= 8;
		addr_hiword+=*((u8 *)data + 1);
		addr_hiword <<= 16;
		// OS_PRINTF(":04%04lx" NL, (addr_hiword>>16));
		return (true);
	}
	return (false);
}

