#include "command_handler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct CommandNameEntry
{
    char *name;
    CommandID id;
    struct CommandNameEntry *next;
} CommandNameEntry;

typedef struct ModuleCommandList
{
    ModuleID module_id;
    CommandNameEntry *command_id;
    struct ModuleCommandList *next;
} ModuleCommandList;

static CommandNameEntry *name_map_head = NULL;
static ModuleCommandList *module_map_head = NULL;

void initialize_command_handler()
{
    name_map_head = NULL;
    module_map_head = NULL;
}

void register_module(ModuleID mod_id, ModuleHandlerFunc handler)
{
    ModuleCommandList *new_module = (ModuleCommandList *)malloc(sizeof(ModuleCommandList));
    new_module->mod_id = mod_id;
    new_module->handler = handler;
    new_module->command_id = NULL; // Start with an empty command list
    new_module->next = module_map_head;
    module_map_head = new_module;
}

void register_command(const char* name, CommandID id, ModuleID mod_id)
{
    ModuleCommandList *mod = module_map_head;
    while (mod != NULL && mod->mod_id != mod_id)
    {mod = mod->next;}

    if (mod == NULL)
    {
        fprintf(stderr, "Module ID %d not registered.\n", mod_id);
        return;
    }

    // Add command to module's command list
    CommandNameEntry *new_entry = (CommandNameEntry *)malloc(sizeof(CommandNameEntry));
    new_entry->name = strdup(name);
    new_entry->id = id;
    new_entry->next = mod->command_id;
    mod->command_id = new_entry;
}

void route_command_by_name(int argc, char* argv[])
{
    if (argc < 1)
    {
        fprintf(stderr, "No command provided.\n");
        return;
    }

    const char *cmd_name = argv[0];
    CommandNameEntry *entry = name_map_head;

    while (entry != NULL)
    {
        if (strcmp(entry->name, cmd_name) != CMD_INVALID)
        {
            // Found the command, route it
            printf("Routing command '%s' with ID %d\n", entry->name, entry->id);
            // Here you would call the appropriate module's handler function
            ModuleHandlerFunc(entry->id);
            return;
        }
        entry = entry->next;
    }

    fprintf(stderr, "Command '%s' not recognized.\n", cmd_name);
}

void cleanup_command_handler()
{
    // Free command name entries
    CommandNameEntry *entry = name_map_head;
    while (entry != NULL)
    {
        CommandNameEntry *temp = entry;
        entry = entry->next;
        free(temp->name);
        free(temp);
    }
    name_map_head = NULL;

    // Free module command lists
    ModuleCommandList *mod = module_map_head;
    while (mod != NULL)
    {
        ModuleCommandList *temp_mod = mod;
        mod = mod->next;

        // Free commands in this module
        CommandNameEntry *cmd_entry = temp_mod->command_id;
        while (cmd_entry != NULL)
        {
            CommandNameEntry *temp_cmd = cmd_entry;
            cmd_entry = cmd_entry->next;
            free(temp_cmd->name);
            free(temp_cmd);
        }

        free(temp_mod);
    }
    module_map_head = NULL;
}