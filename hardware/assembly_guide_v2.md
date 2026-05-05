# HANSI ZOE AI GOGGLES — Assembly Guide v2
## For Metal Welding Steampunk Goggles (VampireFreaks Style)

---

## Base Goggles: What to Buy

### Recommended: German Metal Welding Goggles (Clear Lens)

The base frame is a pair of authentic German-made metal welding goggles. These are the closest match to Hange Zoë's goggles in Attack on Titan — round metal eye cups, rubber lining, leather nose bridge, elastic strap with metal buckle.

**Where to buy (from Germany):**

| Source | Product | Price | Notes |
|--------|---------|-------|-------|
| Amazon.com (ship to DE) | VampireFreaks Steampunk Metal Goggles — Clear Lens | ~$18 + shipping | Best option. Real metal, 50mm glass lenses, leather bridge |
| Amazon.de | TRIXES Steampunk Schweißerbrille (B01I5012DK) | ~10 EUR | Cheaper plastic version, still good look |
| eBay.de | Search: "Schweißerbrille rund Metall Steampunk" | 8-20 EUR | Many options, check for metal construction |
| AliExpress | Search: "vintage welding goggles metal round" | 5-15 EUR | Cheapest, longer shipping, quality varies |
| andracor.com (Berlin) | Steampunk Brille bronze/kupfer/silber | ~12 EUR | German LARP shop, plastic but good design |
| carolandme.de (Berlin) | Steampunk Schweißerbrille mit Prismaglas | ~15 EUR | Berlin-based, with interchangeable prism lenses |

### Key specs to look for:
- **50mm round lenses** (standard welding size — important for our 3D printed mounts)
- **Metal frame** preferred (aluminum or steel — can drill, tap threads, attach brackets)
- **Screw-in lens retaining ring** (lets you swap lenses or add custom optics)
- **Elastic strap with buckle** (not just tied elastic)
- **Rubber gasket/lining** (comfort for extended wear)
- **Clear lenses** (not shade #5 dark — you need to see through them!)

If your goggles come with dark welding lenses, you can buy clear 50mm replacement lenses from steampunkgoggles.com or cut your own from 2mm polycarbonate sheet.

---

## Updated BOM (Phase 6 v2)

| # | Component | Cost |
|---|-----------|------|
| 1 | Metal welding goggles (clear lens, 50mm) | 10-18 EUR |
| 2 | Seeed XIAO ESP32-S3 Sense | 14 EUR |
| 3 | LILYGO T-Glass V2 | 48 EUR |
| 4 | LiPo 3.7V 1000mAh (103040) | 5 EUR |
| 5 | TP4056 USB-C charge module | 1 EUR |
| 6 | Slide switch SPDT | 0.50 EUR |
| 7 | WS2812B strip (3 LEDs) | 2 EUR |
| 8 | Amber gel filter (5x5cm) | 1 EUR |
| 9 | M3 bolts 12mm + nuts (x4) | 1 EUR |
| 10 | M3 bolt 20mm + wingnut (x1, hinge) | 0.50 EUR |
| 11 | Silicone wire 28AWG (1m each: red, black, signal) | 3 EUR |
| 12 | JST-PH 2.0 connectors (2-pin x2, 4-pin x1) | 2 EUR |
| 13 | PETG filament (~40g) | 3 EUR |
| 14 | Leather strap 20mm (optional replacement) | 5 EUR |
| 15 | Brass rivets 4mm (decorative, x6) | 2 EUR |
| 16 | Matte black spray paint (Tamiya TS-6 or TS-29) | 8 EUR |
| | **TOTAL** | **~106 EUR** |

---

## Mount System Overview

The 3D printed mount system (`hansi_mount_v2.scad`) has 5 parts that attach to the round metal goggles:

```
          ┌─ LED Accent Ring (Part 5)
          │   (amber glow around right eye)
          │
    ┌─────┼─── Camera Ring Mount (Part 1)
    │     │     (C-clamp on UPPER right eye cup)
    │     │     (XIAO ESP32-S3 sits on top)
    │     │
    │   ┌─┴──── Right Eye Cup (goggles)
    │   │        50mm lens, metal frame
    │   └─┬────
    │     │
    │     └─── HUD Arm (Part 2a)
    │           (C-clamp on LOWER right eye cup)
    │           │
    │           └─── T-Glass Cradle (Part 2b)
    │                 (hinged, foldable, ~15° prism angle)
    │
    └─── Strap ──── Strap Clip (Part 4) x3
                     (cable routing every ~8cm)
                     │
                     └─── Battery Pod (Part 3)
                           (nape of neck, threads on strap)
                           (LiPo + TP4056 + switch)
```

---

## Step-by-Step Assembly

### Step 1: Print the parts

Open `hansi_mount_v2.scad` in OpenSCAD. Render and export each part separately as STL:

| Part | Print Time | Supports | Notes |
|------|-----------|----------|-------|
| Camera Ring | ~45 min | Yes (overhangs) | Tight tolerance on clamp — test fit first |
| HUD Arm | ~30 min | Minimal | Print flat, hinge end up |
| T-Glass Cradle | ~40 min | Yes (cavity) | Print upside down for clean cavity |
| Battery Pod | ~50 min | Minimal | Print right-side up |
| Strap Clips (x3) | ~20 min | No | Easy print, batch all 3 |
| LED Ring | ~25 min | No | Optional but adds Hange aesthetic |

**Settings:** PETG, 0.2mm layer, 30% infill (40% for camera ring clamp ears), 0.4mm nozzle.

**Post-processing:**
1. Remove supports with flush cutters
2. Test-fit on goggles BEFORE painting
3. Adjust clamp tolerance with sandpaper if too tight
4. Sand all surfaces (220 grit → 400 grit)
5. Prime with filler primer
6. Spray matte black (Tamiya TS-6) — 2 thin coats
7. Optional: bronze dry-brush on edges for steampunk effect

### Step 2: Prepare the goggles

1. Clean the metal goggles with isopropyl alcohol
2. If lenses are dark (shade #5), unscrew the retaining ring and replace with clear 50mm polycarbonate discs
3. Optional: swap the elastic strap for a 20mm leather strap with brass buckle
4. Optional: add brass rivets as decorative accents along the eye cup rims (drill 2mm pilot holes in rubber gasket area, press-fit rivets)

### Step 3: Mount the Camera Ring (Part 1)

1. Open the C-clamp slightly by flexing the PETG
2. Slide over the **upper** rim of the **right** eye cup
3. The XIAO pod should sit on top (12 o'clock position)
4. Insert M3 bolt through both clamp ears
5. Tighten with M3 nut — firm but don't crack the PETG
6. Insert XIAO ESP32-S3 Sense into the pod cavity
   - Camera module facing **forward** (away from face)
   - USB-C port accessible from the rear
7. Camera lens should align with the hole in the ring

### Step 4: Mount the HUD Arm (Parts 2a + 2b)

1. Clamp the HUD arm (Part 2a) on the **lower** rim of the right eye cup
2. The arm extends outward toward the right temple
3. Attach the T-Glass cradle (Part 2b) to the arm end using M3x20mm bolt
4. Use a wingnut or nyloc nut for adjustable tension
5. Insert T-Glass V2 into the cradle:
   - Prism facing **downward and inward** (toward the right eye)
   - USB-C port accessible through the side cutout
   - Touch button accessible through top cutout
6. Adjust hinge angle until the prism projects into your upper-right field of view (~15° inward)
7. Tighten the wingnut to lock the angle
8. Test: the HUD arm should fold **up** and out of the way when not needed

### Step 5: Wire the battery pod (Part 3)

```
                    ┌──────────────────┐
  Wire to XIAO ◄───┤  Slide Switch    │
                    ├──────────────────┤
                    │  TP4056 Module   ├───► USB-C Charging
                    ├──────────────────┤
                    │  LiPo 1000mAh   │
                    └──────────────────┘
```

1. Solder TP4056 module: BAT+ / BAT- to LiPo (through slide switch on BAT+)
2. Solder output wires: OUT+ / OUT- (28AWG, ~40cm each)
3. Insert LiPo into battery cavity
4. Insert TP4056 into its cavity (USB-C port facing rearward through wall cutout)
5. Install slide switch in top cutout (hot glue to secure)
6. Thread the elastic strap through both side slots of the pod
7. Position at the nape of your neck

### Step 6: Cable routing

1. Route power wires (red/black from battery pod) along the strap
2. Clip strap clips (Part 4) every ~8cm along the right side of strap
3. Tuck wires into the cable channels on top of each clip
4. At the goggles: connect to XIAO BAT pads (solder or JST connector)
5. Route a thin USB-C cable from TP4056 to T-Glass along the strap
6. Route WS2812B data wire from XIAO Pin 21 to LED ring (if installed)

### Step 7: LED accent ring (Part 5, optional)

1. Press-fit 3x WS2812B LEDs into the ring cavities (at 60°, 180°, 300°)
2. Solder in daisy-chain: DIN → DOUT → DIN → DOUT → DIN
3. Connect VCC/GND to XIAO 3.3V rail
4. Connect first DIN to XIAO Pin 21
5. Cut amber gel filter into a thin strip and press into the diffuser slot
6. Slide the LED ring over the camera ring (friction fit)

### Step 8: Flash and test

See Phase 4 firmware guide. Quick verification:
1. Power on (slide switch)
2. Right eye: amber LED glow (if installed)
3. XIAO LED blinks slowly (IDLE)
4. Open companion app → BLE connect → "HansiGoggles" appears
5. T-Glass auto-connects within ~10s → shows "HANSI ZOE AI GOGGLES"
6. Set Scout mode → camera captures → phone analyzes → HUD shows result
7. Set Titan mode → speak → Whisper → LLM → TTS responds → HUD shows text
8. Fold HUD arm up → goggles are normal clear-lens goggles

---

## Weight Budget

| Component | Weight |
|-----------|--------|
| Metal goggles (base) | ~120g |
| Camera ring + XIAO | ~25g |
| HUD arm + T-Glass | ~27g |
| LED ring | ~5g |
| Battery pod + LiPo + TP4056 | ~40g |
| Wires, clips, strap | ~15g |
| **TOTAL** | **~232g (~8.2 oz)** |

For reference, a pair of ski goggles is ~200-300g. Comfortable for 2-3 hours of continuous wear.

---

## Aesthetic Finishing (Hange Zoë Style)

### Required (core look):
- Matte black paint on all 3D printed parts
- Clear round 50mm lenses
- Elastic or leather strap

### Recommended (enhanced Hange):
- Leather strap replacement with brass buckle
- Brass rivets on eye cup rims (4 per cup)
- Amber LED glow ring on right eye
- Bronze/copper dry-brush weathering on metal edges
- "ExMachina" or Survey Corps logo engraved/etched on left eye cup

### Optional (maximum steampunk):
- Brass gear decorations hot-glued to camera ring pod
- Leather wrap around battery pod
- Patina wash on metal parts (vinegar + salt solution, 24h)
- Tiny brass compass rose on HUD arm hinge
- Engraved serial number on inside of eye cup: "MK-IV HANSI"

---

*"If you don't try, you'll never know." — Hange Zoë*
