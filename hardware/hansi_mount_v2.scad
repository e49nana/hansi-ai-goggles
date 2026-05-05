// ═══════════════════════════════════════════════════════════════
//  HANSI ZOE AI GOGGLES — 3D Mount System v2
//  For: Metal Welding Steampunk Goggles (VampireFreaks style)
//  OpenSCAD Parametric Design — Phase 6 Updated
// ═══════════════════════════════════════════════════════════════
//
//  Base goggles: German-made metal welding goggles
//    - Eye cup outer diameter: ~62mm (with rubber ring)
//    - Lens diameter: 50mm (standard, screw-in)
//    - Eye cup depth: ~25mm
//    - Nose bridge: leather, ~25mm center gap
//    - Strap: ~20mm elastic with metal buckle
//    - Material: aluminum body, rubber gasket, glass lens
//
//  Mount strategy (fidèle Hange Zoë):
//    - Camera ring: clips onto RIGHT eye cup outer rim
//    - HUD arm: articulated bracket from right eye cup
//    - Battery pod: sits on rear strap at nape
//    - Strap clips: adapter pieces for strap attachment
//
//  Print: PETG, 0.2mm layer, 30% infill, supports
//  Post-process: Sand, prime, spray matte black or bronze
// ═══════════════════════════════════════════════════════════════

// ─── GOGGLES DIMENSIONS (measured from VampireFreaks metal goggles) ──
eye_cup_od = 62;        // outer diameter of eye cup (with rubber ring)
eye_cup_id = 50;        // inner = lens diameter
eye_cup_depth = 25;     // depth of eye cup from face
cup_wall = 2;           // metal wall thickness
lens_d = 50;            // lens diameter (standard welding)
lens_thread_d = 54;     // threaded retaining ring outer dia
strap_w = 20;           // elastic strap width
strap_t = 2.5;          // strap thickness

// ─── ELECTRONICS DIMENSIONS ──────────────────────────────────
// Seeed XIAO ESP32-S3 Sense
xiao_w = 21;    xiao_l = 17.5;   xiao_h = 8;
cam_lens_d = 8; // camera module lens diameter

// LILYGO T-Glass V2
tg_w = 51.5;    tg_l = 20;       tg_h = 12;
prism_w = 15;   prism_h = 12;    // prism optical element

// LiPo battery (103040 = 10x30x40mm, 1000mAh)
bat_w = 30;     bat_l = 40;      bat_h = 10;

// ─── GENERAL ─────────────────────────────────────────────────
wall = 1.8;         // print wall thickness
tol = 0.3;          // fit tolerance
$fn = 60;

// ═══════════════════════════════════════════════════════════════
//  PART 1: Camera Ring Mount
//  Clips around the RIGHT eye cup outer rim.
//  Houses the XIAO ESP32-S3 Sense on TOP of the eye cup.
//  Camera lens peers forward through a cutout.
// ═══════════════════════════════════════════════════════════════
module camera_ring() {
    ring_id = eye_cup_od + 2*tol;       // inside grips the cup
    ring_od = ring_id + 2*wall + 2;     // outer ring
    ring_h = 12;                        // ring height (grip zone)
    
    // XIAO pod dimensions
    pod_w = xiao_w + 2*wall + 2*tol;
    pod_l = xiao_l + 2*wall + 2*tol;
    pod_h = xiao_h + 2*wall + 2*tol;
    
    difference() {
        union() {
            // Clamp ring (top 180°, sits on upper half of eye cup)
            difference() {
                cylinder(d=ring_od, h=ring_h);
                translate([0, 0, -0.5])
                    cylinder(d=ring_id, h=ring_h+1);
                // Remove bottom half — C-clamp shape
                translate([-ring_od/2-1, -ring_od/2-1, -1])
                    cube([ring_od+2, ring_od/2+1, ring_h+2]);
                // Split gap for flex (2mm gap at bottom)
                translate([-1, -ring_od/2-1, -1])
                    cube([2, ring_od+2, ring_h+2]);
            }
            
            // Clamp screw boss (to tighten the C-clamp)
            translate([-ring_od/2, 0, ring_h/2])
                rotate([0, 90, 0])
                    cylinder(d=8, h=8, center=true);
            translate([ring_od/2, 0, ring_h/2])
                rotate([0, 90, 0])
                    cylinder(d=8, h=8, center=true);
            
            // XIAO electronics pod (sits on top of ring)
            translate([-pod_w/2, -pod_l/2, ring_h])
                difference() {
                    // Outer shell with rounded top
                    hull() {
                        cube([pod_w, pod_l, pod_h - 2]);
                        translate([1, 1, pod_h-2])
                            cube([pod_w-2, pod_l-2, 2]);
                    }
                    // XIAO cavity
                    translate([wall+tol, wall+tol, wall])
                        cube([xiao_w+2*tol, xiao_l+2*tol, xiao_h+5]);
                }
        }
        
        // Camera lens hole (front-facing, through ring + pod)
        translate([0, -ring_od/2-1, ring_h + pod_h/2])
            rotate([-90, 0, 0])
                cylinder(d=cam_lens_d+2, h=ring_od);
        
        // USB-C access slot (back of XIAO = facing away from face)
        translate([-5, pod_l/2+ring_id/2-wall, ring_h+wall+1])
            cube([10, wall+2, 4]);
        
        // Ventilation holes (top of pod)
        for (dx = [-6, 0, 6]) {
            translate([dx, 0, ring_h+pod_h-0.5])
                cylinder(d=2.5, h=wall+1);
        }
        
        // Clamp screw holes (M3)
        translate([-ring_od/2-5, 0, ring_h/2])
            rotate([0, 90, 0])
                cylinder(d=3.2, h=ring_od+10);
    }
    
    // Status LED light pipe hole
    translate([pod_w/2-2, 0, ring_h+pod_h-wall])
        cylinder(d=3, h=wall+0.5);
}

// ═══════════════════════════════════════════════════════════════
//  PART 2: T-Glass HUD Arm
//  Attaches to the RIGHT eye cup via a secondary clamp ring.
//  Articulated arm extends forward + inward to position
//  the T-Glass prism in the upper-right FOV.
//  Hinge allows folding the HUD up when not in use.
// ═══════════════════════════════════════════════════════════════
module hud_arm() {
    ring_id = eye_cup_od + 2*tol;
    ring_od = ring_id + 2*wall + 2;
    ring_h = 8;
    
    arm_len = 55;       // length from eye cup to prism position
    arm_w = 10;
    arm_h = 6;
    
    cradle_w = tg_w + 2*wall + 2*tol;
    cradle_l = tg_l + 2*wall + 2*tol;
    cradle_h = tg_h + wall + tol;
    
    // Attachment ring (lower half of eye cup, opposite to camera)
    difference() {
        union() {
            // Lower C-clamp ring
            difference() {
                cylinder(d=ring_od, h=ring_h);
                translate([0, 0, -0.5])
                    cylinder(d=ring_id, h=ring_h+1);
                // Remove top half
                translate([-ring_od/2-1, 0, -1])
                    cube([ring_od+2, ring_od/2+1, ring_h+2]);
            }
            
            // Arm extending outward-forward from temple side
            translate([ring_od/2-2, -arm_w/2, 0])
                cube([arm_len, arm_w, arm_h]);
            
            // Hinge at end of arm
            translate([ring_od/2-2 + arm_len, 0, arm_h/2])
                rotate([90, 0, 0])
                    cylinder(d=8, h=arm_w+4, center=true);
        }
        
        // Hinge bolt hole (M3)
        translate([ring_od/2-2 + arm_len, 0, arm_h/2])
            rotate([90, 0, 0])
                cylinder(d=3.2, h=arm_w+6, center=true);
        
        // Flex gap for clamp
        translate([-1, ring_od/2-1, -1])
            cube([2, 5, ring_h+2]);
    }
}

// T-Glass cradle (separate piece, connects via hinge bolt)
module tglass_cradle() {
    cradle_w = tg_w + 2*wall + 2*tol;
    cradle_l = tg_l + 2*wall + 2*tol;
    cradle_h = tg_h + wall + tol;
    
    arm_stub = 15;
    
    difference() {
        union() {
            // Hinge tab
            translate([0, -5, 0])
                cube([arm_stub, 10, 6]);
            translate([0, 0, 3])
                rotate([0, 90, 0])
                    cylinder(d=8, h=arm_stub);
            
            // T-Glass cradle body
            translate([arm_stub, -cradle_l/2, 0])
            difference() {
                cube([cradle_w, cradle_l, cradle_h]);
                // T-Glass cavity
                translate([wall+tol, wall+tol, wall])
                    cube([tg_w+2*tol, tg_l+2*tol, tg_h+5]);
                // Prism window (front, angled down at ~15°)
                translate([wall+5, -1, wall+2])
                    cube([tg_w-10, wall+2, tg_h-4]);
                // USB-C cutout (end)
                translate([cradle_w-wall-1, cradle_l/2-4, wall+2])
                    cube([wall+2, 8, 5]);
                // Touch button access (top)
                translate([cradle_w/2-5, cradle_l/2-5, cradle_h-wall+0.1])
                    cube([10, 10, wall+1]);
            }
        }
        
        // Hinge bolt hole (M3)
        translate([-1, 0, 3])
            rotate([0, 90, 0])
                cylinder(d=3.4, h=arm_stub+2);
    }
    
    // Prism guide rails (inside cradle, keep T-Glass aligned)
    translate([arm_stub + wall+tol, -cradle_l/2 + wall, wall])
        cube([2, cradle_l - 2*wall, 2]);
    translate([arm_stub + cradle_w - wall - tol - 2, -cradle_l/2 + wall, wall])
        cube([2, cradle_l - 2*wall, 2]);
}

// ═══════════════════════════════════════════════════════════════
//  PART 3: Battery Pod (Nape Mount)
//  Sits on the elastic strap at the back of the head.
//  Houses: LiPo 1000mAh + TP4056 charge module + slide switch.
//  Strap threads through slots on either side.
// ═══════════════════════════════════════════════════════════════
module battery_pod() {
    pod_w = bat_w + 2*wall + 2*tol;
    pod_l = bat_l + 15 + 2*wall + 2*tol;  // battery + TP4056 module
    pod_h = bat_h + 2*wall + 2*tol;
    
    tp_w = 17;  tp_l = 28;  tp_h = 4;  // TP4056 module
    
    difference() {
        // Outer shell
        hull() {
            translate([2, 2, 0]) cylinder(r=2, h=pod_h);
            translate([pod_w-2, 2, 0]) cylinder(r=2, h=pod_h);
            translate([2, pod_l-2, 0]) cylinder(r=2, h=pod_h);
            translate([pod_w-2, pod_l-2, 0]) cylinder(r=2, h=pod_h);
        }
        
        // Battery cavity
        translate([wall+tol, wall+tol, wall])
            cube([bat_w+2*tol, bat_l+2*tol, bat_h+tol+5]);
        
        // TP4056 cavity (behind battery)
        translate([(pod_w-tp_w)/2, wall+tol+bat_l+2, wall])
            cube([tp_w+2*tol, tp_l+2*tol, tp_h+tol+5]);
        
        // USB-C charging port (through wall)
        translate([(pod_w-10)/2, pod_l-wall-1, wall+1])
            cube([10, wall+2, 5]);
        
        // Wire exit holes (both sides, toward goggles)
        translate([-1, 10, wall+2])
            cube([wall+2, 5, 4]);
        translate([pod_w-wall-1, 10, wall+2])
            cube([wall+2, 5, 4]);
        
        // Slide switch cutout (top)
        translate([pod_w/2-4, 5, pod_h-wall+0.1])
            cube([8, 6, wall+1]);
        
        // Strap slots (left and right sides)
        // Strap threads through these to hold the pod
        translate([-1, pod_l/2-strap_w/2-2, (pod_h-strap_t)/2-1])
            cube([wall+2, strap_w+4, strap_t+2*tol+2]);
        translate([pod_w-wall-1, pod_l/2-strap_w/2-2, (pod_h-strap_t)/2-1])
            cube([wall+2, strap_w+4, strap_t+2*tol+2]);
        
        // LED indicator window (TP4056 charge LEDs)
        translate([pod_w/2-3, pod_l-wall+0.1, wall+2])
            cube([6, wall+1, 3]);
        
        // Ventilation
        for (i = [0:2]) {
            translate([4+i*10, -1, wall+2])
                cube([3, wall+2, pod_h-2*wall-2]);
        }
    }
    
    // Internal battery retaining lips
    translate([wall, wall+tol, wall])
        cube([3, 3, bat_h]);
    translate([pod_w-wall-3, wall+tol, wall])
        cube([3, 3, bat_h]);
}

// ═══════════════════════════════════════════════════════════════
//  PART 4: Strap Adapter Clips
//  Small clips that attach accessories to the elastic strap.
//  Used to route cables along the strap neatly.
// ═══════════════════════════════════════════════════════════════
module strap_clip() {
    clip_w = strap_w + 2*wall + 2*tol;
    clip_l = 12;
    clip_h = strap_t + 2*wall + 2*tol;
    
    difference() {
        cube([clip_w, clip_l, clip_h]);
        // Strap channel
        translate([wall, -1, wall])
            cube([strap_w+2*tol, clip_l+2, strap_t+2*tol]);
        // Entry slot (snap-in)
        translate([wall+3, -1, clip_h-wall+0.1])
            cube([strap_w+2*tol-6, clip_l+2, wall+1]);
    }
    
    // Cable channel (on top)
    translate([clip_w/2-4, 0, clip_h])
        difference() {
            cube([8, clip_l, 5]);
            translate([wall, -1, wall])
                cube([8-2*wall, clip_l+2, 5]);
        }
}

// ═══════════════════════════════════════════════════════════════
//  PART 5: LED Accent Ring (Optional)
//  Decorative ring that sits AROUND the right eye cup.
//  Houses 3x WS2812B LEDs behind amber diffuser.
//  Gives the Hange Zoë amber glow effect.
// ═══════════════════════════════════════════════════════════════
module led_ring() {
    ring_id = eye_cup_od + 2*tol + 2*wall + 2;  // sits outside camera ring
    ring_od = ring_id + 6;
    ring_h = 5;
    
    difference() {
        cylinder(d=ring_od, h=ring_h);
        translate([0, 0, -0.5])
            cylinder(d=ring_id, h=ring_h+1);
        
        // LED cavities (3 LEDs at 120° intervals)
        for (a = [60, 180, 300]) {
            rotate([0, 0, a])
                translate([ring_id/2+1.5, -2.5, wall]) {
                    cube([4, 5, 5]);  // WS2812B is 5x5mm
                    // Wire channel
                    translate([2, -2, 0])
                        cube([1.5, 9, 2]);
                }
        }
        
        // Diffuser slot (thin ring channel for amber gel)
        translate([0, 0, ring_h-1.5])
            difference() {
                cylinder(d=ring_od-1, h=2);
                cylinder(d=ring_id+1, h=2);
            }
    }
}

// ═══════════════════════════════════════════════════════════════
//  RENDER — Uncomment parts individually for printing
// ═══════════════════════════════════════════════════════════════

// Reference: eye cup outline (not printed, just for visualization)
%translate([0, 0, -5]) cylinder(d=eye_cup_od, h=eye_cup_depth);

// Part 1: Camera ring mount
color("DarkSlateGray") camera_ring();

// Part 2a: HUD arm (attaches to lower eye cup)
color("SteelBlue") translate([0, 0, -ring_h_offset()]) hud_arm();

// Part 2b: T-Glass cradle (connects to arm via hinge)
color("Teal") translate([95, 0, 0]) tglass_cradle();

// Part 3: Battery pod
color("DimGray") translate([-80, 0, 0]) battery_pod();

// Part 4: Strap clips (x3)
color("Gray") for (i = [0:2]) {
    translate([-80, 60 + i*20, 0]) strap_clip();
}

// Part 5: LED accent ring
color("DarkGoldenrod") translate([0, 80, 0]) led_ring();

// Helper
function ring_h_offset() = 8;

// ═══════════════════════════════════════════════════════════════
//  ASSEMBLY NOTES
// ═══════════════════════════════════════════════════════════════
//
//  Assembly order:
//  1. Slide LED ring onto right eye cup (optional)
//  2. Clamp camera ring onto UPPER right eye cup rim
//     → Tighten M3 bolt through clamp ears
//     → Insert XIAO into pod, camera facing forward
//  3. Clamp HUD arm onto LOWER right eye cup rim
//     → Connect T-Glass cradle via M3 hinge bolt + nut
//     → Insert T-Glass, prism facing down toward eye
//     → Adjust angle to ~15° inward
//  4. Thread strap through battery pod slots
//     → Position at nape of neck
//     → Insert LiPo + TP4056 module
//     → Route USB-C cable from TP4056 to XIAO via strap clips
//  5. Clip strap clips every ~8cm along strap
//     → Route signal + power wires through cable channels
//  6. Connect:
//     → TP4056 OUT → XIAO BAT pads (red/black)
//     → XIAO Pin 21 → WS2812B DIN (through LED ring)
//     → TP4056 OUT → T-Glass USB-C (via thin cable)
//
//  Cable routing (right side, from front to back):
//  Camera Ring → (internal) → strap clip → strap clip → battery pod
//  HUD arm → (short cable) → strap clip → battery pod
//
//  Total weight estimate:
//    Goggles base:   ~120g (metal + glass)
//    Camera ring:    ~15g (PETG) + 10g (XIAO)
//    HUD arm:        ~12g (PETG) + 15g (T-Glass)
//    Battery pod:    ~10g (PETG) + 25g (LiPo) + 5g (TP4056)
//    Misc:           ~10g (wires, clips, LEDs)
//    ────────────────────────────────────
//    TOTAL:          ~222g (~7.8 oz)
//
// ═══════════════════════════════════════════════════════════════
