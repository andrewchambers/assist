#ifndef SPINNER_H
#define SPINNER_H

/**
 * Start the spinner animation on stderr with an optional message.
 * This function is idempotent - calling it multiple times has no effect
 * if the spinner is already running.
 * The spinner will not start if stderr is not a TTY.
 * @param message Optional message to display after the spinner (can be NULL)
 */
void start_spinner(const char *message);

/**
 * Stop the spinner animation.
 * This function is idempotent - calling it multiple times has no effect
 * if the spinner is already stopped.
 */
void stop_spinner(void);

#endif /* SPINNER_H */