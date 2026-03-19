#include "tests/TestHarness.hpp"

#include "ui/saves/Sidebar.hpp"

using ui::saves::Sidebar;

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
