// UAV-BOS Fahrzeug-Tracker - case for the Heltec Wireless Tracker V1.1 (and Fastsaw clone)
//
// Coordinate system: X along the board length, USB-C at X = 0 (front), Y across, Z up.
// All board-relative positions are measured from the PCB corner at the USB end, display side up.
// "Left" / "right" below are as seen from the top with the USB-C port facing you
// (left = high Y, right = low Y). Defaults are measured on the Fastsaw board.
//
// Render a single part with e.g.:
//   openscad -o base.stl    -D 'part="base"'    tracker_case.scad
//   openscad -o lid.stl     -D 'part="lid"'     tracker_case.scad
//   openscad -o plunger_user.stl  -D 'part="plunger_user"'  tracker_case.scad
//   openscad -o plunger_reset.stl -D 'part="plunger_reset"' tracker_case.scad

part = "print"; // [print, base, lid, plunger_user, plunger_reset, assembly]

/* [Board] */
pcb_l = 63.6;         // PCB length
pcb_w = 27.9;         // PCB width
pcb_t = 1.7;          // bare PCB thickness
top_clear = 4.3;      // tallest top part (GPS module 3.8) + margin
bottom_clear = 4.0;   // underside: connector at the USB end 3.6 + margin (rest of the underside is 1.8)
usb_overhang = 1.0;   // USB-C shell past the front PCB edge
rear_overhang = 1.0;  // GPS module past the rear PCB edge
stop_adjust = 2.5;    // moves the rear board stops towards the USB end (measured on a test print)

/* [Display] */
disp_x0 = 10.7;       // visible area starts here (from PCB front; measured 11.7, corrected after a test print)
disp_w = 23.2;        // visible area along the board
disp_h = 12.3;        // visible area across the board
disp_margin = 0.4;    // window oversize per side
disp_cy = pcb_w / 2;  // display centred across the board
frame_x0 = 9;         // display frame starts here (shifted with disp_x0)
frame_w = 32.5;
frame_h = 16.1;
frame_t = 3.1;        // GPS module is ~0.7 higher
window_recess = true; // pocket on the lid inside for a clear cover over the frame area
window_recess_t = 0.6;
window_recess_margin = 0.5;

/* [GPS module (top, rear)] */
gps_w = 20.1;
gps_t = 3.8;

/* [USB-C] */
usb_cy = pcb_w / 2;   // assumed centred
usb_z = 1.6;          // centre above the PCB top (port sits on the PCB)
usb_w = 8.8;
usb_h = 3.2;
usb_clear = 0.5;      // per side

/* [Buttons] */
btn_x = 2.22;         // both buttons, from PCB front (measured 3.22, corrected after a test print)
user_btn_y = 6.2;     // USER (PRG, GPIO0), 6.2 from the right edge
reset_btn_y = pcb_w - 6.2; // RST, 6.2 from the left edge
button_h = 1.5;       // button cap height above PCB top
button_hole = 4.0;
reset_mode = "plunger"; // [plunger, pinhole, none]
pinhole_d = 2.0;

/* [Antennas] */
cable_space = 14;     // free space behind the board for U.FL pigtails and SMA nuts
gnss_sma = true;      // SMA bulkhead on the rear end for an external GNSS antenna
lora_sma = false;     // extra SMA on the side wall (LoRa, unused by the firmware)
sma_hole = 6.6;       // 1/4"-36 SMA thread
sma_min_in_h = 11;    // inner height needed for an SMA bulkhead nut

/* [Case] */
wall = 2.0;
floor_t = 1.6;
lid_t = 1.6;
clr = 0.4;            // board clearance per side
lip_h = 2.0;          // locating lip of the lid
lip_t = 1.2;
fit = 0.25;           // lid lip to wall clearance
corner_r = 2.0;
screw_hole = 1.8;     // M2 self-tapping into the bosses
screw_clear = 2.4;
screw_head = 4.2;
boss_d = 5.6;
front_supports = true; // small supports under the front PCB corners (check against the underside connector)
holddown = true;      // pins in the lid pressing on the rear PCB corners
holddown_x = pcb_l - 3.7; // pin centre from PCB front (measured on a test print)
front_hook = true;    // lid hooks into the front wall (USB end); rear is screwed
hook_w = 4;
hook_len = 3.0;       // tab length below the lid underside
hook_t = 1.2;
hook_nose = 0.8;      // nose depth towards the wall
hook_nose_h = 1.0;
mount_ears = true;    // screw / cable-tie ears on the short ends
ear_len = 10;
ear_t = 3;
vents = true;

$fn = 48;
eps = 0.01;

// Derived dimensions
has_sma = gnss_sma || lora_sma;
// Extra height for an SMA nut goes below the PCB, so display and buttons stay close to the lid.
bottom_eff = has_sma ? max(bottom_clear, sma_min_in_h - pcb_t - top_clear) : bottom_clear;
in_l = pcb_l + usb_overhang + rear_overhang + 2 * clr + cable_space;
in_w = pcb_w + 2 * clr;
in_h = bottom_eff + pcb_t + top_clear;
out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;
base_h = floor_t + in_h;

// PCB origin (corner at the USB end, bottom face)
px = wall + usb_overhang + clr;
py = wall + clr;
pz = floor_t + bottom_eff;
pcb_top = pz + pcb_t;

disp_cx = disp_x0 + disp_w / 2;
frame_cx = frame_x0 + frame_w / 2;
gps_x0 = pcb_l + rear_overhang - gps_w;

// bosses sink 0.5 mm into the walls so they fuse with them
boss_x = out_l - wall - boss_d / 2 + 0.5;
boss_ys = [wall + boss_d / 2 - 0.5, out_w - wall - boss_d / 2 + 0.5];

vent_h = top_clear - lip_h - 0.5;

// Hooks sit in the front corners: the USB opening's top edge is too close to the wall top,
// and the button plunger flanges occupy the space next to it.
hook_board_ys = [hook_w / 2 + 0.2, pcb_w - hook_w / 2 - 0.2];
hook_pocket_depth = 1.0;
hook_pocket_clr = 0.2;

module rounded_box(l, w, h, r) {
  hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(r = r, h = h);
}

module rrect(l, w, h, r) {
  translate([-l / 2, -w / 2, 0]) rounded_box(l, w, h, min(r, l / 2 - eps, w / 2 - eps));
}

// ---------------------------------------------------------------- base
module base() {
  difference() {
    union() {
      difference() {
        rounded_box(out_l, out_w, base_h, corner_r);
        translate([wall, wall, floor_t]) cube([in_l, in_w, in_h + eps]);
      }
      // PCB corner supports
      if (front_supports)
        for (y = [py, py + pcb_w - 3])
          translate([px, y, floor_t - eps]) cube([3, 3, bottom_eff + eps]);
      for (y = [py, py + pcb_w - 4])
        translate([px + pcb_l - 4, y, floor_t - eps]) cube([4, 4, bottom_eff + eps]);
      // end stops behind the overhanging GPS module so the board cannot slide away from the USB opening
      for (y = [py, py + pcb_w - 4])
        translate([px + pcb_l + rear_overhang + clr - stop_adjust, y, floor_t - eps])
          cube([1.5 + stop_adjust, 4, bottom_eff + pcb_t + 1.5]);
      // screw bosses
      for (y = boss_ys) translate([boss_x, y, floor_t - eps]) cylinder(d = boss_d, h = in_h + eps);
      if (mount_ears) ears();
    }
    // USB-C opening
    translate([-eps, py + usb_cy, pcb_top + usb_z])
      rotate([0, 90, 0]) rotate([0, 0, 90])
        rrect(usb_w + 2 * usb_clear, usb_h + 2 * usb_clear, wall + 1, 1.2);
    // GNSS SMA on the rear end
    if (gnss_sma)
      translate([out_l - wall - eps, out_w / 2, floor_t + in_h / 2])
        rotate([0, 90, 0]) cylinder(d = sma_hole, h = wall + 1);
    // optional LoRa SMA on the side wall
    if (lora_sma)
      translate([px + pcb_l + rear_overhang + 6, out_w - wall - eps, floor_t + in_h / 2])
        rotate([-90, 0, 0]) cylinder(d = sma_hole, h = wall + 1);
    // screw holes
    for (y = boss_ys) translate([boss_x, y, floor_t + 1]) cylinder(d = screw_hole, h = in_h);
    if (vents && vent_h >= 1) vent_slots();
    if (front_hook) hook_pockets();
  }
}

// pockets in the inner face of the front wall for the lid hook noses
module hook_pockets() {
  z0 = base_h - hook_len - hook_pocket_clr;
  h = hook_nose_h + 2 * hook_pocket_clr;
  for (y = hook_board_ys)
    translate([wall - hook_pocket_depth, py + y - (hook_w + 0.6) / 2, z0])
      cube([hook_pocket_depth + eps, hook_w + 0.6, h]);
}

module ears() {
  for (side = [0, 1]) {
    x0 = side == 0 ? -ear_len : out_l;
    translate([x0, 0, 0]) difference() {
      hull() {
        translate([side == 0 ? corner_r : ear_len - corner_r, 4, 0]) cylinder(r = corner_r, h = ear_t);
        translate([side == 0 ? corner_r : ear_len - corner_r, out_w - 4, 0]) cylinder(r = corner_r, h = ear_t);
        // overlaps 1 mm into the box so the union is one solid
        translate([side == 0 ? ear_len : -1, corner_r, 0]) cube([1, out_w - 2 * corner_r, ear_t]);
      }
      // slot for M4 screws or cable ties
      hull() for (y = [out_w / 2 - 4, out_w / 2 + 4])
        translate([ear_len / 2, y, -eps]) cylinder(d = 4.4, h = ear_t + 1);
    }
  }
}

// between the PCB top and the lid lips
module vent_slots() {
  n = 4;
  for (i = [0 : n - 1], y = [-eps, out_w - wall - eps])
    translate([px + 18 + i * 7, y, pcb_top + 0.3]) cube([3, wall + 2 * eps, vent_h]);
}

// ---------------------------------------------------------------- lid
// Printed top-down: z = 0 is the outside face, Y is mirrored relative to the base.
function lid_y(board_y) = out_w - (py + board_y);

module lid() {
  difference() {
    union() {
      rounded_box(out_l, out_w, lid_t, corner_r);
      // locating lips along the long walls (inside the base)
      for (y = [wall + fit, out_w - wall - fit - lip_t])
        translate([px, y, lid_t - eps]) cube([pcb_l - 6, lip_t, lip_h]);
      if (holddown)
        for (y = [2, pcb_w - 2])
          translate([px + holddown_x, lid_y(y), lid_t - eps]) cylinder(d = 2.5, h = top_clear);
      if (front_hook)
        for (y = hook_board_ys) translate([0, lid_y(y) - hook_w / 2, 0]) hook_tab();
    }
    // display window
    translate([px + disp_cx, lid_y(disp_cy), -eps])
      rrect(disp_w + 2 * disp_margin, disp_h + 2 * disp_margin, lid_t + 1, 1);
    if (window_recess)
      translate([px + frame_cx, lid_y(disp_cy), lid_t - window_recess_t])
        rrect(frame_w + 2 * window_recess_margin, frame_h + 2 * window_recess_margin, window_recess_t + 1, 1.5);
    // USER button plunger hole
    translate([px + btn_x, lid_y(user_btn_y), -eps]) cylinder(d = button_hole, h = lid_t + 1);
    // Reset
    if (reset_mode == "plunger")
      translate([px + btn_x, lid_y(reset_btn_y), -eps]) cylinder(d = button_hole, h = lid_t + 1);
    else if (reset_mode == "pinhole")
      translate([px + btn_x, lid_y(reset_btn_y), -eps]) cylinder(d = pinhole_d, h = lid_t + 1, $fn = 24);
    // screw holes with counterbore on the outside
    for (y = boss_ys) {
      translate([boss_x, out_w - y, -eps]) cylinder(d = screw_clear, h = lid_t + 1);
      translate([boss_x, out_w - y, -eps]) cylinder(d = screw_head, h = 0.8);
    }
  }
}

// Tab hanging down inside the front wall, nose at the tip pointing into the wall pocket.
// The nose face towards the lid is flat so it holds when the lid is pulled up; the tip is
// chamfered as a lead-in. Close the case by hooking the front in with the rear tilted up.
module hook_tab() {
  x0 = wall + fit;
  z_tip = lid_t + hook_len;
  translate([x0, 0, lid_t - eps]) cube([hook_t, hook_w, hook_len + eps]);
  hull() {
    translate([x0 - hook_nose, 0, z_tip - hook_nose_h]) cube([hook_nose + eps, hook_w, hook_nose_h - 0.4]);
    translate([x0 - eps, 0, z_tip - hook_nose_h]) cube([eps, hook_w, hook_nose_h]);
  }
}

// ---------------------------------------------------------------- button plunger
// Printed standing on the button end. Drop it into the lid hole before closing the case:
// the flange sits under the lid, the top sticks out plunger_top.
plunger_gap = 0.3;         // free travel before the button is touched
plunger_bottom_trim = 1.0; // shortens the part below the lid (measured on a test print)
plunger_top = 2.8;         // how far the plunger sticks out above the lid
mark_depth = 0.4;          // engraved marking on top: "x" = Reset, "dot" = USER
module plunger(mark = "dot") {
  shaft_d = button_hole - 0.5;
  flange_flat = 0.6;
  below = top_clear - button_h - plunger_gap - plunger_bottom_trim; // tip to lid underside
  // 45 deg underside, printable without support; may run down to the tip on short plungers
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

// ---------------------------------------------------------------- dummy board for the assembly view
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

// [board y, marking]
plungers = reset_mode == "plunger" ? [[user_btn_y, "dot"], [reset_btn_y, "x"]] : [[user_btn_y, "dot"]];

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "plunger_user") plunger("dot");
else if (part == "plunger_reset") plunger("x");
else if (part == "assembly") {
  base();
  board_dummy();
  for (p = plungers)
    color("orange") translate([px + btn_x, py + p[0], pcb_top + button_h + plunger_gap + plunger_bottom_trim]) plunger(p[1]);
  color("red", 0.6) translate([0, out_w, base_h + lid_t]) mirror([0, 0, 1]) mirror([0, 1, 0]) lid();
} else {
  base();
  translate([0, out_w + 8, 0]) lid();
  for (i = [0 : len(plungers) - 1]) translate([-8, out_w + 8 + out_w / 2 + i * 9, 0]) plunger(plungers[i][1]);
}
