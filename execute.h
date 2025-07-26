#ifndef EXECUTE_H
#define EXECUTE_H

/**
 * Execute a command and capture its output.
 * Returns exit code on success, -1 on failure.
 * If out_output is provided, captures stdout and stderr.
 * If forward_output is true, also forwards output to parent terminal in real-time.
 */
int exec_command(const char *command, char **out_output, int forward_output);

#endif /* EXECUTE_H */