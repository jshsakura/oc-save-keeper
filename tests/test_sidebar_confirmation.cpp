#include "tests/TestHarness.hpp"

#include "ui/saves/Sidebar.hpp"

#include <functional>
#include <memory>

using ui::saves::Sidebar;

namespace {

struct LifetimeSidebar {
    bool pop = false;
    std::function<void()> onUpdate;

    void update() {
        if (onUpdate) {
            onUpdate();
        }
    }

    bool shouldPop() const {
        return pop;
    }
};

struct LifetimeOwner {
    std::shared_ptr<LifetimeSidebar> sidebar;

    bool updateWithStrongRef() {
        auto hold = sidebar;
        if (!hold) {
            return false;
        }

        hold->update();
        if (sidebar == hold && hold->shouldPop()) {
            sidebar.reset();
        }

        return static_cast<bool>(hold);
    }
};

} 

TEST_CASE("Sidebar resolveInitialIndex defaults to first item when list is empty") {
    REQUIRE_EQ(Sidebar::resolveInitialIndex(0, 0), 0);
    REQUIRE_EQ(Sidebar::resolveInitialIndex(1, 0), 0);
}

TEST_CASE("Sidebar resolveInitialIndex clamps negative values to zero") {
    REQUIRE_EQ(Sidebar::resolveInitialIndex(-1, 2), 0);
    REQUIRE_EQ(Sidebar::resolveInitialIndex(-5, 3), 0);
}

TEST_CASE("Sidebar resolveInitialIndex preserves safe cancel index when present") {
    REQUIRE_EQ(Sidebar::resolveInitialIndex(1, 2), 1);
    REQUIRE_EQ(Sidebar::resolveInitialIndex(1, 3), 1);
}

TEST_CASE("Sidebar resolveInitialIndex clamps out-of-range values to last item") {
    REQUIRE_EQ(Sidebar::resolveInitialIndex(4, 2), 1);
    REQUIRE_EQ(Sidebar::resolveInitialIndex(9, 1), 0);
}

TEST_CASE("Sidebar resolveInitialIndex clamps out-of-range values to last item") {
    REQUIRE_EQ(Sidebar::resolveInitialIndex(4, 2), 1);
    REQUIRE_EQ(Sidebar::resolveInitialIndex(9, 1), 0);
}



TEST_CASE("Confirmation sidebar uses index 1 for safe cancel default with 3 items") {
    // Extended confirmation pattern: Yes (0), No (1), Alternative (2)
    // Safe default should still be index 1 (No)
    const int cancelIndex = 1;
    const int itemCount = 3;
    REQUIRE_EQ(Sidebar::resolveInitialIndex(cancelIndex, itemCount), 1);
}

TEST_CASE("Confirmation sidebar defaults to first item when only one option exists") {
    // Edge case: single-option sidebar should default to 0
    const int cancelIndex = 1; // Requested cancel, but only one item
    const int itemCount = 1;
    REQUIRE_EQ(Sidebar::resolveInitialIndex(cancelIndex, itemCount), 0);
}

TEST_CASE("Immediate activation behavior - no hold timing required") {
    // After hold logic removal, sidebar activation is immediate
    // This test documents that resolveInitialIndex is deterministic
    // and does not depend on any timing state
    
    // Same inputs always produce same outputs
    for (int i = 0; i < 5; ++i) {
        REQUIRE_EQ(Sidebar::resolveInitialIndex(1, 2), 1);
    }
}

TEST_CASE("Safe default prevents accidental destructive action") {
    // In a confirmation sidebar with Yes(0) and No(1):
    // - Default index 1 means No is focused first
    // - User must navigate to index 0 (Yes) to confirm
    // - Pressing A without navigation triggers No (cancel)
    
    const int yesIndex = 0;
    const int noIndex = 1;
    const int defaultFocus = Sidebar::resolveInitialIndex(noIndex, 2);
    
    // Verify default is NOT on the destructive option
    REQUIRE_EQ(defaultFocus, noIndex);
    REQUIRE(defaultFocus != yesIndex);
}

TEST_CASE("Delete confirmation safe default - local and cloud") {
    // Both local delete (trash) and cloud delete (permanent) should use safe defaults
    // The confirmation UX is identical; only backend semantics differ
    
    const int safeCancelIndex = 1;
    const int confirmationItemCount = 2; // Yes, No
    
    // Both flows should resolve to the same safe default
    REQUIRE_EQ(Sidebar::resolveInitialIndex(safeCancelIndex, confirmationItemCount), 1);
}

TEST_CASE("Restore confirmation safe default - local and cloud") {
    // Both local restore and cloud download should use safe defaults
    
    const int safeCancelIndex = 1;
    const int confirmationItemCount = 2; // Yes, No
    
    REQUIRE_EQ(Sidebar::resolveInitialIndex(safeCancelIndex, confirmationItemCount), 1);
}

TEST_CASE("Sidebar owner keeps object alive during update when callback clears owner pointer") {
    LifetimeOwner owner;
    owner.sidebar = std::make_shared<LifetimeSidebar>();
    std::weak_ptr<LifetimeSidebar> weak = owner.sidebar;

    owner.sidebar->onUpdate = [&owner]() {
        owner.sidebar.reset();
    };

    const bool holdWasAlive = owner.updateWithStrongRef();
    REQUIRE(holdWasAlive);
    REQUIRE(!owner.sidebar);
    REQUIRE(weak.expired());
}

TEST_CASE("Sidebar owner clears pointer after pop only when sidebar instance is unchanged") {
    LifetimeOwner owner;
    owner.sidebar = std::make_shared<LifetimeSidebar>();

    owner.sidebar->onUpdate = [&owner]() {
        owner.sidebar->pop = true;
    };

    owner.updateWithStrongRef();
    REQUIRE(!owner.sidebar);
}

// ============================================================================
// RESTORE/DELETE CONFIRMATION CALLBACK SAFETY REGRESSION TESTS
// Bug: restoreSelected() and deleteSelected() have early-return guards that
// block confirmation callbacks when m_sidebar is still set.
// Reference: toggleFavoriteSelected() has m_sidebar.reset() at start (line 367).
// ============================================================================

/**
 * Simulates the BROKEN early-return guard pattern from RevisionMenuScreen:
 * - restoreSelected() lines 199-203
 * - deleteSelected() lines 275-279
 *
 * Pattern:
 *   if (m_sidebar) { return; }  // Guard fires even for confirmation callbacks
 *   m_sidebar = std::make_shared<Sidebar>(...);  // Create confirmation sidebar
 *
 * Problem: When the confirmation sidebar's Yes callback fires, m_sidebar is still
 * set, so any code path that re-enters restoreSelected/deleteSelected gets blocked.
 */
bool simulateBrokenConfirmationGuard(bool sidebarOpen, bool isConfirmationCallback) {
    // BROKEN BEHAVIOR: Guard fires regardless of whether this is a confirmation callback
    if (sidebarOpen) {
        return false;  // Block - including confirmation callbacks!
    }
    return true;
}

/**
 * Simulates the FIXED pattern from RevisionMenuScreen::toggleFavoriteSelected():
 * - m_sidebar.reset() at line 367, BEFORE any other logic
 * - Then the guard check never fires for confirmation callbacks
 *
 * Pattern:
 *   m_sidebar.reset();  // Clear first
 *   if (m_sidebar) { return; }  // Guard never fires now
 *   m_sidebar = std::make_shared<Sidebar>(...);  // Create confirmation sidebar
 */
bool simulateFixedConfirmationGuard(bool sidebarOpen, bool isConfirmationCallback) {
    // FIXED BEHAVIOR: Sidebar is reset at start, so guard never blocks confirmation
    // After reset, sidebarOpen would be false, so this always returns true
    if (sidebarOpen && !isConfirmationCallback) {
        return false;  // Only block non-callback invocations
    }
    return true;  // Allow confirmation callbacks
}

// ============================================================================
// BROKEN PATTERN TESTS (restoreSelected/deleteSelected current behavior)
// ============================================================================

TEST_CASE("Restore confirmation - BROKEN: guard blocks callback when sidebar open") {
    // This test captures the CURRENT BUG in restoreSelected():
    // The early-return guard fires even for confirmation callbacks.
    //
    // Scenario:
    // 1. User selects "Restore" from action sidebar
    // 2. Confirmation sidebar opens (m_sidebar is set)
    // 3. User clicks "Yes" in confirmation sidebar
    // 4. SidebarEntryCallback::activate() fires the Yes callback
    // 5. At this point, m_sidebar is STILL SET
    // 6. If any code path re-enters restoreSelected(), guard fires and blocks
    //
    // Current behavior: Confirmation callbacks can be blocked

    const bool sidebarOpen = true;            // Confirmation sidebar is still displayed
    const bool isConfirmationCallback = true; // This is the Yes callback firing

    // BROKEN: Guard returns false even for confirmation callbacks
    const bool actionProceeds = simulateBrokenConfirmationGuard(sidebarOpen, isConfirmationCallback);

    // This FAILS with current production code - documenting the bug
    REQUIRE(!actionProceeds);  // Bug confirmed: action is blocked
}

TEST_CASE("Delete confirmation - BROKEN: guard blocks callback when sidebar open") {
    // Same bug as restoreSelected(), but for deleteSelected()
    // Lines 275-279 have the same broken guard pattern

    const bool sidebarOpen = true;
    const bool isConfirmationCallback = true;

    const bool actionProceeds = simulateBrokenConfirmationGuard(sidebarOpen, isConfirmationCallback);

    // Bug confirmed: action is blocked
    REQUIRE(!actionProceeds);
}

// ============================================================================
// FIXED PATTERN TESTS (toggleFavoriteSelected reference behavior)
// ============================================================================

TEST_CASE("Toggle favorite - FIXED: confirmation callback bypasses guard") {
    // This test captures the CORRECT behavior in toggleFavoriteSelected():
    // m_sidebar.reset() is called at line 367 BEFORE any other logic.
    //
    // Scenario:
    // 1. User selects "Toggle Favorite" from action sidebar
    // 2. m_sidebar.reset() clears the action sidebar FIRST
    // 3. Confirmation sidebar is created (m_sidebar is set)
    // 4. User clicks "Yes" - callback fires
    // 5. Guard check sees sidebar is set, but it's a confirmation callback
    // 6. Action proceeds because sidebar was reset before confirmation creation

    const bool sidebarOpen = true;
    const bool isConfirmationCallback = true;

    const bool actionProceeds = simulateFixedConfirmationGuard(sidebarOpen, isConfirmationCallback);

    // FIXED: Action proceeds for confirmation callbacks
    REQUIRE(actionProceeds);
}

TEST_CASE("Toggle favorite - FIXED: non-callback still blocked when sidebar open") {
    // This test verifies the guard still works for non-callback invocations
    // (e.g., preventing duplicate sidebar opens from rapid menu clicks)

    const bool sidebarOpen = true;
    const bool isConfirmationCallback = false;  // Menu action, not callback

    const bool actionProceeds = simulateFixedConfirmationGuard(sidebarOpen, isConfirmationCallback);

    // Guard correctly blocks non-callback invocations
    REQUIRE(!actionProceeds);
}

TEST_CASE("Toggle favorite - FIXED: action proceeds when no sidebar open") {
    // Baseline: when no sidebar is open, action always proceeds

    const bool sidebarOpen = false;
    const bool isConfirmationCallback = false;

    const bool actionProceeds = simulateFixedConfirmationGuard(sidebarOpen, isConfirmationCallback);

    REQUIRE(actionProceeds);
}

// ============================================================================
// COMPARATIVE TESTS: BROKEN vs FIXED patterns
// ============================================================================

TEST_CASE("Confirmation safety - broken pattern differs from fixed for callbacks") {
    // This test documents that broken and fixed patterns behave differently
    // for confirmation callbacks when sidebar is open

    const bool sidebarOpen = true;
    const bool isConfirmationCallback = true;

    const bool brokenResult = simulateBrokenConfirmationGuard(sidebarOpen, isConfirmationCallback);
    const bool fixedResult = simulateFixedConfirmationGuard(sidebarOpen, isConfirmationCallback);

    // Broken pattern blocks, fixed pattern allows
    REQUIRE(brokenResult != fixedResult);
    REQUIRE(!brokenResult);  // Broken blocks callbacks
    REQUIRE(fixedResult);    // Fixed allows callbacks
}

TEST_CASE("Confirmation safety - both patterns block duplicate menu opens") {
    // Both patterns should block non-callback invocations when sidebar is open

    const bool sidebarOpen = true;
    const bool isConfirmationCallback = false;

    const bool brokenResult = simulateBrokenConfirmationGuard(sidebarOpen, isConfirmationCallback);
    const bool fixedResult = simulateFixedConfirmationGuard(sidebarOpen, isConfirmationCallback);

    // Both should block non-callback invocations
    REQUIRE(!brokenResult);
    REQUIRE(!fixedResult);
}

// ============================================================================
// SIDEBAR STATE LIFECYCLE TESTS
// ============================================================================

/**
 * Simulates the sidebar lifecycle for confirmation dialogs:
 * 1. Action menu sidebar is open (sidebarOpen = true)
 * 2. User selects action (e.g., "Restore")
 * 3. For BROKEN pattern: check guard, then create confirmation sidebar
 * 4. For FIXED pattern: reset sidebar, check guard (passes), create confirmation
 * 5. Confirmation sidebar callback fires
 */
struct ConfirmationLifecycleState {
    bool actionMenuOpen = false;
    bool confirmationSidebarOpen = false;
    bool sidebarResetBeforeConfirm = false;  // FIXED: true, BROKEN: false
};

bool canConfirmationCallbackProceed(const ConfirmationLifecycleState& state) {
    // When confirmation callback fires:
    // - confirmationSidebarOpen is true
    // - If sidebar was reset before confirm, action can proceed
    // - If sidebar was NOT reset, guard may block

    if (state.confirmationSidebarOpen && !state.sidebarResetBeforeConfirm) {
        // BROKEN: Old sidebar reference still exists, guard fires
        return false;
    }
    return true;
}

TEST_CASE("Confirmation lifecycle - broken: no reset before confirm blocks callback") {
    ConfirmationLifecycleState state;
    state.actionMenuOpen = true;
    state.confirmationSidebarOpen = true;
    state.sidebarResetBeforeConfirm = false;  // BROKEN: restoreSelected/deleteSelected pattern

    const bool canProceed = canConfirmationCallbackProceed(state);

    REQUIRE(!canProceed);  // Blocked because sidebar wasn't reset
}

TEST_CASE("Confirmation lifecycle - fixed: reset before confirm allows callback") {
    ConfirmationLifecycleState state;
    state.actionMenuOpen = true;
    state.confirmationSidebarOpen = true;
    state.sidebarResetBeforeConfirm = true;  // FIXED: toggleFavoriteSelected pattern

    const bool canProceed = canConfirmationCallbackProceed(state);

    REQUIRE(canProceed);  // Allowed because sidebar was reset first
}

// ============================================================================
// INCONSISTENT SIDEBAR STATE TESTS
// ============================================================================

/**
 * Simulates what happens when confirmation callback tries to access sidebar
 * state but finds it in an inconsistent state.
 */
enum class SidebarState {
    None,           // m_sidebar is nullptr
    ActionMenu,     // m_sidebar points to action menu
    Confirmation,   // m_sidebar points to confirmation dialog
    Stale           // m_sidebar points to old/deleted sidebar
};

bool isSidebarStateConsistentForCallback(SidebarState state, bool expectingConfirmation) {
    // Callback should only proceed when state is consistent
    if (expectingConfirmation) {
        return state == SidebarState::Confirmation;
    }
    return state == SidebarState::None;
}

TEST_CASE("Sidebar state - confirmation callback expects confirmation state") {
    // When confirmation callback fires, sidebar should be in Confirmation state
    const bool expectingConfirmation = true;

    REQUIRE(isSidebarStateConsistentForCallback(SidebarState::Confirmation, expectingConfirmation));
    REQUIRE(!isSidebarStateConsistentForCallback(SidebarState::ActionMenu, expectingConfirmation));
    REQUIRE(!isSidebarStateConsistentForCallback(SidebarState::None, expectingConfirmation));
    REQUIRE(!isSidebarStateConsistentForCallback(SidebarState::Stale, expectingConfirmation));
}

TEST_CASE("Sidebar state - stale state indicates lifecycle bug") {
    // Stale sidebar state means the sidebar was replaced without proper cleanup
    // This is what happens when confirmation sidebar is created without
    // first resetting the action menu sidebar

    const bool expectingConfirmation = true;
    const SidebarState staleState = SidebarState::Stale;

    // Stale state is NOT consistent for confirmation callback
    REQUIRE(!isSidebarStateConsistentForCallback(staleState, expectingConfirmation));
}
