"""Generates tests/data_large: 6 properties x 210 rooms = 1,260 rooms and
36 months of rent and expenses (about 41,000 transactions). Deterministic (seeded)."""
import os, random
random.seed(42)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data_large")
os.makedirs(out, exist_ok=True)
props = [f"P{i}" for i in range(1, 7)]
with open(f"{out}/properties.csv", "w") as f:
    f.write("propertyId,name,location\n")
    for p in props: f.write(f"{p},Property {p},Area {p}\n")
rooms = []
with open(f"{out}/rooms.csv", "w") as f:
    f.write("roomId,propertyId,monthlyRent,status,tenantName,moveInDate\n")
    for p in props:
        for n in range(1, 211):
            rid, rent = f"{p}-{n:03d}", random.randrange(310, 366, 5)
            st = random.choices(["Occupied", "Vacant", "Maintenance"], [90, 8, 2])[0]
            if st == "Occupied":
                f.write(f"{rid},{p},{rent},Occupied,Tenant {p}{n:03d},2024-01-01\n"); rooms.append((rid, p, rent))
            else:
                f.write(f"{rid},{p},{rent},{st},,\n")
months = [f"{y}-{m:02d}" for y in (2024, 2025, 2026) for m in range(1, 13)]
tid = 1
with open(f"{out}/transactions.csv", "w") as f:
    f.write("txnId,date,propertyId,roomId,type,category,amount,note\n")
    for m in months:
        for rid, p, rent in rooms:
            if m == "2026-12" and random.random() < 0.1: continue   # some unpaid in the last month
            f.write(f"{tid},{m}-05,{p},{rid},RENT,Rent,{rent},rent\n"); tid += 1
        for p in props:
            f.write(f"{tid},{m}-20,{p},,EXPENSE,Utilities,{random.randrange(300, 900)},util\n"); tid += 1
print(f"rooms={len(rooms)} occupied, transactions={tid - 1}")
