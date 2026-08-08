/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

enum ResultStatus {
    SUCCEEDED,
    CANCELLED
};

/**
 * Outcome of an operation that can either complete with a value or be
 * cancelled.
 */
template <typename T>
struct Result {
    ResultStatus status = CANCELLED;
    T value = {};

    static Result succeeded(T value) {
        return { SUCCEEDED, value };
    }

    static Result cancelled() {
        return { CANCELLED, {} };
    }

    bool didSucceed() const { return status == SUCCEEDED; }
    bool didCancel()  const { return status == CANCELLED; }
};
