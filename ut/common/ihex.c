#include "common.h"
#include "ihex.h"

static u32 hi_word = 0;

static ihex_parse_callback_t _data_parser = NULL;

static void _update_addr(u32 addr)
{
	u8 sum = 0;

	if ((addr >> 16) == (hi_word))
		return; 
	
	hi_word = (addr >> 16);

	sum += 0x02 + 0x04; 
	sum += hi_word & 0xFF;
	sum += (hi_word>>8) & 0xFF;

	OS_PRINTF(":02000004%04X%02X" NL, hi_word, (0x100 - sum) & 0xFF);
}

void ihex_init(ihex_parse_callback_t parser)
{
	_data_parser = parser;
}

void ihex_reset(void)
{
	hi_word = 0;
}


void ihex_flush(u32 addr, u8 *data, int len)
{
#define	HEX_LINE_SIZE (16)
	
	u8 sum = 0;
	char tmp_buf[16];
	int i,l;

	if (len == 0)
		return;

	while (len)
	{
		l = len > HEX_LINE_SIZE ? HEX_LINE_SIZE : len;
		len -= l;

		_update_addr(addr);

		snprintf(tmp_buf, sizeof(tmp_buf), ":%02X%04X00", l, addr & 0xFFFF);
		OS_PRINTF("%s", tmp_buf);

		sum = l;
		sum += addr & 0xFF;
		sum += (addr>>8) & 0xFF;

		for (i=0; i<l; i++)
		{
			OS_PRINTF("%02X", data[i]);
			sum += data[i];

		}

		OS_PRINTF("%02X", (0x100 - sum) & 0xFF);

		OS_PRINTF(NL);
		addr += l;
		data += l;
	}
}

static u8 gethex (ascii **s, u8 *sum) 
{
	u8 d=0;
	ascii ch;
	u8 i;
	
	for (i=0;i<2;i++)
	{	// zpracovat 2 ascii znaky == 1 byte
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

	data = (u8 *)src; // hex->bin prekoduju do tehoz mista

	if (*src != ':')
		return (false);

	src++; // preskocit uvodni ":"
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
		OS_ERROR("IHEX: chsum, SUM = 0x%02x" NL, sum);
		return (false);
	}
	switch (type)
	{
	case 0x00: // normální data
        if (_data_parser != NULL)
		{
			if (_data_parser(addr_hiword+addr, data, llen))
			{
				return (true);
			}
		}
		OS_ERROR("parse failed");
		break;

	case 0x01: // konec dat
		addr_hiword = 0;
		return (true);

	case 0x02: // adresový segment (addr<<4)
		addr_hiword=*((u8 *)data);
		addr_hiword <<= 8;
		addr_hiword+=*((u8 *)data + 1);
		addr_hiword <<= 4;
		return (true);

	case 0x03: // Start Segment Address Record
		break;

	case 0x04: // Extended Linear Address Record (HI 16b)
		addr_hiword=*((u8 *)data);
		addr_hiword <<= 8;
		addr_hiword+=*((u8 *)data + 1);
		addr_hiword <<= 16;
		return (true);

	case 0x05:
		return (true);
	}
	return (false);
}

