// --- DOM Elements ---
const roundNumberEl = document.getElementById('round-number');
const timerDisplayEl = document.getElementById('timer-display');
const requirementsListEl = document.getElementById('requirements-list');
const passwordInputEl = document.getElementById('password-input');
const submitButtonEl = document.getElementById('submit-button');
const feedbackMessageEl = document.getElementById('feedback-message');
const startGameButtonEl = document.getElementById('start-game-button');

// --- Game State Variables ---
let currentRound = 0;
let currentTimeLimit = 60; // Default, will be set by startRound
let timerId = null;        // For setTimeout (actual timeout)
let intervalId = null;     // For setInterval (visual timer update)
let currentRequirements = {};
let gameActive = false;
let timeLeft = 0;

// --- Constants (from C) ---
const INITIAL_TIME = 60;
const TIME_DECREMENT = 5;
const MIN_TIME = 10;
const BASE_MIN_LEN = 6;

// --- Helper: Check if a character is a 'symbol' (like C's ispunct) ---
// Basic version: Not letter, not digit, not whitespace
function isSymbol(char) {
    return !/[a-zA-Z0-9\s]/.test(char);
}

// --- Functions ---

/**
 * Generates password requirements based on the current round.
 * Translates logic directly from the C version.
 * @param {number} round - The current round number (starting from 1).
 * @returns {object} - The requirements object for the round.
 */
function generateRequirements(round) {
    // --- Reset requirements object ---
    let reqs = {
        minLength: 0, minUppercase: 0, minLowercase: 0, minDigits: 0, minSymbols: 0,
        reqStartUpperEndSymbol: false, reqNoConsecutiveChars: false,
        reqPalindrome: false, reqDigitSum: false, digitSumTarget: 0
    };

    // --- Basic Requirements (Translate C logic using Math.random()) ---
    reqs.minLength = BASE_MIN_LEN + round + Math.floor(round / 2); // Increased length slightly faster
    reqs.minUppercase = 1 + Math.floor(round / 2);
    reqs.minLowercase = 1 + Math.floor(round / 2);
    reqs.minDigits = 1 + Math.floor(round / 3);
    reqs.minSymbols = (round > 1) ? (1 + Math.floor((round - 1) / 3)) : 0; // Symbols start round 2

    // Ensure basic counts don't exceed length (Initial check)
    let minCharSum = reqs.minUppercase + reqs.minLowercase + reqs.minDigits + reqs.minSymbols;
    if (reqs.minLength < minCharSum) {
        reqs.minLength = minCharSum; // Minimum length must accommodate minimum counts
    }

    // --- Ridiculous Requirements (Translate C logic) ---

    // Starts with Uppercase, Ends with Symbol (Round 3+)
    if (round >= 3) {
        reqs.reqStartUpperEndSymbol = true;
        if (reqs.minLength < 2) reqs.minLength = 2; // Need at least 2 chars
        if (reqs.minUppercase < 1) reqs.minUppercase = 1;
        if (reqs.minSymbols < 1) reqs.minSymbols = 1;
    }

    // No Consecutive Identical Characters (Round 4+)
    if (round >= 4) {
        reqs.reqNoConsecutiveChars = true;
    }

    // Palindrome (ONLY Round 5 - adjust if desired)
    if (round === 5) {
        reqs.reqPalindrome = true;
        // Optional: Relax other constraints slightly for palindrome round
        // reqs.minDigits = Math.max(0, reqs.minDigits - 1);
        // reqs.minSymbols = Math.max(0, reqs.minSymbols - 1);
    }

    // Specific Sum of Digits (Round 7+)
    if (round >= 7) {
        reqs.reqDigitSum = true;
        if (reqs.minDigits < 1) reqs.minDigits = 1; // Need digits for a sum target
        // Generate target sum: Base 5, increases with round, random element.
        reqs.digitSumTarget = 5 + Math.floor(round / 2) + Math.floor(Math.random() * (round * 2 + 1));
    }

    // --- Final Sanity Check (Translate C logic) ---
    // Readjust min length again if rules imply higher counts
    minCharSum = reqs.minUppercase + reqs.minLowercase + reqs.minDigits + reqs.minSymbols;
    if (reqs.minLength < minCharSum) {
        reqs.minLength = minCharSum;
    }
    // Optional: Further length adjustment for tricky combinations like palindrome + high counts
    // if (reqs.reqPalindrome && reqs.minLength < minCharSum * 1.5) {
    //     reqs.minLength = Math.floor(minCharSum * 1.5) + 1;
    // }

    console.log(`Generated Reqs for Round ${round}:`, reqs); // For debugging
    return reqs;
}

/**
 * Displays the current requirements in the HTML list.
 * @param {object} reqs - The requirements object.
 */
function displayRequirements(reqs) {
    requirementsListEl.innerHTML = ''; // Clear previous list

    function addItem(text) {
        const li = document.createElement('li');
        li.textContent = text;
        requirementsListEl.appendChild(li);
    }

    // Basic
    addItem(`Minimum Length: ${reqs.minLength}`);
    if (reqs.minUppercase > 0) addItem(`Minimum Uppercase: ${reqs.minUppercase}`);
    if (reqs.minLowercase > 0) addItem(`Minimum Lowercase: ${reqs.minLowercase}`);
    if (reqs.minDigits > 0) addItem(`Minimum Digits: ${reqs.minDigits}`);
    if (reqs.minSymbols > 0) addItem(`Minimum Symbols: ${reqs.minSymbols}`);

    // Ridiculous
    let specialRules = false;
    if (reqs.reqStartUpperEndSymbol || reqs.reqNoConsecutiveChars || reqs.reqPalindrome || reqs.reqDigitSum) {
        const titleLi = document.createElement('li');
        titleLi.innerHTML = '<strong>--- Special Rules ---</strong>';
        // Insert title before the first item if special rules exist
        if (requirementsListEl.firstChild) {
            requirementsListEl.insertBefore(titleLi, requirementsListEl.firstChild);
        } else {
             requirementsListEl.appendChild(titleLi);
        }
        specialRules = true;
    }

    if (reqs.reqStartUpperEndSymbol) { addItem("Must START with an Uppercase letter AND END with a Symbol"); }
    if (reqs.reqNoConsecutiveChars) { addItem("No consecutive identical characters (e.g., 'aa', '11')"); }
    if (reqs.reqPalindrome) { addItem("Must be a PALINDROME"); }
    if (reqs.reqDigitSum) { addItem(`The SUM of all digits must be EXACTLY ${reqs.digitSumTarget}`); }

    if (!specialRules && currentRound > 0) { // Only show "None" if round started and no specials
         addItem("(No special rules this round)");
    } else if (currentRound === 0) {
        addItem("Click Start Game!"); // Initial message
    }
}

/**
 * Updates the visual timer display in the HTML.
 */
function updateTimerDisplay() {
    timerDisplayEl.textContent = timeLeft >= 0 ? timeLeft : 0; // Don't show negative time
    // Optional: Add styling for low time
    if (timeLeft <= 5 && timeLeft > 0) {
        timerDisplayEl.style.color = 'red';
        timerDisplayEl.style.fontWeight = 'bold';
    } else {
        timerDisplayEl.style.color = ''; // Reset color
        timerDisplayEl.style.fontWeight = '';
    }
}

/**
 * Handles the timeout event triggered by setTimeout.
 */
function handleTimeout() {
    console.log("Timeout!");
    gameActive = false;
    passwordInputEl.disabled = true;
    submitButtonEl.disabled = true;
    clearInterval(intervalId); // Stop visual timer
    timeLeft = 0; // Ensure timer shows 0
    updateTimerDisplay(); // Update display to 0
    displayFeedback(`Time's up on Round ${currentRound}! Game Over.`, "error");
    startGameButtonEl.textContent = "Play Again?";
    startGameButtonEl.disabled = false;
}

/**
 * Starts the timers (visual interval and timeout).
 * @param {number} seconds - The time limit for the round.
 */
function startTimer(seconds) {
    clearTimeout(timerId); // Clear any residual timeout timer
    clearInterval(intervalId); // Clear any residual interval timer

    timeLeft = seconds;
    updateTimerDisplay(); // Show initial time immediately

    // Interval timer for updating the display every second
    intervalId = setInterval(() => {
        timeLeft--;
        updateTimerDisplay();
        // Note: Timeout is handled separately by setTimeout,
        // this interval is purely for visual feedback.
        // We could stop the interval in handleTimeout, but clearing it
        // here at the start of the *next* timer is also safe.
        if (timeLeft <= 0) {
            clearInterval(intervalId); // Stop interval when time visually hits 0
        }
    }, 1000);

    // Timeout timer for ending the round
    timerId = setTimeout(handleTimeout, seconds * 1000);
}

/**
 * Validates the password against the current requirements.
 * Translates logic directly from the C version.
 * @param {string} password - The password entered by the user.
 * @param {object} reqs - The requirements object for the round.
 * @returns {boolean|string} - True if valid, or a string explaining the failure reason.
 */
function validatePassword(password, reqs) {
    const len = password.length;
    let upperCount = 0;
    let lowerCount = 0;
    let digitCount = 0;
    let symbolCount = 0;
    let currentDigitSum = 0;

    // --- Iterate and Count Basic Types + Calculate Digit Sum ---
    for (let i = 0; i < len; i++) {
        const char = password[i];
        if (/[A-Z]/.test(char)) {
            upperCount++;
        } else if (/[a-z]/.test(char)) {
            lowerCount++;
        } else if (/[0-9]/.test(char)) {
            digitCount++;
            currentDigitSum += parseInt(char, 10); // Add the numeric value
        } else if (isSymbol(char)) { // Use our helper
            symbolCount++;
        }
    }

    // --- Check Basic Counts ---
    if (len < reqs.minLength) return `Validation Fail: Too short (Length: ${len}, Required: ${reqs.minLength})`;
    if (upperCount < reqs.minUppercase) return `Validation Fail: Not enough uppercase (Found: ${upperCount}, Required: ${reqs.minUppercase})`;
    if (lowerCount < reqs.minLowercase) return `Validation Fail: Not enough lowercase (Found: ${lowerCount}, Required: ${reqs.minLowercase})`;
    if (digitCount < reqs.minDigits) return `Validation Fail: Not enough digits (Found: ${digitCount}, Required: ${reqs.minDigits})`;
    if (symbolCount < reqs.minSymbols) return `Validation Fail: Not enough symbols (Found: ${symbolCount}, Required: ${reqs.minSymbols})`;

    // --- Check Ridiculous Requirements (only if active) ---

    // 1. Starts with Uppercase, Ends with Symbol
    if (reqs.reqStartUpperEndSymbol) {
        if (len === 0) return "Validation Fail: Cannot check start/end on empty password."; // Edge case
        if (!/^[A-Z]/.test(password)) return "Validation Fail: Must start with an uppercase letter.";
        // Check last character using isSymbol helper
        if (!isSymbol(password[len - 1])) return "Validation Fail: Must end with a symbol.";
    }

    // 2. No Consecutive Identical Characters
    if (reqs.reqNoConsecutiveChars) {
        for (let i = 0; i < len - 1; i++) {
            if (password[i] === password[i + 1]) {
                return `Validation Fail: Found consecutive identical characters ('${password[i]}${password[i+1]}') at position ${i}.`;
            }
        }
    }

    // 3. Palindrome Check
    if (reqs.reqPalindrome) {
        const reversedPassword = password.split('').reverse().join('');
        if (password !== reversedPassword) {
            return "Validation Fail: Password is not a palindrome.";
        }
    }

    // 4. Digit Sum Check
    if (reqs.reqDigitSum) {
        // Check if the required sum matches the calculated sum
        if (currentDigitSum !== reqs.digitSumTarget) {
            return `Validation Fail: Sum of digits is ${currentDigitSum}, but required sum is ${reqs.digitSumTarget}.`;
        }
         // Sanity check from C: If sum > 0 is required, but min digits is 0, it's impossible.
         if (reqs.minDigits === 0 && reqs.digitSumTarget !== 0) {
             console.warn("Internal Logic Warning: Digit sum > 0 required, but min digits is 0!");
             return `Validation Fail: Impossible Rule - Digit sum required (${reqs.digitSumTarget}), but 0 digits allowed?`;
         }
    }

    // --- All checks passed! ---
    return true;
}

/**
 * Displays feedback messages to the user.
 * @param {string} message - The text to display.
 * @param {string} [type="info"] - "info", "success", or "error" for styling.
 */
function displayFeedback(message, type = "info") {
    feedbackMessageEl.textContent = message;
    feedbackMessageEl.className = ''; // Clear previous classes
    if (type === "error") {
        feedbackMessageEl.classList.add("error-message");
    } else if (type === "success") {
        feedbackMessageEl.classList.add("success-message");
    }
}

/**
 * Sets up and starts a new round.
 */
function startRound() {
    gameActive = true;
    passwordInputEl.disabled = false;
    submitButtonEl.disabled = false;
    passwordInputEl.value = ''; // Clear input
    displayFeedback(""); // Clear previous feedback
    startGameButtonEl.disabled = true; // Disable start button while round is active
    startGameButtonEl.textContent = "Start Game"; // Reset button text


    currentRound++;
    roundNumberEl.textContent = currentRound;

    // Calculate time limit for this round (using constants from C)
    currentTimeLimit = Math.max(MIN_TIME, INITIAL_TIME - (currentRound - 1) * TIME_DECREMENT);

    // Generate and display requirements
    currentRequirements = generateRequirements(currentRound);
    displayRequirements(currentRequirements);

    // Start the timer
    startTimer(currentTimeLimit);
    passwordInputEl.focus(); // Set focus to input field for immediate typing
}

/**
 * Handles the submission of the password attempt.
 */
function handleSubmit() {
    if (!gameActive) return; // Don't submit if game isn't active

    // Stop timers immediately
    clearTimeout(timerId);
    clearInterval(intervalId);

    const password = passwordInputEl.value;
    const validationResult = validatePassword(password, currentRequirements);

    if (validationResult === true) {
        // SUCCESS
        displayFeedback(`Success! Round ${currentRound} complete! Prepare for next round...`, "success");
        passwordInputEl.disabled = true;
        submitButtonEl.disabled = true;
        // Option 1: Automatically start next round after delay
        // setTimeout(startRound, 2500);

        // Option 2: Require clicking button to start next round
         startGameButtonEl.textContent = "Start Next Round";
         startGameButtonEl.disabled = false; // Re-enable start button

    } else {
        // FAILURE
        gameActive = false; // Stop game on failure
        displayFeedback(validationResult, "error"); // Show specific failure reason
        passwordInputEl.disabled = true;
        submitButtonEl.disabled = true;
        startGameButtonEl.textContent = "Play Again?";
        startGameButtonEl.disabled = false; // Allow restarting
    }
}

// --- Event Listeners ---
startGameButtonEl.addEventListener('click', () => {
    // If the button says "Start Next Round", just start it.
    // If it says "Start Game" or "Play Again", reset the round count.
    if (startGameButtonEl.textContent !== "Start Next Round") {
         currentRound = 0; // Reset for a new game
    }
    startRound();
});

submitButtonEl.addEventListener('click', handleSubmit);

// Optional: Allow submitting with Enter key in the password field
passwordInputEl.addEventListener('keypress', function (e) {
    // Check if the pressed key is Enter and the game is active
    if (e.key === 'Enter' && gameActive && !submitButtonEl.disabled) {
        handleSubmit();
    }
});

// --- Initial State Setup ---
function initializeGame() {
    roundNumberEl.textContent = '0';
    timerDisplayEl.textContent = '--';
    displayFeedback("Click 'Start Game' to begin!");
    displayRequirements({}); // Display initial empty state or instructions
    passwordInputEl.disabled = true;
    submitButtonEl.disabled = true;
    startGameButtonEl.textContent = "Start Game";
    startGameButtonEl.disabled = false;
}

initializeGame(); // Set the initial view when the script loads
