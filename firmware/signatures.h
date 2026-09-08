#pragma once
#include <Arduino.h>
#include "config.h"

/*
 * Smart Glass Canary — vendor signature tables
 *
 * Detection is SCORED, not binary. Each signal a device matches adds points;
 * a device alerts once it crosses ALERT_SCORE_THRESHOLD (config.h).
 *
 * Why scoring rather than a plain allow-list: the single strongest signal we
 * get — an advertised name — is also the one most likely to collide with
 * something innocent. "Meta" alone is weak. "Meta" plus a Meta Platforms
 * company ID in the manufacturer data is conclusive. Weights below encode
 * that: a weak signal scores under the threshold on its own and only alerts
 * when something corroborates it.
 *
 * TIER_META  — Meta Ray-Ban / Oakley Meta family.
 * TIER_BROAD — other camera/display eyewear; compiled in only when
 *              ENABLE_BROAD_VENDORS is 1.
 */

#define TIER_META   0
#define TIER_BROAD  1

// ------------------------------------------------------- advertised names
// Matched case-insensitively as a substring of the BLE local name (from the
// advertising packet or the scan response).

struct NameRule { const char* needle; const char* vendor; uint8_t score; uint8_t tier; };

static const NameRule NAME_RULES[] = {
  // --- Meta family. The branded strings are decisive on their own.
  { "ray-ban",     "Meta Ray-Ban",        70, TIER_META  },
  { "rayban",      "Meta Ray-Ban",        70, TIER_META  },
  { "oakley meta", "Oakley Meta",         70, TIER_META  },
  { "hstn",        "Oakley Meta HSTN",    65, TIER_META  },
  { "vanguard",    "Meta Vanguard",       55, TIER_META  },
  // Bare "meta" is deliberately below threshold: it needs corroboration
  // from a company ID or OUI before it will alert.
  { "meta",        "Meta (unspecified)",  35, TIER_META  },

  // --- Broader eyewear. Distinctive product names score high; generic
  //     dictionary words are kept sub-threshold on purpose.
  { "spectacles",  "Snap Spectacles",     70, TIER_BROAD },
  { "echo frames", "Amazon Echo Frames",  70, TIER_BROAD },
  { "xreal",       "Xreal",               70, TIER_BROAD },
  { "nreal",       "Nreal (Xreal)",       70, TIER_BROAD },
  { "rokid",       "Rokid",               70, TIER_BROAD },
  { "rayneo",      "RayNeo",              70, TIER_BROAD },
  { "vuzix",       "Vuzix",               70, TIER_BROAD },
  { "even g1",     "Even Realities G1",   70, TIER_BROAD },
  { "even g2",     "Even Realities G2",   70, TIER_BROAD },
  { "halliday",    "Halliday",            65, TIER_BROAD },
  { "inmo",        "INMO",                65, TIER_BROAD },
  { "engo",        "Engo Eyewear",        60, TIER_BROAD },
  { "solos ",      "Solos",               60, TIER_BROAD },
  { "bose frames", "Bose Frames",         65, TIER_BROAD },
  // "frame" collides with picture frames, door frames and Brilliant Labs
  // Frame alike — sub-threshold, corroboration required.
  { "frame",       "Frame (generic)",     30, TIER_BROAD },
};

// ------------------------------------------- Bluetooth SIG company IDs
// First two bytes of BLE manufacturer-specific data, little-endian.
// Every value below is taken from the Bluetooth SIG assigned-numbers
// database (Nordic's mirror of company_identifiers), so the IDs themselves
// are authoritative. What is heuristic is the assumption that the vendor
// shipping the ID is shipping *glasses* — Meta and Amazon in particular put
// the same ID in phones, speakers and VR headsets. Hence mid-range scores.

struct CompanyRule { uint16_t id; const char* vendor; uint8_t score; uint8_t tier; };

static const CompanyRule COMPANY_RULES[] = {
  { 0x058E, "Meta Platforms Technologies", 55, TIER_META  },
  { 0x01AB, "Meta Platforms",              55, TIER_META  },
  // EssilorLuxottica makes the Ray-Ban and Oakley frames themselves.
  // NB: one widely-copied public CSV mislabels 0x0D53 as Snap; the SIG
  // registry and that project's own source both say Luxottica.
  { 0x0D53, "Luxottica Group",             55, TIER_META  },

  { 0x03C2, "Snapchat",                    55, TIER_BROAD },
  { 0x060C, "Vuzix",                       65, TIER_BROAD }, // only ships eyewear
  { 0x10F9, "Even Realities",              65, TIER_BROAD }, // only ships eyewear
  { 0x08F2, "Microoled",                   60, TIER_BROAD }, // Engo, AR optics
  { 0x0562, "Thalmic Labs / North",        60, TIER_BROAD }, // North Focals
  // Deliberately sub-threshold. Amazon ships 0x0171 across its whole fleet
  // (Echo, Fire TV, Kindle, tags) and Bose 0x009E across every headphone,
  // so alone these are noise. Both products are also cameraless.
  { 0x0171, "Amazon",                      30, TIER_BROAD },
  { 0x009E, "Bose",                        25, TIER_BROAD },
  // Jieli is the silicon in a lot of cheap no-brand camera glasses, but also
  // in countless generic earbuds. Corroboration only, never alone.
  { 0x05D6, "Zhuhai Jieli (chipset)",      20, TIER_BROAD },
  { 0x0BC6, "TCL",                         30, TIER_BROAD }, // RayNeo parent + phones/TVs
  { 0x0040, "Seiko Epson",                 30, TIER_BROAD }, // Moverio + printers
};

// ------------------------------------------------------------ MAC OUIs
// 24-bit IEEE prefixes. These only ever match devices advertising with a
// PUBLIC address — anything using a random/resolvable private address is
// skipped, since its top bytes are meaningless (and rotate).
//
// Populated from the IEEE registry; see docs/fingerprints.md for the
// per-entry provenance. An empty table is handled correctly.

// `nib` holds the 4th byte's high nibble for 28-bit IEEE MA-M blocks, or -1
// for a full 24-bit MA-L block. The distinction matters: Nreal, RayNeo, Solos
// and Halliday only own MA-M blocks, and each of their 24-bit prefixes is
// shared with ~15 unrelated companies. Matching 24 bits there would be almost
// entirely false positives, so we match the full 28.
struct OuiRule { uint8_t b[3]; int8_t nib; const char* vendor; uint8_t score; uint8_t tier; };

static const OuiRule OUI_RULES[] = {
  // Meta Platforms, Inc. — MA-L. No block is registered to "Meta Platforms
  // Technologies"; Reality Labs hardware inherits these.
  { { 0x48, 0x05, 0x60 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0xCC, 0xA1, 0x74 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0xC0, 0xDD, 0x8A }, -1, "Meta Platforms", 45, TIER_META },
  { { 0xD0, 0xB3, 0xC2 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x88, 0x25, 0x08 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x94, 0xF9, 0x29 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0xD4, 0xD6, 0x59 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x78, 0xC4, 0xFA }, -1, "Meta Platforms", 45, TIER_META },
  { { 0xB4, 0x17, 0xA8 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x50, 0x99, 0x03 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x80, 0xF3, 0xEF }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x84, 0x57, 0xF7 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0xF4, 0x4E, 0x35 }, -1, "Meta Platforms", 45, TIER_META },
  { { 0x48, 0x57, 0xDD }, -1, "Facebook",       45, TIER_META },
  { { 0xA4, 0x0E, 0x2B }, -1, "Facebook",       45, TIER_META },
  { { 0x2C, 0x26, 0x17 }, -1, "Oculus VR",      45, TIER_META },
  // EssilorLuxottica — the frames themselves
  { { 0x98, 0x59, 0x49 }, -1, "Luxottica Group", 45, TIER_META },
  { { 0x80, 0xAA, 0x1C }, -1, "Luxottica Tristar", 45, TIER_META },
  { { 0x38, 0x47, 0x12 }, -1, "Luxottica Tristar", 45, TIER_META },

  { { 0x98, 0xA4, 0x0E }, -1, "Snap",          45, TIER_BROAD },
  { { 0x98, 0xDA, 0x92 }, -1, "Vuzix",         50, TIER_BROAD },
  { { 0x60, 0x99, 0xD1 }, -1, "Vuzix/Lenovo",  50, TIER_BROAD },
  { { 0x60, 0x4B, 0xAA }, -1, "Magic Leap",    40, TIER_BROAD },

  // 28-bit MA-M blocks — the nibble is load-bearing, see above.
  { { 0xFC, 0xD2, 0xB6 }, 0xA, "Nreal/Xreal",  45, TIER_BROAD },
  { { 0x00, 0x6A, 0x5E }, 0x5, "RayNeo",       45, TIER_BROAD },
  { { 0xE8, 0x78, 0x29 }, 0xE, "Solos",        45, TIER_BROAD },
  { { 0x38, 0x05, 0x25 }, 0xD, "Halliday",     45, TIER_BROAD },
};
// Only ever consulted for devices advertising a PUBLIC address. In practice
// that is a minority: Meta glasses in particular advertise from a random
// static address, so treat OUI as a bonus signal, not a primary one.
#define OUI_RULES_ACTIVE 1

// -------------------------------------------------------- service UUIDs
// Advertised GATT service UUIDs, as lowercase strings. 16-bit UUIDs are
// compared in their short form ("fd5a"), 128-bit in full dashed form.
// Members-only 16-bit UUIDs in the 0xFDxx range are allocated to a single
// company by the SIG, which makes them strong evidence when present.

// SIG "member" UUIDs in the 0xFDxx/0xFExx range are allocated to exactly one
// company, which makes them strong evidence when they appear. 0xFD5F is the
// single highest-value Meta signal available.
struct Uuid16Rule { uint16_t uuid; const char* vendor; uint8_t score; uint8_t tier; };

static const Uuid16Rule UUID16_RULES[] = {
  { 0xFD5F, "Meta Platforms Technologies", 70, TIER_META  },
  { 0xFEB7, "Meta Platforms",              60, TIER_META  },
  { 0xFEB8, "Meta Platforms",              60, TIER_META  },
  { 0xFE45, "Snap Spectacles",             65, TIER_BROAD },
  { 0xFDD2, "Bose AR",                     45, TIER_BROAD },
  // Vendor-chosen value in unassigned space, single-source. Distinctive in
  // practice, but undocumented and free to change between firmware versions.
  { 0x9100, "Rokid",                       45, TIER_BROAD },
  { 0xFD41, "Amazon Lab126",               35, TIER_BROAD },
};

struct Uuid128Rule { const char* uuid; const char* vendor; uint8_t score; uint8_t tier; };

static const Uuid128Rule UUID128_RULES[] = {
  { "7a230001-5475-a6a4-654c-8431f6ad49c4", "Brilliant Labs Frame", 70, TIER_BROAD },
};
#define UUID_RULES_ACTIVE 1

// ---------------------------------------------------------------- sizes
#define N_NAME_RULES    (sizeof(NAME_RULES)    / sizeof(NAME_RULES[0]))
#define N_COMPANY_RULES (sizeof(COMPANY_RULES) / sizeof(COMPANY_RULES[0]))
#define N_OUI_RULES     (sizeof(OUI_RULES)     / sizeof(OUI_RULES[0]))
#define N_UUID16_RULES  (sizeof(UUID16_RULES)  / sizeof(UUID16_RULES[0]))
#define N_UUID128_RULES (sizeof(UUID128_RULES) / sizeof(UUID128_RULES[0]))

static inline bool tierEnabled(uint8_t tier) {
#if ENABLE_BROAD_VENDORS
  (void)tier; return true;
#else
  return tier == TIER_META;
#endif
}
