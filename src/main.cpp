// Rental Property Occupancy and Cash Flow Manager
// EECE 7205 Mini Project - Iteration 3 prototype
// Author: Dhanush Karthik Ravichandran
//
// Build: g++ -std=c++17 -Wall -Wextra -o rental_manager src/main.cpp
// Run:   ./rental_manager            (uses ./data)
//        ./rental_manager <dataDir>  (uses another folder with the same 3 CSV files)

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// ----------------------------------------------------------------------------
// Data structures
// ----------------------------------------------------------------------------
struct Property {
    string id;        // "P1" .. "P6"
    string name;
    string location;
};

struct Room {
    string id;              // "P1-007"
    string propertyId;
    long long monthlyRent;  // whole USD, 310 - 365
    string status;          // "Occupied", "Vacant", "Maintenance"
    string tenantName;      // empty when not occupied
    string moveInDate;      // YYYY-MM-DD, empty when not occupied
};

struct Transaction {
    int id;
    string date;        // YYYY-MM-DD
    string propertyId;
    string roomId;      // empty for property-level expenses
    string type;        // "RENT" or "EXPENSE"
    string category;
    long long amount;   // whole USD, > 0
    string note;
};

const long long MIN_RENT = 310;
const long long MAX_RENT = 365;
const long long MAX_AMOUNT = 1000000;  // sanity limit for a single transaction

// ----------------------------------------------------------------------------
// Small string helpers
// ----------------------------------------------------------------------------
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

string toLower(string s) {
    for (char& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

string toUpper(string s) {
    for (char& c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

// Splits one CSV line on commas and trims each field.
// (Fields in this project never contain commas; quoted fields are not supported.)
vector<string> splitCSVLine(const string& line) {
    vector<string> fields;
    string field;
    stringstream ss(line);
    while (getline(ss, field, ',')) fields.push_back(trim(field));
    if (!line.empty() && line.back() == ',') fields.push_back("");  // trailing empty field
    return fields;
}

// Parses a whole-number string. Returns false instead of throwing.
bool parseLongLong(const string& s, long long& out) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') i = 1;
    if (i == s.size()) return false;
    for (size_t j = i; j < s.size(); j++)
        if (!isdigit(static_cast<unsigned char>(s[j]))) return false;
    if (s.size() > 15) return false;  // avoid overflow
    out = stoll(s);
    return true;
}

bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// YYYY-MM-DD with a real calendar day.
bool isValidDate(const string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (!isdigit(static_cast<unsigned char>(d[i]))) return false;
    int y = stoi(d.substr(0, 4)), m = stoi(d.substr(5, 2)), day = stoi(d.substr(8, 2));
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || day < 1) return false;
    int daysIn[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxDay = daysIn[m - 1] + ((m == 2 && isLeapYear(y)) ? 1 : 0);
    return day <= maxDay;
}

// YYYY-MM
bool isValidMonth(const string& m) {
    if (m.size() != 7 || m[4] != '-') return false;
    return isValidDate(m + "-01");
}

string formatUSD(long long amount) {
    string digits = to_string(amount < 0 ? -amount : amount);
    string out;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; i--) {
        out.insert(out.begin(), digits[i]);
        if (++count % 3 == 0 && i > 0) out.insert(out.begin(), ',');
    }
    return (amount < 0 ? "-$" : "$") + out;
}

// ----------------------------------------------------------------------------
// Input helpers (all input is read line by line so bad input never jams cin)
// ----------------------------------------------------------------------------
bool inputClosed = false;

// Reads one line. On end of input, sets inputClosed and returns "".
string readLine(const string& prompt) {
    cout << prompt;
    string line;
    if (!getline(cin, line)) {
        inputClosed = true;
        cout << "\n";
        return "";
    }
    return trim(line);
}

// Re-prompts until an integer in [lo, hi] is entered. Returns lo-1 if input ends.
long long getValidInt(const string& prompt, long long lo, long long hi) {
    while (true) {
        string s = readLine(prompt);
        if (inputClosed) return lo - 1;
        long long value;
        if (parseLongLong(s, value) && value >= lo && value <= hi) return value;
        cout << "  Invalid input. Enter a whole number from " << lo << " to " << hi << ".\n";
    }
}

string getValidDate(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        if (inputClosed) return "";
        if (isValidDate(s)) return s;
        cout << "  Invalid date. Use YYYY-MM-DD (for example 2026-11-05).\n";
    }
}

string getValidMonth(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        if (inputClosed) return "";
        if (isValidMonth(s)) return s;
        cout << "  Invalid month. Use YYYY-MM (for example 2026-10).\n";
    }
}

string getNonEmpty(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        if (inputClosed) return "";
        if (!s.empty() && s.find(',') == string::npos) return s;
        cout << "  Value cannot be empty or contain commas.\n";
    }
}

// ----------------------------------------------------------------------------
// Searching
// ----------------------------------------------------------------------------
// Linear search over the (small) property list. O(P).
int findPropertyIndex(const vector<Property>& properties, const string& id) {
    for (size_t i = 0; i < properties.size(); i++)
        if (properties[i].id == id) return static_cast<int>(i);
    return -1;
}

// Binary search; rooms must be sorted by id. O(log R).
int findRoomById(const vector<Room>& rooms, const string& id) {
    int low = 0, high = static_cast<int>(rooms.size()) - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (rooms[mid].id == id) return mid;
        if (rooms[mid].id < id) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}

// Case-insensitive partial match on tenant name. O(R).
vector<int> searchByTenant(const vector<Room>& rooms, const string& text) {
    vector<int> result;
    string key = toLower(text);
    for (size_t i = 0; i < rooms.size(); i++)
        if (!rooms[i].tenantName.empty() && toLower(rooms[i].tenantName).find(key) != string::npos)
            result.push_back(static_cast<int>(i));
    return result;
}

// ----------------------------------------------------------------------------
// File loading
// ----------------------------------------------------------------------------
// Each loader returns false only if the file cannot be opened.
// Malformed lines are skipped with a warning that names the line number.

bool loadProperties(const string& filename, vector<Property>& properties) {
    ifstream in(filename);
    if (!in) return false;
    string line;
    int lineNo = 0;
    getline(in, line);  // header
    lineNo++;
    while (getline(in, line)) {
        lineNo++;
        if (trim(line).empty()) continue;
        vector<string> f = splitCSVLine(line);
        if (f.size() != 3 || f[0].empty() || f[1].empty()) {
            cout << "  Warning: " << filename << " line " << lineNo << " skipped (expected 3 fields).\n";
            continue;
        }
        if (findPropertyIndex(properties, f[0]) != -1) {
            cout << "  Warning: " << filename << " line " << lineNo << " skipped (duplicate property " << f[0] << ").\n";
            continue;
        }
        properties.push_back({f[0], f[1], f[2]});
    }
    return true;
}

bool loadRooms(const string& filename, vector<Room>& rooms, const vector<Property>& properties) {
    ifstream in(filename);
    if (!in) return false;
    string line;
    int lineNo = 0;
    getline(in, line);
    lineNo++;
    while (getline(in, line)) {
        lineNo++;
        if (trim(line).empty()) continue;
        vector<string> f = splitCSVLine(line);
        string where = "  Warning: " + filename + " line " + to_string(lineNo) + " skipped ";
        if (f.size() != 6) { cout << where << "(expected 6 fields, found " << f.size() << ").\n"; continue; }
        Room r;
        r.id = f[0];
        r.propertyId = f[1];
        r.status = f[3];
        r.tenantName = f[4];
        r.moveInDate = f[5];
        if (r.id.empty()) { cout << where << "(missing room ID).\n"; continue; }
        if (findPropertyIndex(properties, r.propertyId) == -1) { cout << where << "(unknown property " << r.propertyId << ").\n"; continue; }
        if (!parseLongLong(f[2], r.monthlyRent) || r.monthlyRent < MIN_RENT || r.monthlyRent > MAX_RENT) {
            cout << where << "(rent '" << f[2] << "' is not a number from " << MIN_RENT << " to " << MAX_RENT << ").\n";
            continue;
        }
        if (r.status != "Occupied" && r.status != "Vacant" && r.status != "Maintenance") { cout << where << "(unknown status '" << r.status << "').\n"; continue; }
        if (r.status == "Occupied" && (r.tenantName.empty() || !isValidDate(r.moveInDate))) { cout << where << "(occupied room needs a tenant and move-in date).\n"; continue; }
        rooms.push_back(r);
    }
    // Keep rooms sorted by id so findRoomById can use binary search.
    sort(rooms.begin(), rooms.end(), [](const Room& a, const Room& b) { return a.id < b.id; });
    // Remove duplicate IDs (keep the first one after sorting).
    for (size_t i = 1; i < rooms.size();) {
        if (rooms[i].id == rooms[i - 1].id) {
            cout << "  Warning: duplicate room " << rooms[i].id << " ignored.\n";
            rooms.erase(rooms.begin() + i);
        } else {
            i++;
        }
    }
    return true;
}

bool loadTransactions(const string& filename, vector<Transaction>& transactions,
                      const vector<Property>& properties, const vector<Room>& rooms) {
    ifstream in(filename);
    if (!in) return false;
    string line;
    int lineNo = 0;
    getline(in, line);
    lineNo++;
    while (getline(in, line)) {
        lineNo++;
        if (trim(line).empty()) continue;
        vector<string> f = splitCSVLine(line);
        string where = "  Warning: " + filename + " line " + to_string(lineNo) + " skipped ";
        if (f.size() != 8) { cout << where << "(expected 8 fields, found " << f.size() << ").\n"; continue; }
        Transaction t;
        long long idValue;
        if (!parseLongLong(f[0], idValue) || idValue <= 0) { cout << where << "(bad transaction ID).\n"; continue; }
        t.id = static_cast<int>(idValue);
        t.date = f[1];
        t.propertyId = f[2];
        t.roomId = f[3];
        t.type = f[4];
        t.category = f[5];
        t.note = f[7];
        if (!isValidDate(t.date)) { cout << where << "(bad date '" << t.date << "').\n"; continue; }
        if (findPropertyIndex(properties, t.propertyId) == -1) { cout << where << "(unknown property " << t.propertyId << ").\n"; continue; }
        if (t.type != "RENT" && t.type != "EXPENSE") { cout << where << "(type must be RENT or EXPENSE).\n"; continue; }
        if (t.type == "RENT" && findRoomById(rooms, t.roomId) == -1) { cout << where << "(rent for unknown room '" << t.roomId << "').\n"; continue; }
        if (!parseLongLong(f[6], t.amount) || t.amount <= 0 || t.amount > MAX_AMOUNT) { cout << where << "(amount '" << f[6] << "' is not a positive whole number).\n"; continue; }
        transactions.push_back(t);
    }
    return true;
}

// ----------------------------------------------------------------------------
// File saving
// ----------------------------------------------------------------------------
bool saveAll(const string& dir, const vector<Property>& properties, const vector<Room>& rooms,
             const vector<Transaction>& transactions) {
    ofstream p(dir + "/properties.csv"), r(dir + "/rooms.csv"), t(dir + "/transactions.csv");
    if (!p || !r || !t) return false;
    p << "propertyId,name,location\n";
    for (const Property& x : properties) p << x.id << ',' << x.name << ',' << x.location << '\n';
    r << "roomId,propertyId,monthlyRent,status,tenantName,moveInDate\n";
    for (const Room& x : rooms)
        r << x.id << ',' << x.propertyId << ',' << x.monthlyRent << ',' << x.status << ',' << x.tenantName << ',' << x.moveInDate << '\n';
    t << "txnId,date,propertyId,roomId,type,category,amount,note\n";
    for (const Transaction& x : transactions)
        t << x.id << ',' << x.date << ',' << x.propertyId << ',' << x.roomId << ',' << x.type << ','
          << x.category << ',' << x.amount << ',' << x.note << '\n';
    return true;
}

// ----------------------------------------------------------------------------
// Display
// ----------------------------------------------------------------------------
void displayRooms(const vector<Room>& list) {
    cout << "  " << left << setw(9) << "Room" << setw(6) << "Prop" << right << setw(6) << "Rent" << "  "
         << left << setw(12) << "Status" << setw(20) << "Tenant" << "Move-in\n";
    cout << "  " << string(63, '-') << "\n";
    for (const Room& r : list)
        cout << "  " << left << setw(9) << r.id << setw(6) << r.propertyId << right << setw(6) << formatUSD(r.monthlyRent)
             << "  " << left << setw(12) << r.status << setw(20) << (r.tenantName.empty() ? "-" : r.tenantName)
             << (r.moveInDate.empty() ? "-" : r.moveInDate) << "\n";
}

void showMenu() {
    cout << "\n===== Rental Property Manager =====\n"
         << "  1. Occupancy dashboard\n"
         << "  2. Available rooms\n"
         << "  3. Search (room ID or tenant name)\n"
         << "  4. Move in tenant\n"
         << "  5. Move out tenant\n"
         << "  6. Record rent payment\n"
         << "  7. Record expense          (Iteration 4)\n"
         << "  8. Monthly cash flow report\n"
         << "  9. Pending rent report\n"
         << " 10. Update room             (Iteration 4)\n"
         << " 11. Save\n"
         << "  0. Exit\n";
}

// ----------------------------------------------------------------------------
// Features
// ----------------------------------------------------------------------------
// Counts statuses per property in one pass over the rooms. O(R * P) with P = 6.
void showOccupancyDashboard(const vector<Property>& properties, const vector<Room>& rooms) {
    size_t n = properties.size();
    vector<int> occ(n, 0), vac(n, 0), maint(n, 0);
    for (const Room& r : rooms) {
        int p = findPropertyIndex(properties, r.propertyId);
        if (p == -1) continue;
        if (r.status == "Occupied") occ[p]++;
        else if (r.status == "Vacant") vac[p]++;
        else maint[p]++;
    }
    cout << "\n  OCCUPANCY DASHBOARD\n";
    cout << "  " << left << setw(5) << "ID" << setw(22) << "Property" << right << setw(6) << "Rooms" << setw(6) << "Occ"
         << setw(6) << "Vac" << setw(7) << "Maint" << setw(8) << "Occ %" << "\n";
    cout << "  " << string(60, '-') << "\n";
    int tOcc = 0, tVac = 0, tMaint = 0;
    for (size_t i = 0; i < n; i++) {
        int total = occ[i] + vac[i] + maint[i];
        double pct = total ? 100.0 * occ[i] / total : 0.0;
        cout << "  " << left << setw(5) << properties[i].id << setw(22) << properties[i].name << right << setw(6) << total
             << setw(6) << occ[i] << setw(6) << vac[i] << setw(7) << maint[i] << setw(7) << fixed << setprecision(1) << pct << "%\n";
        tOcc += occ[i]; tVac += vac[i]; tMaint += maint[i];
    }
    int all = tOcc + tVac + tMaint;
    cout << "  " << string(60, '-') << "\n";
    cout << "  " << left << setw(27) << "ALL PROPERTIES" << right << setw(6) << all << setw(6) << tOcc << setw(6) << tVac
         << setw(7) << tMaint << setw(7) << fixed << setprecision(1) << (all ? 100.0 * tOcc / all : 0.0) << "%\n";
}

// Filters vacant rooms, then sorts a COPY by rent so the main list stays sorted by id.
void listAvailableRooms(const vector<Room>& rooms, const vector<Property>& properties) {
    string prop = toUpper(readLine("  Property ID (blank = all): "));
    if (inputClosed) return;
    if (!prop.empty() && findPropertyIndex(properties, prop) == -1) {
        cout << "  No property with ID " << prop << ".\n";
        return;
    }
    long long maxRent = getValidInt("  Maximum rent (0 = no limit): ", 0, MAX_AMOUNT);
    if (inputClosed) return;
    vector<Room> result;
    for (const Room& r : rooms)
        if (r.status == "Vacant" && (prop.empty() || r.propertyId == prop) && (maxRent == 0 || r.monthlyRent <= maxRent))
            result.push_back(r);
    sort(result.begin(), result.end(), [](const Room& a, const Room& b) {
        if (a.monthlyRent != b.monthlyRent) return a.monthlyRent < b.monthlyRent;
        return a.id < b.id;
    });
    if (result.empty()) { cout << "  No vacant rooms match.\n"; return; }
    cout << "\n  " << result.size() << " vacant room(s), cheapest first:\n";
    displayRooms(result);
}

void searchMenu(const vector<Room>& rooms) {
    string key = readLine("  Enter room ID (e.g. P3-010) or part of a tenant name: ");
    if (inputClosed || key.empty()) return;
    int idx = findRoomById(rooms, toUpper(key));
    if (idx != -1) { displayRooms({rooms[idx]}); return; }
    vector<int> hits = searchByTenant(rooms, key);
    if (hits.empty()) { cout << "  Room not found and no tenant matches '" << key << "'.\n"; return; }
    vector<Room> list;
    for (int i : hits) list.push_back(rooms[i]);
    cout << "  " << list.size() << " tenant match(es):\n";
    displayRooms(list);
}

bool moveInTenant(vector<Room>& rooms) {
    string id = toUpper(readLine("  Room ID: "));
    if (inputClosed) return false;
    int idx = findRoomById(rooms, id);
    if (idx == -1) { cout << "  Room not found.\n"; return false; }
    if (rooms[idx].status != "Vacant") { cout << "  Room " << id << " is " << rooms[idx].status << ", not Vacant.\n"; return false; }
    string name = getNonEmpty("  Tenant name: ");
    if (inputClosed) return false;
    string date = getValidDate("  Move-in date (YYYY-MM-DD): ");
    if (inputClosed) return false;
    rooms[idx].status = "Occupied";
    rooms[idx].tenantName = name;
    rooms[idx].moveInDate = date;
    cout << "  " << name << " moved in to " << id << " (rent " << formatUSD(rooms[idx].monthlyRent) << "/month).\n";
    return true;
}

bool moveOutTenant(vector<Room>& rooms) {
    string id = toUpper(readLine("  Room ID: "));
    if (inputClosed) return false;
    int idx = findRoomById(rooms, id);
    if (idx == -1) { cout << "  Room not found.\n"; return false; }
    if (rooms[idx].status != "Occupied") { cout << "  Room " << id << " is not occupied.\n"; return false; }
    cout << "  " << rooms[idx].tenantName << " moved out of " << id << ". Room is now Vacant.\n";
    rooms[idx].status = "Vacant";
    rooms[idx].tenantName = "";
    rooms[idx].moveInDate = "";
    return true;
}

int nextTransactionId(const vector<Transaction>& transactions) {
    int maxId = 5000;
    for (const Transaction& t : transactions) maxId = max(maxId, t.id);
    return maxId + 1;
}

bool recordRentPayment(const vector<Room>& rooms, vector<Transaction>& transactions) {
    string id = toUpper(readLine("  Room ID: "));
    if (inputClosed) return false;
    int idx = findRoomById(rooms, id);
    if (idx == -1) { cout << "  Room not found.\n"; return false; }
    if (rooms[idx].status != "Occupied") { cout << "  Room " << id << " is not occupied; rent cannot be recorded.\n"; return false; }
    long long amount = getValidInt("  Amount paid (USD): ", 1, MAX_AMOUNT);
    if (inputClosed) return false;
    string date = getValidDate("  Payment date (YYYY-MM-DD): ");
    if (inputClosed) return false;
    Transaction t{nextTransactionId(transactions), date, rooms[idx].propertyId, id, "RENT", "Rent", amount, "Rent payment"};
    transactions.push_back(t);
    cout << "  Recorded " << formatUSD(amount) << " from " << rooms[idx].tenantName << " (" << id << "), txn " << t.id << ".\n";
    return true;
}

// CORE OPERATION 1: monthly cash flow per property.
// One pass over transactions; property lookup is linear over P = 6. O(T * P) time, O(P) space.
void monthlyCashFlowReport(const vector<Property>& properties, const vector<Transaction>& transactions, const string& month) {
    size_t n = properties.size();
    vector<long long> income(n, 0), expense(n, 0);
    int matched = 0;
    for (const Transaction& t : transactions) {
        if (t.date.substr(0, 7) != month) continue;
        int p = findPropertyIndex(properties, t.propertyId);
        if (p == -1) continue;
        matched++;
        if (t.type == "RENT") income[p] += t.amount;
        else expense[p] += t.amount;
    }
    cout << "\n  CASH FLOW REPORT FOR " << month << "\n";
    cout << "  " << left << setw(5) << "ID" << setw(22) << "Property" << right << setw(11) << "Income" << setw(11) << "Expenses"
         << setw(11) << "Net" << "\n";
    cout << "  " << string(60, '-') << "\n";
    long long tIn = 0, tOut = 0;
    for (size_t i = 0; i < n; i++) {
        cout << "  " << left << setw(5) << properties[i].id << setw(22) << properties[i].name << right << setw(11) << formatUSD(income[i])
             << setw(11) << formatUSD(expense[i]) << setw(11) << formatUSD(income[i] - expense[i]) << "\n";
        tIn += income[i];
        tOut += expense[i];
    }
    cout << "  " << string(60, '-') << "\n";
    cout << "  " << left << setw(27) << "TOTAL" << right << setw(11) << formatUSD(tIn) << setw(11) << formatUSD(tOut)
         << setw(11) << formatUSD(tIn - tOut) << "\n";
    if (matched == 0) cout << "  No transactions recorded for " << month << ".\n";
}

// CORE OPERATION 2 (approach 1, nested loops): pending rent per occupied room.
// For each occupied room, scan every transaction and add up the rent paid in the month.
// O(R * T) time, O(R) extra space for the result list.
void pendingRentReport(const vector<Room>& rooms, const vector<Transaction>& transactions, const string& month) {
    cout << "\n  PENDING RENT FOR " << month << "\n";
    cout << "  " << left << setw(9) << "Room" << setw(20) << "Tenant" << right << setw(8) << "Rent" << setw(9) << "Paid"
         << setw(9) << "Due" << "  Status\n";
    cout << "  " << string(64, '-') << "\n";
    int unpaid = 0, partial = 0, checked = 0;
    long long totalDue = 0;
    for (const Room& r : rooms) {
        if (r.status != "Occupied") continue;
        if (r.moveInDate.substr(0, 7) > month) continue;  // tenant had not moved in yet
        checked++;
        long long paid = 0;
        for (const Transaction& t : transactions)
            if (t.type == "RENT" && t.roomId == r.id && t.date.substr(0, 7) == month) paid += t.amount;
        if (paid >= r.monthlyRent) continue;
        long long due = r.monthlyRent - paid;
        string status = paid == 0 ? "UNPAID" : "PARTIAL";
        if (paid == 0) unpaid++; else partial++;
        totalDue += due;
        cout << "  " << left << setw(9) << r.id << setw(20) << r.tenantName << right << setw(8) << formatUSD(r.monthlyRent)
             << setw(9) << formatUSD(paid) << setw(9) << formatUSD(due) << "  " << status << "\n";
    }
    cout << "  " << string(64, '-') << "\n";
    // Bug fix (found by test T12): with no occupied rooms, the old code printed
    // "All occupied rooms are fully paid", which is misleading.
    if (checked == 0) cout << "  No occupied rooms to check for " << month << ".\n";
    else if (unpaid + partial == 0) cout << "  All " << checked << " occupied rooms are fully paid for " << month << ".\n";
    else cout << "  " << unpaid << " unpaid, " << partial << " partial. Total outstanding: " << formatUSD(totalDue) << "\n";
}

// ----------------------------------------------------------------------------
// Main program flow
// ----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    string dataDir = argc > 1 ? argv[1] : "data";
    vector<Property> properties;
    vector<Room> rooms;
    vector<Transaction> transactions;

    cout << "Loading data from " << dataDir << "/ ...\n";
    if (!loadProperties(dataDir + "/properties.csv", properties))
        cout << "  Warning: could not open " << dataDir << "/properties.csv. Starting with no properties.\n";
    if (!loadRooms(dataDir + "/rooms.csv", rooms, properties))
        cout << "  Warning: could not open " << dataDir << "/rooms.csv. Starting with no rooms.\n";
    if (!loadTransactions(dataDir + "/transactions.csv", transactions, properties, rooms))
        cout << "  Warning: could not open " << dataDir << "/transactions.csv. Starting with no transactions.\n";
    cout << "Loaded " << properties.size() << " properties, " << rooms.size() << " rooms, "
         << transactions.size() << " transactions.\n";

    bool unsaved = false;
    while (true) {
        showMenu();
        long long choice = getValidInt("Choose an option: ", 0, 11);
        if (inputClosed) break;
        string month;
        switch (choice) {
            case 1: showOccupancyDashboard(properties, rooms); break;
            case 2: listAvailableRooms(rooms, properties); break;
            case 3: searchMenu(rooms); break;
            case 4: if (moveInTenant(rooms)) unsaved = true; break;
            case 5: if (moveOutTenant(rooms)) unsaved = true; break;
            case 6: if (recordRentPayment(rooms, transactions)) unsaved = true; break;
            case 7: case 10: cout << "  This option is planned for Iteration 4.\n"; break;
            case 8:
                month = getValidMonth("  Month (YYYY-MM): ");
                if (!inputClosed) monthlyCashFlowReport(properties, transactions, month);
                break;
            case 9:
                month = getValidMonth("  Month (YYYY-MM): ");
                if (!inputClosed) pendingRentReport(rooms, transactions, month);
                break;
            case 11:
                if (saveAll(dataDir, properties, rooms, transactions)) { cout << "  Saved to " << dataDir << "/.\n"; unsaved = false; }
                else cout << "  Error: could not write files in " << dataDir << "/.\n";
                break;
            case 0:
                if (unsaved) {
                    string ans = toLower(readLine("  You have unsaved changes. Save before exiting? (y/n): "));
                    if (ans == "y" || ans == "yes") {
                        if (saveAll(dataDir, properties, rooms, transactions)) cout << "  Saved.\n";
                        else cout << "  Error: could not save.\n";
                    }
                }
                cout << "Goodbye.\n";
                return 0;
        }
        if (inputClosed) break;
    }
    if (unsaved) cout << "Input ended with unsaved changes; nothing was written.\n";
    return 0;
}
