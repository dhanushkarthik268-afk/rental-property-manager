#!/usr/bin/env bash
# Automated tests for the Rental Property Manager.
# Usage (from the repository root):  bash tests/run_tests.sh
# Each test pipes scripted keyboard input into the program and checks that the
# output contains the expected lines. Expected values were computed independently
# from the CSV files (see tests/README.md).

cd "$(dirname "$0")/.." || exit 1
BIN=./rental_manager
if [ ! -x "$BIN" ]; then
  g++ -std=c++17 -Wall -Wextra -o rental_manager src/main.cpp || exit 1
fi
[ -d tests/data_large ] || python3 tests/gen_large.py > /dev/null

PASS=0; FAIL=0
# run_test <id> <description> <dataDir> <input> <expected 1> [expected 2] ...
run_test() {
  local id="$1" desc="$2" dir="$3" input="$4"; shift 4
  local out
  out=$(printf "$input" | timeout 60 "$BIN" "$dir" 2>&1)
  local ok=1 exp
  for exp in "$@"; do
    if ! grep -qF -- "$exp" <<< "$out"; then ok=0; echo "    missing: $exp"; fi
  done
  if [ $ok -eq 1 ]; then PASS=$((PASS+1)); echo "PASS  $id  $desc"; else FAIL=$((FAIL+1)); echo "FAIL  $id  $desc"; fi
}

run_test T1  "Load sample data" data '0\n' \
  "Loaded 6 properties, 126 rooms, 255 transactions."
run_test T2  "Occupancy dashboard totals" data '1\n0\n' \
  "ALL PROPERTIES                126   108    14      4   85.7%"
run_test T3  "Cash flow report, Oct 2026" data '8\n2026-10\n0\n' \
  'TOTAL                          $32,874     $4,285    $28,589'
run_test T4  "Pending rent report, Oct 2026" data '9\n2026-10\n0\n' \
  'P1-001   Sneha Kumar             $325       $0     $325  UNPAID' \
  "9 unpaid, 3 partial. Total outstanding: \$3,471"
run_test T5  "Record rent, then pending report updates" data '6\nP1-001\n325\n2026-10-28\n9\n2026-10\n0\nn\n' \
  "Recorded \$325 from Sneha Kumar (P1-001), txn 5256." \
  "8 unpaid, 3 partial. Total outstanding: \$3,146"
run_test T6  "Binary search finds first and last room" data '3\nP1-001\n3\np6-020\n0\n' \
  "P1-001   P1      \$325  Occupied" "P6-020   P6"
run_test T7  "Available rooms at rent boundary 315 / 314" data '2\n\n315\n2\n\n314\n0\n' \
  "1 vacant room(s), cheapest first:" "P6-005   P6      \$315  Vacant" "No vacant rooms match."
run_test T8  "Partial payment completed in same month" data '6\nP1-009\n173\n2026-10-29\n9\n2026-10\n0\nn\n' \
  "9 unpaid, 2 partial. Total outstanding: \$3,298"
run_test T9  "Invalid menu, month and date input" data 'abc\n42\n8\n2026-13\n2026-10\n0\n' \
  "Invalid input. Enter a whole number from 0 to 11." "Invalid month. Use YYYY-MM" "CASH FLOW REPORT FOR 2026-10"
run_test T10 "Rent for vacant room; zero and negative amounts" data '6\nP1-003\n6\nP1-001\n0\n-5\n325\n2026-02-30\n2026-10-28\n0\nn\n' \
  "Room P1-003 is not occupied; rent cannot be recorded." \
  "Invalid input. Enter a whole number from 1 to 1000000." "Invalid date. Use YYYY-MM-DD"
run_test T11 "Move in to an occupied room is rejected" data '4\nP1-001\n4\nP9-001\n0\n' \
  "Room P1-001 is Occupied, not Vacant." "Room not found."
run_test T12 "Empty dataset (headers only)" tests/data_empty '1\n9\n2026-10\n8\n2026-10\n0\n' \
  "Loaded 0 properties, 0 rooms, 0 transactions." "No occupied rooms to check for 2026-10." \
  "No transactions recorded for 2026-10."
run_test T13 "Month with no data / nothing pending" data '8\n2026-12\n9\n2026-09\n0\n' \
  "No transactions recorded for 2026-12." "All 108 occupied rooms are fully paid for 2026-09."
run_test T14 "Malformed CSV lines are skipped" tests/data_malformed '0\n' \
  "rooms.csv line 129 skipped (rent '999'" "transactions.csv line 257 skipped (amount 'twelve'" \
  "Loaded 6 properties, 126 rooms, 255 transactions."
run_test T15 "Missing data files" tests/data_missing '1\n0\n' \
  "could not open tests/data_missing/rooms.csv" "Loaded 0 properties, 0 rooms, 0 transactions."
run_test T16 "Windows (CRLF) line endings" tests/data_crlf '9\n2026-10\n0\n' \
  "Loaded 6 properties, 126 rooms, 255 transactions." "9 unpaid, 3 partial. Total outstanding: \$3,471"
run_test T17 "Larger input: 1,260 rooms, 40,997 transactions" tests/data_large '9\n2026-12\n0\n' \
  "Loaded 6 properties, 1260 rooms, 40997 transactions." "115 unpaid, 0 partial. Total outstanding: \$38,960"
run_test T18 "Input ends unexpectedly" data '1\n9\n' \
  "ALL PROPERTIES"

# T19: persistence uses a temporary copy so the real data is never changed.
TMP=$(mktemp -d); cp data/*.csv "$TMP"/
printf '4\nP1-003\nTest Tenant\n2026-11-01\n11\n0\n' | "$BIN" "$TMP" > /dev/null
run_test T19 "Save, restart, change persists" "$TMP" '3\nP1-003\n0\n' \
  "P1-003   P1      \$330  Occupied    Test Tenant         2026-11-01"
rm -rf "$TMP"

echo "----------------------------------------"
echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
