#include "Shell.h"
#include "command_handler.h"
#include "execute.h"
#include "parser.h"

void *ModuleHandlerFunc(CommandID cmd_id)
{
    printf("Handling command ID %d\n", cmd_id);
}

// The main entry point for the shell application.
int main(int argc, char **argv)
{
    // 1. Set up the signal handlers for Ctrl+C, etc.
    setup_signals();
    //2. Initialize the command handler system
    initialize_command_handler();
    //3. Register modules and their commands
    register_module(MODULE_SW_RELAY_CLIENT, ModuleHandlerFunc);

    register_command("RELAY_CLI_CMD_GET", RELAY_CLI_CMD_GET, MODULE_SW_RELAY_CLIENT);

    register_command("RELAY_CLI_CMD_SET", RELAY_CLI_CMD_SET, MODULE_SW_RELAY_CLIENT);

   // route_command_by_name(5, (char *[]){"RELAY_CLI_CMD_GET", "0", "0", "1"});

    //route_command_by_name(5, (char *[]){"RELAY_CLI_CMD_SET", "1", "0", "0", "0"});

    //4. Start the main Read-Eval-Print Loop.
    shell_loop();
 
    //5. Cleanup and exit.
    cleanup_command_handler() return 0;
}