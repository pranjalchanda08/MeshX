#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

// --- Public Type Definitions ---
typedef enum {
    MODULE_SW_RELAY_CLIENT,
    MODULE_LIGHT_CWWW_CLIENT,
    MODULE_OS_TIMER,
    MODULE_MESHX_NVS
} ModuleID;

typedef enum {
    CMD_INVALID = -1,
   /*Module: Switch Relay Client */
    RELAY_CLI_CMD_GET,
    RELAY_CLI_CMD_SET,
    RELAY_CLI_CMD_SET_UNACK,

  /*Module: Light CWWW Client */
    CWWW_CLI_UT_CMD_ONOFF_GET,
    CWWW_CLI_UT_CMD_ONOFF_SET,
    CWWW_CLI_UT_CMD_ONOFF_SET_UNACK,
    CWWW_CLI_UT_CMD_CTL_GET,
    CWWW_CLI_UT_CMD_CTL_SET,
    CWWW_CLI_UT_CMD_CTL_SET_UNACK,
    CWWW_CLI_UT_CMD_LIGHTNESS_SET,
    CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK,
    CWWW_CLI_UT_CMD_TEMPERATURE_SET,
    CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK,
    CWWW_CLI_UT_CMD_DELTA_UV_SET,
    CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK,
    CWWW_CLI_UT_CMD_TEMP_RANGE_SET,
    CWWW_CLI_UT_CMD_TEMP_RANGE_SET_UNACK,
 /*  Module: OS Timer                                  */
    OS_TIMER_CLI_CMD_CREATE,
    OS_TIMER_CLI_CMD_ARM,
    OS_TIMER_CLI_CMD_REARM,
    OS_TIMER_CLI_CMD_DISARM,
    OS_TIMER_CLI_CMD_DELETE,
    OS_TIMER_CLI_CMD_PERIOD_SET,

  /* Module :MeshX NVS*/
    MESHX_NVS_CLI_CMD_OPEN,
    MESHX_NVS_CLI_CMD_SET,
    MESHX_NVS_CLI_CMD_GET,
    MESHX_NVS_CLI_CMD_COMMIT,
    MESHX_NVS_CLI_CMD_REMOVE,
    MESHX_NVS_CLI_CMD_ERASE,
    MESHX_NVS_CLI_CMD_CLOSE
    
} CommandID;

typedef void (*ModuleHandlerFunc)(CommandID cmd_id);

// --- Public Function Prototypes ---

// Call this at the start of your program
void initialize_command_handler();

void register_module(ModuleID mod_id, ModuleHandlerFunc handler);
// Call this to add a new command to the system
void register_command(const char* name, CommandID id, ModuleID mod_id);

// Call this to parse and execute a command string
void route_command_by_name(int argc, char* argv[]);

// Call this before your program exits to clean up memory
void cleanup_command_handler();

#endif