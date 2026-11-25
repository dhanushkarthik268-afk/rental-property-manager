# Independent expected-value calculator (not part of the C++ program).
# Usage: python3 tests/expected_values.py <dataDir> <YYYY-MM>
import csv, sys
d = sys.argv[1]; month = sys.argv[2]
props = list(csv.DictReader(open(f"{d}/properties.csv")))
rooms = list(csv.DictReader(open(f"{d}/rooms.csv")))
tx = list(csv.DictReader(open(f"{d}/transactions.csv")))
st = {s: sum(r["status"] == s for r in rooms) for s in ("Occupied", "Vacant", "Maintenance")}
print("status", st, "occ%", round(100*st["Occupied"]/len(rooms), 1))
inc = sum(int(t["amount"]) for t in tx if t["date"][:7] == month and t["type"] == "RENT")
exp = sum(int(t["amount"]) for t in tx if t["date"][:7] == month and t["type"] == "EXPENSE")
print("cashflow", inc, exp, inc - exp)
unpaid = partial = due = 0
for r in rooms:
    if r["status"] != "Occupied" or r["moveInDate"][:7] > month: continue
    paid = sum(int(t["amount"]) for t in tx if t["type"] == "RENT" and t["roomId"] == r["roomId"] and t["date"][:7] == month)
    if paid < int(r["monthlyRent"]):
        due += int(r["monthlyRent"]) - paid
        if paid == 0: unpaid += 1
        else: partial += 1
print("pending", unpaid, partial, due)
vac = sorted((int(r["monthlyRent"]), r["roomId"]) for r in rooms if r["status"] == "Vacant")
print("vacant", vac)
