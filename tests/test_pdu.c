#include "test.h"
#include "pdu.h"
#include <string.h>

// PDU encode/decode tests — GSM 03.40 SMS-SUBMIT / SMS-DELIVER formats.
//
// 7-bit packing per GSM 03.38:
//   octet[n] = (septet[n] >> (n%7)) | (septet[n+1] << (7 - n%7))
// "Hello" packs to C8 32 9B FD 06.
//
// SCTS semi-octets are low-nibble-first:
//   20/09/21 15:24:06 +08  →  "02901251426080"
//
// Phone number BCD: digits swapped pairwise, odd length filled with F:
//   420123456789  →  "241032547698"

// Helper: prepare a decode context
static void _pdu_setup(pdu_t *pdu, ascii *content, ascii *phone)
{
    pdu_init(pdu);
    pdu->content = content;
    pdu->tel_num = phone;
    memset(content, 0, 200);
    memset(phone, 0, 24);
}

// ---- decode: basic SMS-DELIVER, 7-bit ----

TEST(pdu_decode_text7)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // SCA=00, type=04 (DELIVER), OA=+420123456789, PID=00, DCS=00,
    // SCTS=20/09/21 15:24:06 +08, UDL=05, UD="Hello"
    const ascii *v =
        "00040C9124103254769800000290125142608005C8329BFD06";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 5);
    ASSERT_EQ(pdu.type, PDU_TYPE_TEXT7);
    ASSERT_EQ(pdu.size, 5);
    ASSERT_STREQ(pdu.tel_num, "+420123456789");
    ASSERT_STREQ(pdu.content, "Hello");
    ASSERT_EQ(pdu.timestamp[0], 20);
    ASSERT_EQ(pdu.timestamp[1], 9);
    ASSERT_EQ(pdu.timestamp[2], 21);
    ASSERT_EQ(pdu.timestamp[3], 15);
    ASSERT_EQ(pdu.timestamp[4], 24);
    ASSERT_EQ(pdu.timestamp[5], 6);
}

// ---- decode: SMSC address is skipped ----

TEST(pdu_decode_smsc_skipped)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // same as above but with 7-octet SMSC prepended
    const ascii *v =
        "07916407058099F9040C9124103254769800000290125142608005C8329BFD06";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 5);
    ASSERT_STREQ(pdu.tel_num, "+420123456789");
    ASSERT_STREQ(pdu.content, "Hello");
}

// ---- decode: 8-bit data ----

TEST(pdu_decode_text8)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // DCS=04 (8-bit), UDL=05, raw ASCII
    const ascii *v =
        "00040C912410325476980004029012514260800548656C6C6F";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 5);
    ASSERT_EQ(pdu.type, PDU_TYPE_TEXT8);
    ASSERT_STREQ(pdu.content, "Hello");
}

// ---- decode: UCS2 ----

TEST(pdu_decode_ucs2)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // DCS=08 (UCS2), UDL=04 octets = 2 chars "He"
    const ascii *v =
        "00040C912410325476980008029012514260800400480065";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_EQ(pdu.type, PDU_TYPE_UCS2);
    ASSERT_STREQ(pdu.content, "He");
}

// ---- decode: concatenated 8-bit ----

TEST(pdu_decode_concat_text8)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // type=44 (UDHI), DCS=04, UDL=8 (6 UDH + 2 data)
    // UDH: len=05, IEI=00, IEDL=03, ref=07, max=02, seq=01
    const ascii *v =
        "00440C91241032547698000402901251426080080500030702014142";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_EQ(pdu.id, 7);
    ASSERT_EQ(pdu.count, 2);
    ASSERT_EQ(pdu.nr, 1);
    ASSERT_STREQ(pdu.content, "AB");
}

// ---- decode: concatenated 7-bit ----

TEST(pdu_decode_concat_text7)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // UDL=09 (7 UDH septets + 2 text), 7-bit text after 48-bit UDH
    // starts at bit offset 1: "A"=82, "B"=42
    const ascii *v =
        "00440C91241032547698000002901251426080090500030702018242";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_EQ(pdu.id, 7);
    ASSERT_EQ(pdu.count, 2);
    ASSERT_EQ(pdu.nr, 1);
    ASSERT_STREQ(pdu.content, "AB");
}

// ---- decode: status report ----

TEST(pdu_decode_status_report)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // type=02 (STATUS-REPORT), MR=05, RA=+420123456789,
    // SCTS+DT (2x7 octets), ST=00
    const ascii *v =
        "0002050C91241032547698029012514260800290125142608000";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_EQ(pdu.type, PDU_TYPE_SR);
    ASSERT_EQ(pdu.content[0], 5);   // message reference
    ASSERT_EQ(pdu.content[1], 0);   // status code
    ASSERT_STREQ(pdu.tel_num, "+420123456789");
}

// ---- decode: national number format ----

TEST(pdu_decode_national_number)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // OA type=A1 (TON=national, NPI=ISDN), 11 digits + F fill
    const ascii *v =
        "00040BA12143658709F100000290125142608000";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 0);
    ASSERT_STREQ(pdu.tel_num, "12345678901");
}

TEST(pdu_decode_ton_unknown)
{
    // The encoder uses type 0x81 (TON=unknown, NPI=ISDN) for numbers
    // without '+'. The decoder must accept TON=0 as digits.
    // Currently returns "unknown_81".
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    const ascii *v =
        "00040B812143658709F100000290125142608000";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 0);
    ASSERT_STREQ(pdu.tel_num, "12345678901");
}

// ---- decode: edge cases ----

TEST(pdu_decode_zero_length)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // UDL=00 — zero length is valid
    const ascii *v =
        "00040C9124103254769800000290125142608000";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 0);
    ASSERT_EQ(pdu.size, 0);
}

TEST(pdu_decode_wrong_size)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // UDL=A1 (161) — over the maximum
    const ascii *v =
        "00040C91241032547698000002901251426080A1";

    ASSERT_EQ(pdu_decode(&pdu, v), 0);
}

TEST(pdu_decode_unknown_mti)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // type=01 (SUBMIT) — not supported in decode direction
    const ascii *v =
        "00010C9124103254769800000290125142608000";

    ASSERT_EQ(pdu_decode(&pdu, v), -2);
}

TEST(pdu_decode_unknown_dcs)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // DCS=E4 — coding group 1110 reserved → rejected
    const ascii *v =
        "00040C9124103254769800E40290125142608000";

    ASSERT_EQ(pdu_decode(&pdu, v), -4);
}

// ---- decode: unknown UDH IEI is discarded ----

TEST(pdu_decode_unknown_udh)
{
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // UDH with IEI=24 (language shift) — not concatenation,
    // UDH discarded, content must still be extracted
    const ascii *v =
        "00440C91241032547698000402901251426080080524030102034142";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_EQ(pdu.count, 0);        // no concat info
    ASSERT_STREQ(pdu.content, "AB");
}

// ---- encode: basic SMS-SUBMIT, 7-bit ----

TEST(pdu_encode_text7)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "Hello";
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 5;
    pdu.count     = 1;
    pdu.nr        = 1;
    pdu.sr        = false;

    s16 len = pdu_encode(buf, &pdu);

    // SCA=00, type=11, MR=00, DA=0C 91 241032547698,
    // PID=00, DCS=00, VP=8F, UDL=05, UD=C8329BFD06
    // 20 octets total, SMSC octet excluded from the returned length
    ASSERT_EQ(len, 19);
    ASSERT_STREQ(buf, "0011000C9124103254769800008F05C8329BFD06");
}

// ---- encode: status report requested ----

TEST(pdu_encode_srr)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "Hello";
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 5;
    pdu.count     = 1;
    pdu.nr        = 1;
    pdu.sr        = true;

    pdu_encode(buf, &pdu);

    // PDU type byte has SRR bit: 11 | 20 = 31
    ASSERT_STREQ(buf, "0031000C9124103254769800008F05C8329BFD06");
}

// ---- encode: concatenated 7-bit ----

TEST(pdu_encode_concat_text7)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "AB";
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 2;
    pdu.count     = 2;
    pdu.nr        = 1;
    pdu.sr        = false;

    pdu_encode(buf, &pdu);

    // type=51 (UDHI), UDL=09 (7 UDH septets + 2),
    // UDH: 05 00 03 00 02 01, 7-bit "AB" with 1 fill bit: 82 42
    ASSERT_STREQ(buf, "0051000C9124103254769800008F090500030002018242");
}

// ---- encode: UCS2 raw data ----

TEST(pdu_encode_ucs2)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "\x00\x48\x00\x65";   // raw UCS2 "He"
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_UCS2;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 4;
    pdu.count     = 1;
    pdu.nr        = 1;
    pdu.sr        = false;

    pdu_encode(buf, &pdu);

    // DCS=08, UDL=04 octets
    ASSERT_STREQ(buf, "0011000C9124103254769800088F0400480065");
}

// ---- encode: national number (no '+') ----

TEST(pdu_encode_national_number)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "Hi";
    pdu.tel_num   = "123456789";    // 9 digits, odd → F fill
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 2;
    pdu.count     = 1;
    pdu.nr        = 1;
    pdu.sr        = false;

    pdu_encode(buf, &pdu);

    // DA type=81 (not 91), 9 digits + F → 21436587F9
    // "Hi" packs to C8 34
    ASSERT_STREQ(buf, "001100098121436587F900008F02C834");
}

// ---- encode: error cases ----

TEST(pdu_encode_nr_out_of_range)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "AB";
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 2;
    pdu.count     = 2;
    pdu.nr        = 3;              // nr > count

    ASSERT_EQ(pdu_encode(buf, &pdu), -1);
}

TEST(pdu_encode_empty_text7)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "";
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 0;
    pdu.count     = 1;
    pdu.nr        = 1;

    ASSERT_EQ(pdu_encode(buf, &pdu), -3);
}

TEST(pdu_encode_invalid_number)
{
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "Hi";
    pdu.tel_num   = "1";            // too short
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 2;
    pdu.count     = 1;
    pdu.nr        = 1;

    ASSERT_EQ(pdu_encode(buf, &pdu), -2);
}

// ---- bug discovery tests ----
//
// These tests assert the CORRECT per-spec behavior and FAIL against
// the current implementation. Fix the bugs, then these go green.

TEST(pdu_extension_chars_encodable)
{
    // _enc7b() must encode extension-table characters:
    //   '^' == ESC(0x1B) + extension 0x14
    // Currently '^' silently becomes '?'.
    pdu_t pdu;
    ascii buf[PDU_MAX_LENGTH];

    pdu_init(&pdu);
    pdu.content   = "a^b";
    pdu.tel_num   = "+420123456789";
    pdu.type      = PDU_TYPE_TEXT7;
    pdu.data_type = PDU_DATA_RAW;
    pdu.size      = 3;
    pdu.count     = 1;
    pdu.nr        = 1;
    pdu.sr        = false;

    pdu_encode(buf, &pdu);

    // septets a=61, ESC=1B, ext=14, b=62 → packed E1 0D 45 0C
    ASSERT_STREQ(buf, "0011000C9124103254769800008F04E10D450C");
}

TEST(pdu_euro_extension_decode)
{
    // GSM 03.38 extension 0x65 is € (U+20AC) — must decode to the
    // UTF-8 sequence E2 82 AC. Currently maps to ¤ and is written
    // as a raw byte instead of UTF-8.
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // UDL=02, septets: 1B (ESC) + 65 → packed 9B 32
    const ascii *v =
        "00040C91241032547698000002901251426080029B32";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 3);
    ASSERT_STREQ(pdu.content, "\xE2\x82\xAC");
}

TEST(pdu_multipart_ucs2_udh_subtracted)
{
    // The UCS2 decode branch must subtract the UDH octets from the
    // user-data length. Currently the extra octets leak into the text.
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // type=44 (UDHI), DCS=08, UDL=0A (6 UDH + 4 data octets = 2 chars)
    // UDH: 05 00 03 07 02 01, data "He", trailing "0041" must NOT be read
    const ascii *v =
        "00440C912410325476980008029012514260800A050003070201004800650041";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_STREQ(pdu.content, "He");
}

TEST(pdu_greek_alphabet_decode)
{
    // GSM 0x10 is Δ (U+0394) — must decode to UTF-8 CE 94.
    // Currently maps to control char 0x10.
    pdu_t pdu;
    ascii content[200], phone[24];

    _pdu_setup(&pdu, content, phone);

    // UDL=01, single septet 0x10
    const ascii *v =
        "00040C912410325476980000029012514260800110";

    s16 len = pdu_decode(&pdu, v);

    ASSERT_EQ(len, 2);
    ASSERT_STREQ(pdu.content, "\xCE\x94");
}

// ---- pdu_init defaults ----

TEST(pdu_init_defaults)
{
    pdu_t pdu;

    pdu_init(&pdu);

    ASSERT_EQ(pdu.type, PDU_TYPE_UNKNOWN);
    ASSERT_EQ(pdu.data_type, PDU_DATA_UTF8);
    ASSERT_EQ(pdu.count, 1);
    ASSERT_EQ(pdu.nr, 0);
    ASSERT_EQ(pdu.size, 0);
    ASSERT(pdu.content == NULL);
    ASSERT(pdu.tel_num == NULL);
}

// ---- regression: phone buffer is exactly PDU_MAX_PHONENUM_LEN (20 bytes) ----
//
// In the firmware the destination really is 20 bytes: sms_struct_t.tel_num is
// SMS_MAX_PHONE_LEN (20) and modem_sms_unso_pdu_parse() passes a 20-byte
// OS_MEM_ALLOC(MAX_PHONENUM_LEN). A 20-digit number with a non-international
// TON used to write 20 digits + a NUL terminator into those 20 bytes, i.e. one
// byte past the end (pdu_parse_telnum). Caught with ASan; the canary below
// makes the overrun fail here without needing a sanitizer build.
TEST(pdu_telnum_no_overrun_at_capacity)
{
    pdu_t pdu;
    ascii content[PDU_MAX_SMS_LEN + 1];

    struct {
        ascii tel[PDU_MAX_PHONENUM_LEN];
        u8    canary[8];
    } s;

    pdu_init(&pdu);
    pdu.content = content;
    pdu.tel_num = s.tel;
    memset(content, 0, sizeof(content));
    memset(s.tel, 0xA5, sizeof(s.tel));
    memset(s.canary, 0xC7, sizeof(s.canary));

    // SMS-DELIVER, OA length 0x14 (= 20 digits), TON 0x81 (national/unknown)
    s16 len = pdu_decode(&pdu,
                         "00" "04" "14" "81"
                         "11111111111111111111"
                         "00" "00" "00000000000000" "00");

    ASSERT(len <= 0);              // 20 digits + NUL cannot fit in 20 bytes
    ASSERT(s.canary[0] == 0xC7);   // nothing was written past tel[]
    ASSERT(s.canary[7] == 0xC7);
}

// Numbers that do fit must still decode unchanged.
TEST(pdu_telnum_18digit_decodes)
{
    pdu_t pdu;
    ascii content[200];   // _pdu_setup() memsets 200 bytes
    ascii phone[24];      // and 24 bytes here

    _pdu_setup(&pdu, content, phone);

    // OA length 0x12 (= 18 digits), TON 0x81
    pdu_decode(&pdu,
               "00" "04" "12" "81"
               "222222222222222222"
               "00" "00" "00000000000000" "00");

    ASSERT_STREQ(pdu.tel_num, "222222222222222222");
}
