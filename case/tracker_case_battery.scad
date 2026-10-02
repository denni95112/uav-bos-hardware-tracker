// UAV-BOS Personen-Tracker - battery case for the Heltec Wireless Tracker V1.1 (and Fastsaw clone)
// One 18650 cell in a standard 1x18650 holder lies below the board. The board uses its own
// charger (TP4054, 500 mA) and USB / battery switching, see README.
//
// Worn on a lanyard: the snap hook clips into a bridge in the middle of the top long edge. The case
// hangs horizontally, so the landscape display reads upright and tilts a little towards the wearer. The LoRa antenna
// points down from the bottom edge, the power switch sits in the top edge.
//
// Same coordinate system and board parameters as tracker_case.scad:
// X along the board length, USB-C at X = 0 (front), Y across, Z up.
// "Left" / "right" are as seen from the top with the USB-C port facing you (left = high Y).
//
// Render a single part with e.g.:
//   openscad -o battery_base.stl -D 'part="base"' tracker_case_battery.scad
//   openscad -o battery_lid.stl  -D 'part="lid"'  tracker_case_battery.scad
// The button plungers are identical to tracker_case.scad (plunger_user.stl, plunger_reset.stl).

part = "print"; // [print, base, lid, plunger_user, plunger_reset, assembly, exploded]

/* [Board] */
pcb_l = 63.6;
pcb_w = 27.9;
pcb_t = 1.7;
top_clear = 4.3;      // tallest top part (GPS module 3.8) + margin
bottom_clear = 5.0;   // underside connector 3.6 + battery plug and leads
usb_overhang = 1.0;
rear_overhang = 1.0;
stop_adjust = 2.5;    // moves the rear board stops towards the USB end (measured on a test print)

/* [Display] */
disp_x0 = 10.7;
disp_w = 23.2;
disp_h = 12.3;
disp_margin = 0.4;
disp_cy = pcb_w / 2;
frame_x0 = 9;
frame_w = 32.5;
frame_h = 16.1;
frame_t = 3.1;
window_recess = true;
window_recess_t = 0.6;
window_recess_margin = 0.5;

/* [GPS module (top, rear)] */
gps_w = 20.1;
gps_t = 3.8;

/* [USB-C] */
usb_cy = pcb_w / 2;
usb_z = 1.6;
usb_w = 8.8;
usb_h = 3.2;
usb_clear = 0.5;

/* [Buttons] */
btn_x = 2.22;
user_btn_y = 6.2;
reset_btn_y = pcb_w - 6.2;
button_h = 1.5;
button_hole = 4.0;
reset_mode = "plunger"; // [plunger, pinhole, none]
pinhole_d = 2.0;

/* [Battery] */
cell_d = 18.6;        // protected cell (with protection PCB and wrap)
cell_l = 69.5;
// MPD BH-18650-W (for protected cells, 150 mm leads), datasheet tolerance +-0.5
hold_l = 77.7;
hold_w = 20.9;
hold_h = 21.3;        // holder bottom to the top of the end walls
hold_floor = 1.5;     // under the cell (assembly view only)
hold_clr = 0.6;       // per side, covers the tolerance
hold_lead_space = 2.5; // at both ends for the solder tabs and leads
hold_top_gap = 0.5;   // cell top to the wiring space
hold_rib_h = 3;       // locating ribs on the floor
hold_rib_t = 1.2;

/* [Power switch] */
// Slide switch in the top edge, in the battery plus lead. It carries the 500 mA charge current.
power_switch = true;
sw_type = "TS01CQE"; // [TS01CQE, RUNCCI]
// TS01CQE: C&K datasheet, silver contacts, 3 A @ 28 V DC.
// RUNCCI: Amazon B09TVDZ8P2, 0.5 A @ 50 V DC, body 12.7 x 6.6, 5 mm knob. Body depth, knob
// cross-section and travel are not listed; the values here are assumptions, measure before printing.
//            body l, body h, body d, pins, knob, knob height, travel, wall in pocket
sw_presets = [["TS01CQE", 10.16, 5.08, 8.79, 3.3, 2.49, 3.1, 3.05, 1.0],
              ["RUNCCI",  12.7,  6.6,  8.2,  6.8, 2.0,  5.0, 3.0,  1.2]];
sw_p = sw_presets[search([sw_type], sw_presets)[0]];
sw_l = sw_p[1];       // body along the wall (travel direction)
sw_h = sw_p[2];       // body across the wall
sw_d = sw_p[3];       // body depth without pins
sw_pins = sw_p[4];
sw_lever = sw_p[5];   // knob cross-section
sw_lever_l = sw_p[6]; // knob height above the body
sw_travel = sw_p[7];
sw_wall = sw_p[8];    // wall thinned to this in a pocket that locates the body; the knob stands out sw_lever_l - sw_wall
sw_slot_clr = 0.3;    // per side

/* [Antennas] */
cable_space = 14;     // free space behind the board for U.FL pigtails, SMA nut and switch
lora_sma = true;      // SMA bulkhead in the bottom edge, the 868 MHz antenna points down (vertical polarisation)
gnss_sma = false;     // SMA on the rear end; the onboard GNSS antenna usually works when worn
sma_hole = 6.6;
sma_nut_d = 9.2;

/* [Case] */
wall = 2.0;
floor_t = 1.6;
lid_t = 1.6;
clr = 0.4;
lip_h = 2.0;
lip_t = 1.2;
fit = 0.25;
corner_r = 2.0;
insert_hole = 4.0;    // ruthex M3 x 5.7 heat-set insert
insert_depth = 9;
boss_d = 8;
screw_clear = 3.4;
boss_off = 2.3;       // boss centres diagonally outside the inner corners (>= 1.2 wall to the cavity)
holddown = true;
holddown_x = pcb_l - 3.7;

/* [Lanyard] */
// A bridge on the top edge for the snap hook of a standard lanyard. Seen from the end it is a
// hexagon with a hole along X: 45 deg legs and roof, printable without support. The hook passes
// through the hole and grips the outer bar.
lanyard = true;
lanyard_side = "left"; // [left, right] long side that is up when worn = top of the display text
lug_x = 0;             // offset of the bridge from the middle of the case length
lug_w = 6;             // bridge width along the case
lug_depth = 8;         // how far it stands out from the wall
lug_bar = 3.5;         // outer bar the hook grips; must pass through the hook gate
lug_leg = 4.5;         // vertical thickness of the two legs
lug_open = 6;          // free height of the hole at the bar
lug_top_gap = 0.5;     // bridge top below the lid joint

/* [Strap] */
strap_loops = false;   // alternative mount: flat ears with webbing slots on both short ends
strap_w = 25;          // webbing width
strap_t = 3.5;        // slot width
ear_len = 12;
ear_t = 4;
ear_side = 2.5;       // ears are this much wider than the box on each side

$fn = 48;
eps = 0.01;

// Derived dimensions
in_l = max(pcb_l + usb_overhang + rear_overhang + 2 * clr + cable_space, hold_l + 2 * hold_lead_space);
in_w = pcb_w + 2 * clr;
out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;

// PCB origin (corner at the USB end, bottom face)
px = wall + usb_overhang + clr;
py = wall + clr;
hold_top = floor_t + hold_h;
wire_z = hold_top + hold_top_gap; // floor of the wiring space under the board and of the rear compartment
pz = wire_z + bottom_clear;
pcb_top = pz + pcb_t;
base_h = pcb_top + top_clear;
in_h = base_h - floor_t;

hold_x = wall + (in_l - hold_l) / 2;
hold_y = wall + (in_w - hold_w) / 2;

disp_cx = disp_x0 + disp_w / 2;
frame_cx = frame_x0 + frame_w / 2;
gps_x0 = pcb_l + rear_overhang - gps_w;

// Rear compartment: behind the board stops, above the battery holder
stop_x = px + pcb_l + rear_overhang + clr - stop_adjust;
stop_l = 1.5 + stop_adjust;
rear_x0 = stop_x + stop_l;
rear_mid = (rear_x0 + wall + in_l) / 2;
sma_z = (wire_z + base_h) / 2;
sw_x = rear_mid;
sw_z = sma_z;
sw_pocket = sw_l + 0.3;
sw_pocket_h = sw_h + 0.3;
// side posts only where the rear compartment is long enough, otherwise the wall pocket alone locates the body
sw_post = sw_x - sw_pocket / 2 - 1.2 >= rear_x0 && sw_x + sw_pocket / 2 + 1.2 <= wall + in_l ? 1.2 : 0;

assert(hold_w + 2 * (hold_clr + hold_rib_t) <= in_w, "battery holder too wide for the case");
assert(hold_l + 2 * (hold_clr + hold_rib_t) <= in_l, "battery holder too long for the case");
assert(!(lora_sma || gnss_sma) || (sma_z - sma_nut_d / 2 >= wire_z && sma_z + sma_nut_d / 2 <= base_h),
       "SMA nut does not fit between the battery holder and the lid");
assert(!lora_sma || rear_mid - sma_nut_d / 2 >= rear_x0, "LoRa SMA nut hits the board stops");
assert(len(search([sw_type], sw_presets)) == 1, "unknown sw_type");
assert(!power_switch || sw_x - sw_pocket / 2 - sw_post >= rear_x0, "power switch hits the board stops");
assert(!power_switch || sw_x + sw_pocket / 2 + sw_post <= wall + in_l, "power switch hits the rear wall");
assert(!power_switch || sw_z + sw_pocket_h / 2 <= base_h - 1, "power switch too close to the lid");
assert(lanyard_side == "left" || lanyard_side == "right", "lanyard_side must be left or right");

screw_pos = [for (x = [wall - boss_off, out_l - wall + boss_off], y = [wall - boss_off, out_w - wall + boss_off]) [x, y]];

top_left = lanyard_side == "left";
lug_cx = out_l / 2 + lug_x;
lug_in = lug_depth - lug_bar; // hole depth from the wall to the bar
lug_h = 2 * lug_leg + 2 * lug_in + lug_open;
lug_z0 = base_h - lug_top_gap - lug_h; // above the centre of mass, so the display tilts up a little when worn
assert(!lanyard || lug_z0 >= 0, "lanyard bridge too tall for the case");
assert(!lanyard || lug_h - 2 * lug_depth > 0, "lanyard bridge too deep for its height");

module rounded_box(l, w, h, r) {
  hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(r = r, h = h);
}

module rrect(l, w, h, r) {
  translate([-l / 2, -w / 2, 0]) rounded_box(l, w, h, min(r, l / 2 - eps, w / 2 - eps));
}

// Cantilever from the right wall (low Y) with a 45 deg underside, printable without support.
module ledge(x0, lx, depth, z_top, h) {
  hull() {
    translate([x0, wall - eps, z_top - h]) cube([lx, depth + eps, h]);
    translate([x0, wall - eps, z_top - h - depth]) cube([lx, eps, h + depth]);
  }
}

module left_wall() { translate([0, out_w, 0]) mirror([0, 1, 0]) children(); }

module both_walls() { children(); left_wall() children(); }

// Features modelled on the right wall, moved to the edge that is up / down when worn
module top_wall() { if (top_left) left_wall() children(); else children(); }

module bottom_wall() { if (top_left) children(); else left_wall() children(); }

// ---------------------------------------------------------------- base
module base() {
  difference() {
    union() {
      difference() {
        rounded_box(out_l, out_w, base_h, corner_r);
        translate([wall, wall, floor_t]) cube([in_l, in_w, in_h + eps]);
      }
      holder_ribs();
      // PCB corner supports above the battery holder
      both_walls() {
        ledge(px, 3, clr + 3, pz, 2);
        ledge(px + pcb_l - 4, 4, clr + 4, pz, 2);
        // end stops behind the overhanging GPS module so the board cannot slide away from the USB opening
        ledge(stop_x, stop_l, clr + 4, pcb_top + 1.5, pcb_top + 1.5 - pz + 2);
      }
      for (p = screw_pos) translate([p[0], p[1], 0]) cylinder(d = boss_d, h = base_h);
      if (power_switch) switch_mount();
      if (lanyard) top_wall() lanyard_lug();
      if (strap_loops) strap_ears();
    }
    // USB-C opening
    translate([-eps, py + usb_cy, pcb_top + usb_z])
      rotate([0, 90, 0]) rotate([0, 0, 90])
        rrect(usb_w + 2 * usb_clear, usb_h + 2 * usb_clear, wall + 1, 1.2);
    if (lora_sma)
      bottom_wall() translate([rear_mid, wall + eps, sma_z]) rotate([90, 0, 0]) cylinder(d = sma_hole, h = wall + 1);
    if (gnss_sma)
      translate([out_l - wall - eps, out_w / 2, sma_z]) rotate([0, 90, 0]) cylinder(d = sma_hole, h = wall + 1);
    if (power_switch) {
      slot_l = sw_lever + sw_travel + 2 * sw_slot_clr;
      slot_h = sw_lever + 2 * sw_slot_clr;
      top_wall() {
        translate([sw_x - slot_l / 2, -eps, sw_z - slot_h / 2]) cube([slot_l, wall + 2 * eps, slot_h]);
        translate([sw_x - sw_pocket / 2, sw_wall, sw_z - sw_pocket_h / 2]) cube([sw_pocket, wall - sw_wall + eps, sw_pocket_h]);
      }
    }
    for (p = screw_pos) translate([p[0], p[1], base_h - insert_depth]) cylinder(d = insert_hole, h = insert_depth + eps);
  }
}

// On the right wall; profile in (outward = -Y, Z), extruded along X. Overlaps 1 mm into the wall.
module lanyard_lug() {
  z0 = lug_z0;
  z1 = lug_z0 + lug_h;
  d = lug_depth;
  translate([lug_cx - lug_w / 2, 0, 0]) rotate([90, 0, 90]) linear_extrude(lug_w)
    difference() {
      polygon([[1, z0], [0, z0], [-d, z0 + d], [-d, z1 - d], [0, z1], [1, z1]]);
      polygon([[0.5, z0 + lug_leg - 0.5], [-lug_in, z0 + lug_leg + lug_in],
               [-lug_in, z1 - lug_leg - lug_in], [0.5, z1 - lug_leg + 0.5]]);
    }
}

// Ribs along the holder sides and corner stops at the ends; the middle of the ends stays free for the leads.
module holder_ribs() {
  x0 = hold_x - hold_clr - hold_rib_t;
  x1 = hold_x + hold_l + hold_clr;
  y0 = hold_y - hold_clr - hold_rib_t;
  y1 = hold_y + hold_w + hold_clr;
  for (y = [y0, y1]) translate([x0, y, floor_t - eps]) cube([x1 + hold_rib_t - x0, hold_rib_t, hold_rib_h + eps]);
  for (x = [x0, x1], y = [y0, y1 + hold_rib_t - 4])
    translate([x, y, floor_t - eps]) cube([hold_rib_t, 4, hold_rib_h + eps]);
}

// Pocket in the wall, shelf and two side posts on the inside of the top wall; the switch is glued in.
module switch_mount() {
  top_wall() {
    // short, so the 45 deg underside stays above the battery holder; the wall pocket locates the body
    depth = min(sw_wall + sw_d - wall, 3.5);
    ledge(sw_x - sw_pocket / 2 - sw_post, sw_pocket + 2 * sw_post, depth, sw_z - sw_pocket_h / 2, 1.2);
    if (sw_post > 0)
      for (x = [sw_x - sw_pocket / 2 - sw_post, sw_x + sw_pocket / 2])
        translate([x, wall - eps, sw_z - sw_pocket_h / 2 - eps]) cube([sw_post, depth + eps, sw_pocket_h]);
  }
}

module strap_ears() {
  for (side = [0, 1]) {
    x0 = side == 0 ? -ear_len : out_l - 1;
    slot_x = side == 0 ? -ear_len / 2 - 0.5 : out_l + ear_len / 2 + 0.5;
    difference() {
      // overlaps 1 mm into the box so the union is one solid
      translate([x0, -ear_side, 0]) rounded_box(ear_len + 1, out_w + 2 * ear_side, ear_t, corner_r);
      translate([slot_x, out_w / 2, -eps]) rrect(strap_t, strap_w + 1.5, ear_t + 1, strap_t / 2);
    }
  }
}

// ---------------------------------------------------------------- lid
// Printed top-down: z = 0 is the outside face, Y is mirrored relative to the base.
function lid_y(board_y) = out_w - (py + board_y);

module lid() {
  difference() {
    union() {
      rounded_box(out_l, out_w, lid_t, corner_r);
      for (p = screw_pos) translate([p[0], out_w - p[1], 0]) cylinder(d = boss_d, h = lid_t);
      // locating lips along the long walls (inside the base)
      for (y = [wall + fit, out_w - wall - fit - lip_t])
        translate([px, y, lid_t - eps]) cube([pcb_l - 6, lip_t, lip_h]);
      if (holddown)
        for (y = [2, pcb_w - 2])
          translate([px + holddown_x, lid_y(y), lid_t - eps]) cylinder(d = 2.5, h = top_clear);
    }
    translate([px + disp_cx, lid_y(disp_cy), -eps])
      rrect(disp_w + 2 * disp_margin, disp_h + 2 * disp_margin, lid_t + 1, 1);
    if (window_recess)
      translate([px + frame_cx, lid_y(disp_cy), lid_t - window_recess_t])
        rrect(frame_w + 2 * window_recess_margin, frame_h + 2 * window_recess_margin, window_recess_t + 1, 1.5);
    translate([px + btn_x, lid_y(user_btn_y), -eps]) cylinder(d = button_hole, h = lid_t + 1);
    if (reset_mode == "plunger")
      translate([px + btn_x, lid_y(reset_btn_y), -eps]) cylinder(d = button_hole, h = lid_t + 1);
    else if (reset_mode == "pinhole")
      translate([px + btn_x, lid_y(reset_btn_y), -eps]) cylinder(d = pinhole_d, h = lid_t + 1, $fn = 24);
    // the lid is too thin for a counterbore, the heads sit on top
    for (p = screw_pos) translate([p[0], out_w - p[1], -eps]) cylinder(d = screw_clear, h = lid_t + 1);  }
}

// ---------------------------------------------------------------- button plunger (same as tracker_case.scad)
plunger_gap = 0.3;
plunger_bottom_trim = 1.0;
plunger_top = 2.8;
mark_depth = 0.4;
module plunger(mark = "dot") {
  shaft_d = button_hole - 0.5;
  flange_flat = 0.6;
  below = top_clear - button_h - plunger_gap - plunger_bottom_trim;
  flange_cone = min((button_hole + 1.6 - shaft_d) / 2, below - flange_flat);
  flange_d = shaft_d + 2 * flange_cone;
  lower = below - flange_cone - flange_flat;
  top = below + lid_t + plunger_top;
  assert(flange_d >= button_hole + 0.8, "plunger flange too small to be retained by the lid");
  difference() {
    union() {
      cylinder(d = shaft_d, h = top);
      translate([0, 0, lower]) cylinder(d1 = shaft_d, d2 = flange_d, h = flange_cone);
      translate([0, 0, lower + flange_cone]) cylinder(d = flange_d, h = flange_flat);
    }
    translate([0, 0, top - mark_depth]) {
      if (mark == "x")
        for (a = [45, -45]) rotate([0, 0, a]) translate([-shaft_d * 0.35, -0.3, 0]) cube([shaft_d * 0.7, 0.6, mark_depth + 1]);
      else if (mark == "dot")
        cylinder(d = 1.2, h = mark_depth + 1, $fn = 24);
    }
  }
}

// ---------------------------------------------------------------- dummy parts for the assembly view
module board_dummy() {
  color("steelblue") translate([px, py, pz]) cube([pcb_l, pcb_w, pcb_t]);
  color("dimgray") translate([px + frame_x0, py + disp_cy - frame_h / 2, pcb_top]) cube([frame_w, frame_h, frame_t]);
  color("black") translate([px + disp_x0, py + disp_cy - disp_h / 2, pcb_top + frame_t - 0.2 + eps])
    cube([disp_w, disp_h, 0.2]);
  color("goldenrod") translate([px + gps_x0, py + (pcb_w - gps_w) / 2, pcb_top]) cube([gps_w, gps_w, gps_t]);
  color("silver") translate([px - usb_overhang, py + usb_cy - usb_w / 2, pcb_top + usb_z - usb_h / 2])
    cube([7.3, usb_w, usb_h]);
  for (y = [user_btn_y, reset_btn_y])
    color("white") translate([px + btn_x, py + y, pcb_top]) cylinder(d = 2.5, h = button_h);
}

module battery_dummy() {
  cell_cz = floor_t + hold_floor + cell_d / 2;
  end_t = (hold_l - cell_l) / 2;
  color("#333") difference() {
    union() {
      translate([hold_x, hold_y, floor_t]) cube([hold_l, hold_w, cell_cz - floor_t]);
      for (x = [hold_x, hold_x + hold_l - end_t]) translate([x, hold_y, floor_t]) cube([end_t, hold_w, hold_h]);
    }
    translate([hold_x + end_t, hold_y + hold_w / 2, cell_cz]) rotate([0, 90, 0]) cylinder(d = cell_d + 0.6, h = cell_l);
  }
  translate([hold_x + end_t, hold_y + hold_w / 2, cell_cz]) rotate([0, 90, 0]) {
    color("royalblue") cylinder(d = cell_d, h = cell_l - 1);
    color("silver") translate([0, 0, cell_l - 1]) cylinder(d = 7, h = 1);
  }
}

module switch_dummy(explode = 0) {
  top_wall() translate([0, -explode, 0]) {
    color("#222") translate([sw_x - sw_l / 2, sw_wall, sw_z - sw_h / 2]) cube([sw_l, sw_d, sw_h]);
    color("silver") translate([sw_x - sw_lever / 2 - sw_travel / 2, sw_wall - sw_lever_l, sw_z - sw_lever / 2])
      cube([sw_lever, sw_lever_l, sw_lever]);
    for (dx = [-2.54, 0, 2.54]) color("silver") translate([sw_x + dx - 0.25, sw_wall + sw_d, sw_z - 0.25]) cube([0.5, sw_pins, 0.5]);
  }
}

// SMA bulkhead with nuts on both sides of the wall and an 868 MHz stub antenna
module sma_dummy(antenna = true) {
  color("gold") {
    translate([-wall - 8, 0, 0]) rotate([0, 90, 0]) cylinder(d = 6.3, h = wall + 14);
    translate([-wall - 2.2, 0, 0]) rotate([0, 90, 0]) cylinder(d = sma_nut_d, h = 2, $fn = 6);
    translate([0.2, 0, 0]) rotate([0, 90, 0]) cylinder(d = sma_nut_d, h = 2, $fn = 6);
  }
  if (antenna) color("#111") {
    translate([6, 0, 0]) rotate([0, 90, 0]) cylinder(d = 9, h = 8, $fn = 6);
    translate([14, 0, 0]) rotate([0, 90, 0]) cylinder(d1 = 9, d2 = 7, h = 40);
  }
}

// Snap hook through the bridge and the start of a 20 mm lanyard
module lanyard_dummy() {
  zc = lug_z0 + lug_h / 2;
  top_wall() {
    color("silver") translate([lug_cx, -(lug_depth - lug_bar / 2), zc]) scale([1.5, 1, 1]) {
      rotate_extrude($fn = 32) translate([4.2, 0]) circle(d = 2.5, $fn = 12);
      translate([0, -4.2, 0]) rotate([90, 0, 0]) cylinder(d = 4, h = 12);
    }
    color("royalblue") translate([lug_cx - 10, -(lug_depth - lug_bar / 2 + 4.2 + 12) - 40, zc - 0.6]) cube([20, 40, 1.2]);
  }
}

module inserts_dummy() {
  for (p = screw_pos) color("gold") translate([p[0], p[1], base_h - 5.7]) cylinder(d = 4.4, h = 5.7);
}

module screws_dummy() {
  for (p = screw_pos) color("#444") translate([p[0], p[1], base_h + lid_t]) {
    cylinder(d = 5.5, h = 3);
    translate([0, 0, -8]) cylinder(d = 3, h = 8);
  }
}

module window_dummy() {
  color("lightcyan", 0.5) translate([px + frame_cx, py + disp_cy, base_h])
    rrect(frame_w + 2 * window_recess_margin - 0.2, frame_h + 2 * window_recess_margin - 0.2, window_recess_t - 0.1, 1.5);
}

// [board y, marking]
plungers = reset_mode == "plunger" ? [[user_btn_y, "dot"], [reset_btn_y, "x"]] : [[user_btn_y, "dot"]];

// exploded view: z offsets of the layers
ex = part == "exploded" ? 1 : 0;
ex_battery = 45 * ex;
ex_board = 70 * ex;
ex_lid = 105 * ex;
ex_screws = 125 * ex;

module assembly() {
  base();
  inserts_dummy();
  translate([0, 0, ex_battery]) battery_dummy();
  translate([0, 0, ex_board]) board_dummy();
  if (power_switch) switch_dummy(ex * 20);
  if (lora_sma) bottom_wall() translate([rear_mid, -ex * 20, sma_z]) rotate([0, 0, -90]) sma_dummy();
  if (gnss_sma) translate([out_l + ex * 20, out_w / 2, sma_z]) sma_dummy(antenna = false);
  if (lanyard && !ex) lanyard_dummy();
  translate([0, 0, ex_lid]) {
    for (p = plungers)
      color("orange") translate([px + btn_x, py + p[0], pcb_top + button_h + plunger_gap + plunger_bottom_trim - ex * 10])
        plunger(p[1]);
    window_dummy();
    color("red", 0.6) translate([0, out_w, base_h + lid_t]) mirror([0, 0, 1]) mirror([0, 1, 0]) lid();
  }
  translate([0, 0, ex_screws]) screws_dummy();
}

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "plunger_user") plunger("dot");
else if (part == "plunger_reset") plunger("x");
else if (part == "assembly" || part == "exploded") assembly();
else {
  lid_y0 = out_w + 8 + (lanyard ? lug_depth : 0) + 2 * max(ear_side, boss_off + boss_d / 2 - wall);
  base();
  translate([0, lid_y0, 0]) lid();
  for (i = [0 : len(plungers) - 1]) translate([-10, lid_y0 + out_w / 2 + i * 9, 0]) plunger(plungers[i][1]);
}
