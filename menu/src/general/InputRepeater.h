/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

/**
 * General-purpose input repeater. Fires once on press, then (after repeatDelay
 * frames of being held) repeats every repeatInterval frames. Works for both
 * single buttons and directional inputs (button pairs, dpad, analog axes),
 * unifying the old ButtonRepeater and JoystickRepeater.
 */
struct InputRepeater {
    enum Type {
        C_UP_DOWN,
        C_LEFT_RIGHT,
        JOYSTICK_UP_DOWN,
        JOYSTICK_LEFT_RIGHT,
        DPAD_UP_DOWN,
        DPAD_LEFT_RIGHT,
        L_R
    };

    // Which input this repeater reads (used by update(joypad_inputs_t)).
    Type type = JOYSTICK_UP_DOWN;

    int deadzone = 6;
    int repeatDelay = 16;
    int repeatInterval = 4;

    int holdFrames = 0;
    int lastDirection = 0;   // -1, 0 or +1 (0/1 when used as a button)
    bool isRepeating = false;

    // True while an input is held (direction != 0). Updated every step()/update().
    bool isEngaged = false;

    // Map the raw joypad state to a direction (+1 = up/right, -1 = down/left).
    int directionFor(const joypad_inputs_t& joypad) const {
        switch (type) {
            case C_UP_DOWN:
                if (joypad.btn.c_up)   return +1;
                if (joypad.btn.c_down) return -1;
                return 0;
            case C_LEFT_RIGHT:
                if (joypad.btn.c_right) return +1;
                if (joypad.btn.c_left)  return -1;
                return 0;
            case DPAD_UP_DOWN:
                if (joypad.btn.d_up)   return +1;
                if (joypad.btn.d_down) return -1;
                return 0;
            case DPAD_LEFT_RIGHT:
                if (joypad.btn.d_right) return +1;
                if (joypad.btn.d_left)  return -1;
                return 0;
            case JOYSTICK_UP_DOWN:
                if (joypad.stick_y >  deadzone) return +1;
                if (joypad.stick_y < -deadzone) return -1;
                return 0;
            case JOYSTICK_LEFT_RIGHT:
                if (joypad.stick_x >  deadzone) return +1;
                if (joypad.stick_x < -deadzone) return -1;
                return 0;
            case L_R:
                if (joypad.btn.r) return +1;
                if (joypad.btn.l) return -1;
                return 0;
        }

        return 0;
    }

    // Core repeat logic. `direction` is -1/0/+1 (or 0/1 for a button).
    // Returns the direction that fired this frame, or 0 for no fire.
    int step(int direction) {
        isEngaged = (direction != 0);

        if (direction != lastDirection) {
            holdFrames = 0;
            lastDirection = direction;
        }

        if (direction == 0) {
            isRepeating = false;
            return 0;
        }

        holdFrames++;
        isRepeating = holdFrames > repeatDelay;

        if (holdFrames == 1) {
            return direction;
        }

        if (holdFrames > repeatDelay && (holdFrames % repeatInterval) == 0) {
            return direction;
        }

        return 0;
    }

    // Directional update from a full joypad, using `type`.
    int update(const joypad_inputs_t& joypad) {
        return step(directionFor(joypad));
    }

    // Button update: fires true on press then repeats.
    bool update(bool isDown) {
        return step(isDown ? 1 : 0) != 0;
    }
};

/**
 * Edge detector for a single button. update() reports the transition observed
 * this frame: BUTTON_DOWN (a press), BUTTON_UP (a release), or UNCHANGED.
 *
 * All events are suppressed until a clean release has been seen. This discards a
 * button that is already held on entry -- e.g. the press that navigated into the
 * current scene -- along with the release that ends that hold, so reporting only
 * begins once the user has let go and can press the button afresh.
 */
struct InputWatcher {
    enum Event {
        UNCHANGED,
        BUTTON_DOWN,   // rising edge:  released -> pressed
        BUTTON_UP      // falling edge: pressed  -> released
    };

    // True once a release has been observed; until then nothing is reported.
    bool armed = false;

    // Last pressed state (only meaningful once armed).
    bool lastDown = false;

    void reset() {
        armed = false;
        lastDown = false;
    }

    Event update(bool isDown) {
        // Wait for the button to be released before reporting anything, ignoring
        // any hold carried in from a previous context and its trailing release.
        if (!armed) {
            if (!isDown) {
                armed = true;
            }
            return UNCHANGED;
        }

        Event event = UNCHANGED;

        if (isDown && !lastDown) {
            event = BUTTON_DOWN;
        }
        else if (!isDown && lastDown) {
            event = BUTTON_UP;
        }

        lastDown = isDown;
        return event;
    }
};
