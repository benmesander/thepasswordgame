#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>     // For alarm(), read(), STDIN_FILENO
#include <signal.h>     // For signal(), SIGALRM
#include <termios.h>    // For disabling terminal echo

// --- Constants ---
#define INITIAL_TIME 60       // Starting time in seconds
#define TIME_DECREMENT 5      // Seconds to decrease time each round
#define MIN_TIME 10           // Minimum time limit
#define MAX_PASSWORD_LEN 100  // Max buffer size for password input
#define BASE_MIN_LEN 6        // Starting minimum password length

// --- Global Variables ---
volatile sig_atomic_t timed_out = 0; // Flag set by signal handler

// --- Structures ---
typedef struct {
    // Basic Requirements
    int min_length;
    int min_uppercase;
    int min_lowercase;
    int min_digits;
    int min_symbols;

    // Ridiculous Requirements (Flags: 0 = false, 1 = true)
    int req_start_upper_end_symbol;
    int req_no_consecutive_chars;
    int req_palindrome;
    int req_digit_sum;
    int digit_sum_target; // Only relevant if req_digit_sum is true

} PasswordRequirements;

// --- Function Prototypes ---
void handle_timeout(int sig);
void generate_requirements(PasswordRequirements *reqs, int round);
void display_requirements(const PasswordRequirements *reqs, int time_limit);
int get_hidden_input(char *buffer, int max_len);
int validate_password(const char *password, const PasswordRequirements *reqs);
void set_terminal_echo(int enable);

// --- Main Game Logic ---
int main() {
    srand(time(NULL)); // Seed the random number generator

    int round = 1;
    int current_time_limit = INITIAL_TIME;
    PasswordRequirements current_reqs;
    char password_buffer[MAX_PASSWORD_LEN];
    int successful_round = 1; // Flag to control game loop

    printf("--- Password Generation Game ---\n");
    printf("You will be given password requirements and a time limit.\n");
    printf("Enter a password meeting the criteria before time runs out!\n");
    printf("Press Enter to start...\n");
    getchar(); // Wait for user to start

    // Set up the signal handler for the timer
    signal(SIGALRM, handle_timeout);

    while (successful_round) {
        printf("\n--- Round %d ---\n", round);

        // Generate and display requirements for this round
        generate_requirements(&current_reqs, round);
        display_requirements(&current_reqs, current_time_limit);

        // Reset timeout flag and set the alarm
        timed_out = 0;
        alarm(current_time_limit); // Start the timer

        printf("Enter password: ");
        fflush(stdout); // Make sure prompt is shown before potentially blocking read

        // Get password input (hidden)
        int input_result = get_hidden_input(password_buffer, MAX_PASSWORD_LEN);

        // Cancel the alarm regardless of whether input was received or timeout occurred
        alarm(0);

        // Check if the input timed out
        if (timed_out) {
            printf("\n\n *** Time's up! ***\n");
            successful_round = 0; // End the game
            continue; // Skip validation
        }

        // Check if there was an error reading input
        if (input_result <= 0) {
             printf("\nError reading input.\n");
             successful_round = 0;
             continue;
        }

        printf("\n"); // Add a newline after hidden input is done

        // Validate the entered password
        if (validate_password(password_buffer, &current_reqs)) {
            printf("Success! Requirements met.\n");
            round++;
            // Decrease time limit, but not below MIN_TIME
            current_time_limit = (current_time_limit - TIME_DECREMENT > MIN_TIME) ?
                                 current_time_limit - TIME_DECREMENT : MIN_TIME;
        } else {
            printf("Failure! Password did not meet all requirements.\n");
            successful_round = 0; // End the game
        }

        // Small delay before next round or game over message
        sleep(1);
    }

    printf("\n--- Game Over ---\n");
    if (successful_round == 0 && !timed_out) { // Failed requirements, not timeout
         printf("You failed to meet the requirements for round %d.\n", round);
    } else if (timed_out) {
         printf("You ran out of time on round %d.\n", round);
    }
     printf("You completed %d round(s).\n", round - 1);


    return 0;
}

// --- Function Implementations ---

/**
 * @brief Signal handler for SIGALRM. Sets the timed_out flag.
 * @param sig The signal number (unused, but required by signal handler signature).
 */
void handle_timeout(int sig) {
    (void)sig; // Suppress unused parameter warning
    timed_out = 1;
    // Note: It's generally unsafe to call most functions (like printf)
    // directly inside a signal handler. Setting a flag is the safest approach.
    // We'll print the timeout message in the main loop after checking the flag.
    // We add a newline here to break the `read` call in get_hidden_input.
    write(STDOUT_FILENO, "\nTimeout!\n", 10); // Use write for signal safety
}

/**
 * @brief Generates password requirements based on the current round.
 * Complexity increases with the round number.
 * @param reqs Pointer to the PasswordRequirements struct to populate.
 * @param round The current round number (starting from 1).
 */
/**
 * @brief Generates password requirements based on the current round.
 * Complexity increases with the round number, adding ridiculous rules later.
 * @param reqs Pointer to the PasswordRequirements struct to populate.
 * @param round The current round number (starting from 1).
 */
void generate_requirements(PasswordRequirements *reqs, int round) {
    // --- Reset all requirements ---
    memset(reqs, 0, sizeof(PasswordRequirements)); // Important to clear flags!

    // --- Basic Requirements ---
    reqs->min_length = BASE_MIN_LEN + round + (round / 2); // Increase length a bit faster
    reqs->min_uppercase = 1 + (round / 2);
    reqs->min_lowercase = 1 + (round / 2);
    reqs->min_digits    = 1 + (round / 3);
    reqs->min_symbols   = (round > 1) ? (1 + (round-1) / 3) : 0; // Symbols start round 2

    // Ensure basic counts don't exceed length
    int min_char_sum = reqs->min_uppercase + reqs->min_lowercase + reqs->min_digits + reqs->min_symbols;
    if (reqs->min_length < min_char_sum) {
        reqs->min_length = min_char_sum; // Minimum length must accommodate minimum counts
    }

    // --- Ridiculous Requirements ---

    // Starts with Uppercase, Ends with Symbol (Round 3+)
    if (round >= 3) {
        reqs->req_start_upper_end_symbol = 1;
        // Ensure min length is at least 2 for this rule
        if (reqs->min_length < 2) reqs->min_length = 2;
        // Ensure we require at least one uppercase and one symbol for this rule
        if (reqs->min_uppercase < 1) reqs->min_uppercase = 1;
        if (reqs->min_symbols < 1) reqs->min_symbols = 1;
    }

    // No Consecutive Identical Characters (Round 4+)
    if (round >= 4) {
        reqs->req_no_consecutive_chars = 1;
    }

    // Palindrome (ONLY Round 5 - adjust if desired)
    if (round == 5) {
        reqs->req_palindrome = 1;
        // Palindromes often look simpler, maybe relax other constraints slightly for this round?
        // Example: reduce required symbols/digits IF palindrome is active
        // reqs->min_digits = (reqs->min_digits > 1) ? reqs->min_digits - 1 : 0;
        // reqs->min_symbols = (reqs->min_symbols > 1) ? reqs->min_symbols - 1 : 0;
    }

    // Specific Sum of Digits (Round 7+)
    if (round >= 7) {
        reqs->req_digit_sum = 1;
        // Ensure we require at least one digit for this rule
        if (reqs->min_digits < 1) reqs->min_digits = 1;
        // Generate a target sum. Base 5, increases with round, random element.
        reqs->digit_sum_target = 5 + (round / 2) + (rand() % (round * 2 + 1));
    }

     // --- Final Sanity Check (Optional but Recommended) ---
     // Readjust min length again if rules imply higher counts
     min_char_sum = reqs->min_uppercase + reqs->min_lowercase + reqs->min_digits + reqs->min_symbols;
     if (reqs->min_length < min_char_sum) {
        reqs->min_length = min_char_sum;
     }
     // If palindrome is required, length must allow diverse chars if other counts are high
     if (reqs->req_palindrome && reqs->min_length < min_char_sum * 1.5) {
         // Increase length slightly for palindromes with many required char types
         // reqs->min_length = (int)(min_char_sum * 1.5) + 1;
     }
}

/**
 * @brief Displays the current password requirements and time limit.
 * @param reqs Pointer to the PasswordRequirements struct.
 * @param time_limit The current time limit in seconds.
 */
void display_requirements(const PasswordRequirements *reqs, int time_limit) {
    printf("Time Limit: %d seconds\n", time_limit);
    printf("Requirements:\n");
    // Basic
    printf("  - Minimum Length: %d\n", reqs->min_length);
    if (reqs->min_uppercase > 0) printf("  - Minimum Uppercase: %d\n", reqs->min_uppercase);
    if (reqs->min_lowercase > 0) printf("  - Minimum Lowercase: %d\n", reqs->min_lowercase);
    if (reqs->min_digits > 0)    printf("  - Minimum Digits: %d\n", reqs->min_digits);
    if (reqs->min_symbols > 0)   printf("  - Minimum Symbols (e.g., !@#$%%^&*): %d\n", reqs->min_symbols);

    // Ridiculous
    printf("  --- Special Rules ---\n");
    if (reqs->req_start_upper_end_symbol) {
        printf("  - Must START with an Uppercase letter\n");
        printf("  - Must END with a Symbol\n");
    }
    if (reqs->req_no_consecutive_chars) {
        printf("  - No consecutive identical characters (e.g., 'aa', '11')\n");
    }
    if (reqs->req_palindrome) {
        printf("  - Must be a PALINDROME (reads the same forwards and backwards)\n");
    }
    if (reqs->req_digit_sum) {
        printf("  - The SUM of all digits must be EXACTLY %d\n", reqs->digit_sum_target);
    }
     if (!reqs->req_start_upper_end_symbol && !reqs->req_no_consecutive_chars &&
         !reqs->req_palindrome && !reqs->req_digit_sum) {
         printf("  - (None this round)\n");
     }
}

/**
 * @brief Sets terminal echoing on or off.
 * @param enable 1 to enable echo, 0 to disable.
 */
void set_terminal_echo(int enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (!enable) {
        tty.c_lflag &= ~ECHO; // Disable echo bit
    } else {
        tty.c_lflag |= ECHO;  // Enable echo bit
    }
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

/**
 * @brief Reads a line of input from the user without echoing it to the terminal.
 * Handles the backspace character for basic editing.
 * @param buffer The buffer to store the input.
 * @param max_len The maximum size of the buffer (including null terminator).
 * @return The number of characters read (excluding null terminator), or -1 on error.
 * Returns 0 if timeout occurred before any input.
 */
int get_hidden_input(char *buffer, int max_len) {
    if (buffer == NULL || max_len <= 0) {
        return -1; // Invalid arguments
    }

    set_terminal_echo(0); // Disable echoing

    int i = 0;
    char ch;
    ssize_t bytes_read;

    memset(buffer, 0, max_len); // Clear the buffer

    while (i < max_len - 1) {
        // Read one character at a time
        // This read call will be interrupted by the SIGALRM signal
        bytes_read = read(STDIN_FILENO, &ch, 1);

        if (timed_out) { // Check flag immediately after read returns
             set_terminal_echo(1); // Re-enable echo before returning
             buffer[i] = '\0'; // Null terminate potentially partial input
             return 0; // Indicate timeout occurred
        }

        if (bytes_read < 0) { // Read error (potentially EINTR if not handled, but timeout is handled)
            set_terminal_echo(1);
            perror("read error");
            return -1;
        }

        if (bytes_read == 0) { // EOF - less likely in interactive session
             break;
        }


        if (ch == '\n' || ch == '\r') { // Enter key pressed
            break; // End of input
        } else if (ch == 127 || ch == 8) { // Handle backspace (ASCII 127 or 8)
            if (i > 0) {
                i--;
                 // Optionally print backspace, space, backspace to erase visually
                 // write(STDOUT_FILENO, "\b \b", 3);
            }
        } else if (isprint(ch)) { // Only add printable characters
             buffer[i++] = ch;
             // Optionally print '*' for visual feedback
             // write(STDOUT_FILENO, "*", 1);
        }
    }

    buffer[i] = '\0'; // Null-terminate the string
    set_terminal_echo(1); // Re-enable echoing

    return i; // Return number of characters entered
}

/**
 * @brief Validates if the given password meets ALL specified requirements, including ridiculous ones.
 * @param password The password string to validate.
 * @param reqs Pointer to the PasswordRequirements struct.
 * @return 1 if the password is valid, 0 otherwise.
 */
int validate_password(const char *password, const PasswordRequirements *reqs) {
    int len = 0;
    int upper_count = 0;
    int lower_count = 0;
    int digit_count = 0;
    int symbol_count = 0;
    int current_digit_sum = 0;

    // --- Basic Length Check ---
    len = strlen(password);
    if (len < reqs->min_length) {
        printf("    Validation Fail: Too short (Length: %d, Required: %d)\n", len, reqs->min_length);
        return 0;
    }

    // --- Iterate and Count Basic Types + Calculate Digit Sum ---
    for (int i = 0; password[i] != '\0'; i++) {
        if (isupper(password[i])) {
            upper_count++;
        } else if (islower(password[i])) {
            lower_count++;
        } else if (isdigit(password[i])) {
            digit_count++;
            current_digit_sum += password[i] - '0'; // Convert char digit to int and add
        } else if (ispunct(password[i])) { // ispunct checks for punctuation characters
            symbol_count++;
        }
    }

    // --- Check Basic Counts ---
    if (upper_count < reqs->min_uppercase) {
        printf("    Validation Fail: Not enough uppercase (Found: %d, Required: %d)\n", upper_count, reqs->min_uppercase);
        return 0;
    }
    if (lower_count < reqs->min_lowercase) {
        printf("    Validation Fail: Not enough lowercase (Found: %d, Required: %d)\n", lower_count, reqs->min_lowercase);
        return 0;
    }
    if (digit_count < reqs->min_digits) {
        printf("    Validation Fail: Not enough digits (Found: %d, Required: %d)\n", digit_count, reqs->min_digits);
        return 0;
    }
    if (symbol_count < reqs->min_symbols) {
        printf("    Validation Fail: Not enough symbols (Found: %d, Required: %d)\n", symbol_count, reqs->min_symbols);
        return 0;
    }

    // --- Check Ridiculous Requirements (only if active) ---

    // 1. Starts with Uppercase, Ends with Symbol
    if (reqs->req_start_upper_end_symbol) {
        if (len == 0) { // Should be caught by min_length, but safe check
             printf("    Validation Fail: Cannot check start/end on empty password.\n");
             return 0;
        }
        if (!isupper(password[0])) {
            printf("    Validation Fail: Must start with an uppercase letter.\n");
            return 0;
        }
        if (!ispunct(password[len - 1])) {
            printf("    Validation Fail: Must end with a symbol.\n");
            return 0;
        }
    }

    // 2. No Consecutive Identical Characters
    if (reqs->req_no_consecutive_chars) {
        for (int i = 0; i < len - 1; i++) {
            if (password[i] == password[i + 1]) {
                printf("    Validation Fail: Found consecutive identical characters ('%c%c') at position %d.\n", password[i], password[i+1], i);
                return 0;
            }
        }
    }

    // 3. Palindrome Check
    if (reqs->req_palindrome) {
        int is_palindrome = 1; // Assume true initially
        for (int i = 0; i < len / 2; i++) {
            if (password[i] != password[len - 1 - i]) {
                is_palindrome = 0;
                break;
            }
        }
        if (!is_palindrome) {
            printf("    Validation Fail: Password is not a palindrome.\n");
            return 0;
        }
    }

    // 4. Digit Sum Check
    if (reqs->req_digit_sum) {
        if (current_digit_sum != reqs->digit_sum_target) {
            printf("    Validation Fail: Sum of digits is %d, but required sum is %d.\n", current_digit_sum, reqs->digit_sum_target);
            return 0;
        }
         // Also check if the required min digits was 0 but the sum target wasn't 0 (makes it impossible)
         // This check should ideally be handled in generation, but belt-and-suspenders here.
         if (reqs->min_digits == 0 && reqs->digit_sum_target != 0) {
             printf("    Internal Logic Warning: Digit sum required, but min digits is 0!\n");
             // It's impossible to meet, so fail it.
             return 0;
         }
    }


    // --- All checks passed! ---
    return 1;
}
