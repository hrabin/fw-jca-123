#include "common.h"
#include "cfg.h"
#include "cfg_table.h"


const cfg_table_t CFG_TABLE[] = {
    //   ID               read_access,   write_access,  default value
    //
    {CFG_ID_STATE,        ACCESS_SYSTEM, ACCESS_SYSTEM, "S1"}, // S<system_state>,?,?,?
    {CFG_ID_PASSWD_SYSTEM,ACCESS_SYSTEM, ACCESS_SYSTEM, "uhlokRopnude"}, // most powerful password, access to everything
    {CFG_ID_PASSWD_ADMIN, ACCESS_ADMIN,  ACCESS_ADMIN,  "admin"},
    {CFG_ID_MAIN_SETUP,   ACCESS_USER,   ACCESS_ADMIN,  "5,0,0"}, // <siren beep time>,0,0
    {CFG_ID_MAIN_SWITCH,  ACCESS_USER,   ACCESS_ADMIN,  "0"}, // TBD
    {CFG_ID_SIM_PIN,      ACCESS_ADMIN,  ACCESS_ADMIN,  "1234"}, // TODO: use PIN for SIM
    {CFG_ID_APN,          ACCESS_USER,   ACCESS_ADMIN,  "internet"},
    
    // setup server for tracking (i.e. Traccar)
    {CFG_ID_TRACER_ADDR,  ACCESS_USER,   ACCESS_ADMIN,  "0.0.0.0:5013"}, // <IP>:<port>
    {CFG_ID_TRACER_PARAM, ACCESS_USER,   ACCESS_ADMIN,  "000000,10,30,300,0,3,0"}, // id, period, period_roaming, wait_time, protocol, limit_speed, end_mode
    
    // setup server for control  
//  {CFG_ID_SERVER_ADDR,  ACCESS_USER,   ACCESS_ADMIN,  "0.0.0.0:4444"}, // <IP>:<port> 
//  {CFG_ID_SERVER_KEY,   ACCESS_SYSTEM, ACCESS_ADMIN,  "a7604a1357412dbc688ee89b43378785"}, // AES 128 key (HEX)
    
    // setup server for OTA FW update
    {CFG_ID_UPDATE_SERVER_ADDR, ACCESS_USER, ACCESS_ADMIN, "0.0.0.0:3333"}, // <IP>:<port> used by command "UPDATE"
    {CFG_ID_UPDATE_SERVER_KEY, ACCESS_SYSTEM, ACCESS_ADMIN, "80feb47cae3c19274f2408d35398514c"}, // AES 128 key (HEX)

    {CFG_ID_TIME_ZONE,    ACCESS_USER,   ACCESS_USER,   "1,1"}, // timezone %d, DST 1/0

    {CFG_ID_DEVICE_NAME,  ACCESS_USER,   ACCESS_ADMIN,  "Your alarm reports: "},

    {CFG_ID_ADMIN_NAME,   ACCESS_USER,   ACCESS_ADMIN,  "Admin"},
    {CFG_ID_USER1_NAME,   ACCESS_USER,   ACCESS_USER,   "User 1"},
    {CFG_ID_USER2_NAME,   ACCESS_USER,   ACCESS_USER,   "User 2"},
    {CFG_ID_USER3_NAME,   ACCESS_USER,   ACCESS_USER,   "User 3"},
    {CFG_ID_USER4_NAME,   ACCESS_USER,   ACCESS_USER,   "User 4"},

    {CFG_ID_USER1_PHONE,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_USER2_PHONE,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_USER3_PHONE,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_USER4_PHONE,  ACCESS_USER,   ACCESS_USER,   ""},

    {CFG_ID_USER1_PASS,   ACCESS_ADMIN,  ACCESS_ADMIN,   ""},
    {CFG_ID_USER2_PASS,   ACCESS_ADMIN,  ACCESS_ADMIN,   ""},
    {CFG_ID_USER3_PASS,   ACCESS_ADMIN,  ACCESS_ADMIN,   ""},
    {CFG_ID_USER4_PASS,   ACCESS_ADMIN,  ACCESS_ADMIN,   ""},

    {CFG_ID_EVENT_SETUP_000,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_001,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_002,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_003,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_004,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_005,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_006,  ACCESS_USER,   ACCESS_USER,   ""},
    {CFG_ID_EVENT_SETUP_007,  ACCESS_USER,   ACCESS_USER,   ""},

    {CFG_ID_TEXT_EVENT_NAME_000, ACCESS_USER, ACCESS_ADMIN,  "Boot up"},
    {CFG_ID_TEXT_EVENT_NAME_001, ACCESS_USER, ACCESS_ADMIN,  "Shut down"},
    {CFG_ID_TEXT_EVENT_NAME_002, ACCESS_USER, ACCESS_ADMIN,  "Power fail"},
    {CFG_ID_TEXT_EVENT_NAME_003, ACCESS_USER, ACCESS_ADMIN,  "Power recovery"},
    {CFG_ID_TEXT_EVENT_NAME_004, ACCESS_USER, ACCESS_ADMIN,  "Battery LOW"},
    {CFG_ID_TEXT_EVENT_NAME_005, ACCESS_USER, ACCESS_ADMIN,  "Battery OK"},
    {CFG_ID_TEXT_EVENT_NAME_006, ACCESS_USER, ACCESS_ADMIN,  "Set"},
    {CFG_ID_TEXT_EVENT_NAME_007, ACCESS_USER, ACCESS_ADMIN,  "Unset"},
    {CFG_ID_TEXT_EVENT_NAME_008, ACCESS_USER, ACCESS_ADMIN,  "Alarm"},
    {CFG_ID_TEXT_EVENT_NAME_010, ACCESS_USER, ACCESS_ADMIN,  "Alarm cancel"},
    {CFG_ID_TEXT_EVENT_NAME_011, ACCESS_USER, ACCESS_ADMIN,  "Alarm timeout"},
    {CFG_ID_TEXT_EVENT_NAME_012, ACCESS_USER, ACCESS_ADMIN,  "Fault"},
    {CFG_ID_TEXT_EVENT_NAME_013, ACCESS_USER, ACCESS_ADMIN,  "Fault recovery"},
    {CFG_ID_TEXT_EVENT_NAME_014, ACCESS_USER, ACCESS_ADMIN,  "Jamming"},
    {CFG_ID_TEXT_EVENT_NAME_015, ACCESS_USER, ACCESS_ADMIN,  "No jamming"},


    {CFG_ID_TEXT_SOURCE_NAME_000, ACCESS_USER, ACCESS_ADMIN, "Unknown"},
    {CFG_ID_TEXT_SOURCE_NAME_001, ACCESS_USER, ACCESS_ADMIN, "Unit"},
    {CFG_ID_TEXT_SOURCE_NAME_002, ACCESS_USER, ACCESS_ADMIN, "Ignition key"},
    {CFG_ID_TEXT_SOURCE_NAME_003, ACCESS_USER, ACCESS_ADMIN, "Door"},
    {CFG_ID_TEXT_SOURCE_NAME_004, ACCESS_USER, ACCESS_ADMIN, "Input 1"},
    {CFG_ID_TEXT_SOURCE_NAME_005, ACCESS_USER, ACCESS_ADMIN, "Shock detector"},
    {CFG_ID_TEXT_SOURCE_NAME_006, ACCESS_USER, ACCESS_ADMIN, "Lock"},
    {CFG_ID_TEXT_SOURCE_NAME_007, ACCESS_USER, ACCESS_ADMIN, "Network"},
    {CFG_ID_TEXT_SOURCE_NAME_008, ACCESS_USER, ACCESS_ADMIN, "Battery"},
    {CFG_ID_TEXT_SOURCE_NAME_009, ACCESS_USER, ACCESS_ADMIN, ""},
    {CFG_ID_TEXT_SOURCE_NAME_010, ACCESS_USER, ACCESS_ADMIN, ""},
    {CFG_ID_TEXT_SOURCE_NAME_011, ACCESS_USER, ACCESS_ADMIN, ""},
    {CFG_ID_TEXT_SOURCE_NAME_012, ACCESS_USER, ACCESS_ADMIN, ""},
    {CFG_ID_TEXT_SOURCE_NAME_013, ACCESS_USER, ACCESS_ADMIN, ""},
    {CFG_ID_TEXT_SOURCE_NAME_014, ACCESS_USER, ACCESS_ADMIN, ""},
    {CFG_ID_TEXT_SOURCE_NAME_015, ACCESS_USER, ACCESS_ADMIN, ""},
        
    {CFG_ID_END, 0, 0, NULL} // end of table
};


static const cfg_table_t *_table_get(cfg_id_t cfg_id)
{
    const cfg_table_t *t = &CFG_TABLE[0];

    while (t->cfg_id != CFG_ID_END)
    {
        if (t->cfg_id == cfg_id)
        {
            return (t);
        }
        t++;
    }
    return (NULL);
}

cfg_id_t cfg_table_get_id(u32 row)
{   
    if (row >= sizeof(CFG_TABLE)/sizeof(cfg_table_t))
        return (CFG_ID_END);

    return (CFG_TABLE[row].cfg_id);
}

bool cfg_table_default(ascii *dest, cfg_id_t cfg_id)
{
    const cfg_table_t *t =_table_get(cfg_id);

    if (t == NULL)
        return (false);

    strncpy(dest, t->data, CFG_ITEM_SIZE);
    return (true);
}

bool cfg_table_access_read(cfg_id_t cfg_id, access_auth_t auth)
{
    const cfg_table_t *t =_table_get(cfg_id);

    if (t == NULL)
        return (false);

    return ((auth >= t->read_access) ? true : false);
}

bool cfg_table_access_write(cfg_id_t cfg_id, access_auth_t auth)
{
   const cfg_table_t *t =_table_get(cfg_id);

    if (t == NULL)
        return (false);

    return ((auth >= t->write_access) ? true : false);
}

