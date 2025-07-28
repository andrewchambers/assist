#ifndef AGENT_COMMANDS_H
#define AGENT_COMMANDS_H

#include <limits.h>

/**
 * Global buffer containing the executable path.
 * Initialized by self_exec_path_init().
 */
extern char g_executable_path[PATH_MAX * 2];

/**
 * Initialize the executable path with argv[0] from main.
 * This should be called early in main() to store the executable path.
 * The function will attempt to resolve argv[0] to an absolute path,
 * combining it with the current working directory if necessary.
 */
void self_exec_path_init(const char *argv0);

/**
 * Handle agent commands (agent-files, agent-cd, agent-done, agent-abort).
 * This function reads the state file from MINICODER_STATE_FILE environment variable,
 * updates it based on the command, and writes it back.
 * 
 * @param cmd The agent command name (e.g., "agent-files", "agent-cd")
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, 1 on error
 */
int agent_command_main(const char *cmd, int argc, char *argv[]);

#endif /* AGENT_COMMANDS_H */