#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../DisplayView.h"

// Minimal stand-in for the display view. The production struct carries icon and
// route state too, but the cascade rule only cares about these text rows.
struct TestLeftColumnView {
  std::string title;
  std::string line1;
  std::string line2;
  std::string position;
};

// Empty strings are how the production helper removes duplicate rows. The UI
// then naturally pulls later rows upward because row height is based on visible
// text only.
static std::vector<std::string> visibleRows(const TestLeftColumnView& v) {
  std::vector<std::string> rows;
  if (!v.title.empty()) rows.push_back(v.title);
  if (!v.line1.empty()) rows.push_back(v.line1);
  if (!v.line2.empty()) rows.push_back(v.line2);
  if (!v.position.empty()) rows.push_back(v.position);
  return rows;
}

static void expectRows(const char* name, TestLeftColumnView v, std::vector<std::string> expected) {
  cascadeDuplicateLines(v, sameAircraftText);
  std::vector<std::string> actual = visibleRows(v);
  if (actual == expected) return;

  std::cerr << "FAIL " << name << "\nexpected:";
  for (const std::string& row : expected) std::cerr << " [" << row << "]";
  std::cerr << "\nactual:  ";
  for (const std::string& row : actual) std::cerr << " [" << row << "]";
  std::cerr << "\n";
  std::exit(1);
}

int main() {
  // The original bug: when the title and first detail line were identical, the
  // display showed the same text twice instead of cascading the remaining rows.
  expectRows(
    "first two identical rows cascade",
    { "A20N", "A20N", "Lufthansa", "FL330" },
    { "A20N", "Lufthansa", "FL330" });

  // The retained-aircraft screen had its own duplicate check. This keeps that
  // behavior covered now that the rule lives in one shared helper.
  expectRows(
    "line1 and line2 identical rows cascade",
    { "Aircraft", "DLH4JA", "DLH4JA", "FL330" },
    { "Aircraft", "DLH4JA", "FL330" });

  // Duplicate suppression should not depend on the rows being neighbors.
  expectRows(
    "non-adjacent duplicate rows cascade",
    { "DLH4JA", "D-AINZ", "Lufthansa", "DLH4JA" },
    { "DLH4JA", "D-AINZ", "Lufthansa" });

  // Match the sketch's aircraft-text normalization so harmless case/spacing
  // differences do not leak through as duplicate display rows.
  expectRows(
    "case and spacing normalized like sketch",
    { "A 20N", "a20n", "DLH4JA", "FL330" },
    { "A 20N", "DLH4JA", "FL330" });

  // Guard against the helper getting too aggressive and hiding useful rows.
  expectRows(
    "distinct rows stay visible",
    { "A20N", "DLH4JA", "Lufthansa", "FL330" },
    { "A20N", "DLH4JA", "Lufthansa", "FL330" });

  std::cout << "left column cascade tests passed\n";
  return 0;
}
