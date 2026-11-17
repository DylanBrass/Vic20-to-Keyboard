import random
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import schemdraw
import schemdraw.elements as elm

try:
    OUT_DIR = Path(__file__).resolve().parent
except NameError:
    OUT_DIR = Path.cwd()

WIRES = {
    1: 38, 3: 4, 5: 12, 6: 13, 7: 11, 8: 14, 9: 21, 10: 8, 11: 10,
    12: 9, 13: 16, 14: 17, 15: 18, 16: 15, 17: 7, 18: 6, 19: 5, 20: 4,
}

PIN_COLORS = [
    ("white", "#FFFFFF"), ("empty", "#00000000"), ("black", "#1A1A1A"),
    ("empty", "#00000000"), ("grey", "#C4C7CC"), ("purple", "#7B3F98"),
    ("blue", "#1F5FBF"), ("green", "#1E9B5A"), ("yellow", "#F5C800"),
    ("orange", "#F28C1B"), ("red", "#E0262B"), ("brown", "#8B5A2B"),
    ("grey", "#C4C7CC"), ("purple", "#7B3F98"), ("blue", "#1F5FBF"),
    ("green", "#1E9B5A"), ("yellow", "#F5C800"), ("orange", "#F28C1B"),
    ("red", "#E0262B"), ("brown", "#8B5A2B"),
]
NK = len(PIN_COLORS)

LEFT_PINS = ["G", "G", 19, 20, 21, 47, 48, 15, 0, 35, 36, 37, 38, 39, 40, 41, 42, 2, 1, "RX", "TX", "G"]
RIGHT_PINS = ["G", "5V", 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17, 16, 15, 7, 6, 5, 4, "RST", "3V3", "3V3"]
SPECIAL = {
    "TX": "GPIO43 TX", "RX": "GPIO44 RX", 0: "GPIO0 BOOT", 3: "GPIO3 strap",
    45: "GPIO45 strap", 46: "GPIO46 strap", 19: "GPIO19 USB D-", 20: "GPIO20 USB D+"
}
NB = len(LEFT_PINS)

TXT, DIM, EDGE = "#222222", "#555555", "#2B2B2B"
SP = 0.55
LANE = 0.52
ESP_X = 20


def pin_name(p):
    if p == "G":
        return "GND"
    if p in ("3V3", "5V", "RST"):
        return str(p)
    return SPECIAL.get(p, f"GPIO{p}")


def board_pins():
    pins = []
    for side, header, tag in (("left", LEFT_PINS, "Left"), ("right", RIGHT_PINS, "Right")):
        for i, p in enumerate(header):
            pins.append(elm.IcPin(name=pin_name(p), pin=f"{tag}-{i + 1}", side=side,
                                  slot=f"{NB - i}/{NB}", color=DIM,
                                  anchorname=f"{side[0].upper()}{i}", pinlblsize=7, lblsize=8))
    pins.append(elm.IcPin(name="USB-C (USB)", pin="", side="top", pos=0.78,
                          color=DIM, anchorname="usb", lblsize=8))
    pins.append(elm.IcPin(name="USB-C (UART)", pin="", side="top", pos=0.22,
                          color=DIM, anchorname="uart", lblsize=8))
    return pins


def kbd_pins():
    return [elm.IcPin(name=f"{i + 1}  {cname}", pin="", side="right",
                      slot=f"{NK - i}/{NK}", color=EDGE, anchorname=f"k{i + 1}", lblsize=9)
            for i, (cname, _) in enumerate(PIN_COLORS)]


def seg_cross(s, t):
    (p, q), (r, u) = s, t

    def hv(a, b):
        return abs(a[1] - b[1]) < 1e-9, abs(a[0] - b[0]) < 1e-9

    sh, sv = hv(p, q)
    th, tv = hv(r, u)
    if sh and tv:
        h, v = (p, q), (r, u)
    elif sv and th:
        h, v = (r, u), (p, q)
    else:
        return False
    e = 1e-6
    return (min(h[0][0], h[1][0]) + e < v[0][0] < max(h[0][0], h[1][0]) - e and
            min(v[0][1], v[1][1]) + e < h[0][1] < max(v[0][1], v[1][1]) - e)


def count_crossings(routes):
    n = 0
    for i in range(len(routes)):
        for j in range(i + 1, len(routes)):
            net_i, net_j = routes[i]["net"], routes[j]["net"]
            weight = 50 if ({net_i, net_j} == {1, 12}) else 1
            if net_i == net_j:
                continue
            for s in zip(routes[i]["pts"], routes[i]["pts"][1:]):
                for t in zip(routes[j]["pts"], routes[j]["pts"][1:]):
                    if seg_cross(s, t):
                        n += weight
    return n


def build_routes(nets, st, kx, ex_right, top0):
    lx0 = kx + 1.4
    xm = lx0 + LANE * len(st["lx"]) + 0.8
    xf0 = ex_right + 1.2
    slot, xf = {}, {}
    cnt, xfi = {1: 0, -1: 0}, {1: 0, -1: 0}
    for g in st["order"]:
        if nets[g]["side"] != "R":
            continue
        dr = st["dir"][g]
        slot[g] = cnt[dr]
        cnt[dr] += 2 if len(nets[g]["srcs"]) > 1 else 1
        xf[g] = xf0 + LANE * xfi[dr]
        xfi[dr] += 1
    lane_x = {k: lx0 + LANE * i for i, k in enumerate(st["lx"])}
    routes, merges = [], []
    for g, net in nets.items():
        b, srcs = net["b"], net["srcs"]
        col = lambda k: PIN_COLORS[k - 1][1]
        if net["side"] == "R":
            dr = st["dir"][g]
            ylane = lambda j: dr * (top0 + LANE * (slot[g] + j))
            if len(srcs) == 1:
                k, a = srcs[0]
                yt = ylane(0)
                pts = [a, (lane_x[k], a[1]), (lane_x[k], yt), (xf[g], yt), (xf[g], b[1]), b]
                routes.append(dict(net=g, pts=pts, color=col(k), kind="wire"))
            else:
                ys = [ylane(0), ylane(1)]
                if st["flip"][g]:
                    ys.reverse()
                ym = sum(ys) / 2
                for (k, a), yt in zip(srcs, ys):
                    pts = [a, (lane_x[k], a[1]), (lane_x[k], yt), (xm, yt), (xm, ym)]
                    routes.append(dict(net=g, pts=pts, color=col(k), kind="wire"))
                routes.append(dict(net=g, pts=[(xm, ym), (xf[g], ym), (xf[g], b[1]), b],
                                   kind="trunk", colors=[col(k) for k, _ in srcs]))
                merges.append(((xm, ym), g))
        else:
            if len(srcs) == 1:
                k, a = srcs[0]
                pts = [a, (lane_x[k], a[1]), (lane_x[k], b[1]), b]
                routes.append(dict(net=g, pts=pts, color=col(k), kind="wire"))
            else:
                for (k, a), off in zip(srcs, (LANE / 2, -LANE / 2)):
                    pts = [a, (lane_x[k], a[1]), (lane_x[k], b[1] + off), (xm, b[1] + off), (xm, b[1])]
                    routes.append(dict(net=g, pts=pts, color=col(k), kind="wire"))
                routes.append(dict(net=g, pts=[(xm, b[1]), b], kind="trunk",
                                   colors=[col(k) for k, _ in srcs]))
                merges.append(((xm, b[1]), g))
    info = dict(n_top=xfi[1], n_bot=xfi[-1])
    return routes, merges, info


def total_cost(nets, st, kx, ex_right, top0):
    routes, _, info = build_routes(nets, st, kx, ex_right, top0)
    return count_crossings(routes) + 3 * info["n_bot"]


def clone(st):
    return {k: (v[:] if isinstance(v, list) else dict(v)) for k, v in st.items()}


def optimise(nets, kx, ex_right, top0, seed=42, iters=8000, restarts=4):
    rnd = random.Random(seed)
    all_src = sorted(k for n in nets.values() for k, _ in n["srcs"])
    right = [g for g, n in nets.items() if n["side"] == "R"]
    st = dict(lx=list(all_src), order=list(right), xf=list(right),
              dir={g: 1 for g in nets}, flip={g: False for g in nets})
    cost = lambda s: total_cost(nets, s, kx, ex_right, top0)
    best, bc = st, cost(st)
    print(f"  -> Initial layout cost score: {bc}", flush=True)

    steps_per_restart = iters // restarts
    for r in range(restarts):
        print(f"  -> Running optimization restart {r+1}/{restarts}...", flush=True)
        cur = clone(best if r == 0 else st)
        if r:
            rnd.shuffle(cur["lx"])
            rnd.shuffle(cur["order"])
            for g in right:
                cur["dir"][g] = rnd.choice((1, -1))
        cc = cost(cur)
        for _ in range(steps_per_restart):
            cand = clone(cur)
            x = rnd.random()
            if x < 0.15:
                g = rnd.choice(right)
                cand["dir"][g] = -cand["dir"][g]
            elif x < 0.22:
                g = rnd.choice(list(cand["flip"]))
                cand["flip"][g] = not cand["flip"][g]
            else:
                lst = cand[rnd.choice(("lx", "order"))]
                i, j = rnd.sample(range(len(lst)), 2)
                lst[i], lst[j] = lst[j], lst[i]
            c = cost(cand)
            if c <= cc:
                cur, cc = cand, c
        if cc < bc:
            best, bc = cur, cc
    return best, bc


if __name__ == "__main__":
    print("Initializing schematic drawing...", flush=True)
    with schemdraw.Drawing(show=False) as d:
        d.config(unit=3, fontsize=10, lw=2)

        hk = NK * SP + 0.8
        hb = NB * SP + 0.8

        kbd = elm.Ic(pins=kbd_pins(), size=(4.2, hk), edgepadW=0.6).at((0, 0)).anchor("center")
        kbd.label("VIC-20 keyboard connector", loc="top", fontsize=12, ofst=(0, 0.5))
        d.add(kbd)

        esp = elm.Ic(pins=board_pins(), size=(6.2, hb), edgepadW=0.6).at((ESP_X, 0)).anchor("center")
        esp.label("ESP32-S3-DevKitC-1", loc="top", fontsize=13, ofst=(0, 0.5))
        d.add(esp)

        anchors = {}
        for side, header in (("L", LEFT_PINS), ("R", RIGHT_PINS)):
            for i, p in enumerate(header):
                anchors.setdefault(p, (side, getattr(esp, f"{side}{i}")))

        nets = {}
        for kpin in sorted(WIRES):
            gpio = WIRES[kpin]
            if gpio not in anchors:
                raise SystemExit(f"GPIO {gpio!r} is not on the headers (pin {kpin})")
            a = getattr(kbd, f"k{kpin}")
            side, b = anchors[gpio]
            net = nets.setdefault(gpio, dict(side=side, b=(b[0], b[1]), srcs=[]))
            net["srcs"].append((kpin, (a[0], a[1])))

        kx = kbd.k1[0]
        ex_right = esp.R0[0]
        top0 = esp.R0[1] + SP * 1.5 + 1.4

        print("Starting routing optimization...", flush=True)
        state, ncross = optimise(nets, kx, ex_right, top0)
        print(f"Optimization completed! Final cost score: {ncross}", flush=True)

        routes, merges, info = build_routes(nets, state, kx, ex_right, top0)

        for r in routes:
            for p, q in zip(r["pts"], r["pts"][1:]):
                d.add(elm.Line().at(p).to(q).color(EDGE).linewidth(5))
        for r in routes:
            for p, q in zip(r["pts"], r["pts"][1:]):
                if r["kind"] == "wire":
                    d.add(elm.Line().at(p).to(q).color(r["color"]).linewidth(2.6))
                else:
                    c1, c2 = r["colors"]
                    d.add(elm.Line().at(p).to(q).color(c1).linewidth(2.6))
                    d.add(elm.Line().at(p).to(q).color(c2).linewidth(2.6).linestyle('--'))
        for (mx, my), g in merges:
            d.add(elm.Dot(radius=0.16).at((mx, my)).color(EDGE).fill("white"))

        y_hdr = -(top0 + LANE * (info['n_bot'] + 2) + 3.0)
        vic_20_led_connector = elm.Header(rows=3).at((5, y_hdr))
        d.add(vic_20_led_connector)
        led = elm.LED(fill="red").length(1).at((0, y_hdr + 0.5)).theta(90)
        d.add(led)
        d.add(elm.Wire("-").at(led.end).to(vic_20_led_connector.pin2))
        d.add(elm.Wire("-").color("red").at(led.start).to(vic_20_led_connector.pin3))

        p3, p5 = vic_20_led_connector.pin2, vic_20_led_connector.pin3
        gnd_pin, v33_pin = esp.L21, esp.R21
        n_xf = max(info['n_top'], info['n_bot'])
        y_gnd = p3[1] - 0.3
        x_gnd = gnd_pin[0] - 0.35
        y_3v3 = p5[1] - 0.8
        x_3v3 = max(v33_pin[0] + 0.5, ex_right + 0.8 + LANE * n_xf + 0.3)
        for pts, col in (
                ([p3, (p3[0], y_gnd), (x_gnd, y_gnd), (x_gnd, gnd_pin[1]), gnd_pin], "black"),
                ([p5, (p5[0], y_3v3), (x_3v3, y_3v3), (x_3v3, v33_pin[1]), v33_pin], "red"),
        ):
            for p, q in zip(pts, pts[1:]):
                d.add(elm.Line().at(p).to(q).color(col).linewidth(2))

        OUT_DIR.mkdir(parents=True, exist_ok=True)
        print("Saving output diagram files...", flush=True)
        d.save(str(OUT_DIR / "wiring.png"), dpi=170, transparent=False)
        print(f"SUCCESS: Diagram successfully saved to {OUT_DIR / 'wiring.png'}", flush=True)