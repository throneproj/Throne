#!/usr/bin/env python3
"""Exercise the GUI's config generator offline. Usage: python test_xray_route_outbounds.py <Throne executable>"""

import json
import sqlite3
import subprocess
import sys
import tempfile
from contextlib import closing
from pathlib import Path


def proxy(tag, port):
    return {"tag": tag, "protocol": "socks", "settings": {"servers": [{"address": "127.0.0.1", "port": port}]}}


def check(exe, root):
    data = root / "data"
    db = data / "config" / "throne.db"
    db.parent.mkdir(parents=True)
    out = root / "dump"

    def dump():
        subprocess.run([exe, "-dump-configs", str(out), "-appdata", str(data)], check=True, capture_output=True, timeout=60)

    def read(name):
        return json.loads((out / name).read_text(encoding="utf-8"))

    with closing(sqlite3.connect(db)) as c:
        c.execute("CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT NOT NULL)")
    dump()
    direct = {"protocol": "freedom", "tag": "direct"}
    listener_rule = {"type": "field", "inboundTag": ["user-in"], "domain": ["full:ipify.org"], "outboundTag": "direct"}
    original = dict(listener_rule, inboundTag=["user-in", "throne-bridge"])
    full = {"inbounds": [{"listen": "127.0.0.1", "port": 10080, "protocol": "socks", "tag": "user-in"}],
            "routing": {"rules": [listener_rule]}}
    nl = {"type": "custom", "subtype": "xrayfullconfig", "name": "NL1",
          "config": json.dumps(dict(full, outbounds=[direct, proxy("exit", 21001)]))}
    us = dict(nl, name="US1",
              config=json.dumps(dict(full, outbounds=[direct, proxy("other", 21002), proxy("exit", 21003)])))

    with closing(sqlite3.connect(db)) as c, c:
        group = c.execute("SELECT MIN(id) FROM groups").fetchone()[0]
        route = 77
        c.execute("INSERT INTO route_profiles(id,name) VALUES(?,?)", (route, "Country routes"))
        c.execute("UPDATE settings SET value=? WHERE key='current_route_id'", (str(route),))
        for pid, row in [(11, {"type": "socks", "name": "Main", "server": "127.0.0.1", "server_port": 21000}), (12, nl), (13, us)]:
            c.execute("INSERT INTO profiles(id,type,name,gid,outbound_json) VALUES (?,?,?,?,?)",
                      (pid, row["type"], row["name"], group, json.dumps(row)))
        for i, (pid, domain) in enumerate([(12, "google.com"), (13, "netflix.com"), (12, "ipify.org")]):
            c.execute("INSERT INTO route_rules(route_profile_id,rule_order,domain_json,outbound_id) VALUES (?,?,?,?)",
                      (route, i, json.dumps([domain]), pid))
        c.execute("INSERT INTO route_rules(route_profile_id,rule_order,process_path_regex_json,outbound_id) VALUES (?,?,?,?)",
                  (route, 3, json.dumps([".+Steam.+"]), 13))
    dump()
    for name in ("profile-11.json", "profile-11.tun.json", "profile-12.json"):
        doc = read(name)
        assert "error" not in doc, doc.get("error")
        if name == "profile-12.json":
            assert doc["xray"]["routing"]["rules"] == [original]  # normal launch still honors the subscription
        else:
            assert "xray" not in doc  # no rule took over the main Xray slot
        sb = doc["singbox"]
        assert sb["route"]["final"] == "proxy"
        rules = sb["route"]["rules"]
        names = {d: next(r["outbound"] for r in rules if d in r.get("domain", []))
                 for d in ("google.com", "netflix.com", "ipify.org")}
        assert names["google.com"] == names["ipify.org"] != names["netflix.com"]
        assert any(".+Steam.+" in r.get("process_path_regex", []) and r.get("outbound") == names["netflix.com"] for r in rules)
        for tag in set(names.values()):
            assert any(o["tag"] == tag and o["type"] == "socks" and o["server"] == "127.0.0.1" for o in sb["outbounds"])
        sides = doc["xray_full_configs"]
        assert len(sides) == 2
        exits = set()
        for side in sides:
            bridge, = side["inbounds"]
            assert (bridge["tag"], bridge["protocol"], bridge["listen"]) == ("throne-bridge", "socks", "127.0.0.1")
            assert bridge["settings"]["auth"] == "password"
            assert side["routing"]["rules"] == [original]
            chosen, = [o for o in side["outbounds"] if o.get("tag") == "exit"]
            exits.add(chosen["settings"]["servers"][0]["port"])
        assert exits == {21001, 21003}
    with closing(sqlite3.connect(db)) as c:
        assert json.loads(c.execute("SELECT outbound_json FROM profiles WHERE id=13").fetchone()[0]) == us


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="throne-route-test-") as tmp:
        check(str(Path(sys.argv[1]).resolve()), Path(tmp))
