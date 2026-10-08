"""
GPSSpeed v4 board -- single source of truth for the netlist.

Everything else (schematic, PCB, BOM, CPL) is generated from PARTS below, so
the schematic and the board can never drift apart.

Pins are referenced by NAME wherever the symbol has meaningful names, and by
NUMBER for 2-terminal passives. This matters: the red and green LEDs from JLC
have OPPOSITE pin numbering (red: 1=A 2=K, green: 1=K 2=A), so wiring LEDs by
pin number would put one of them in backwards.

Block overview
--------------
  J1 screw terminal: 1 = +12V in, 2 = GND, 3 = SIGNAL out to PerfectPass
  Power:   F1 60V polyfuse -> D3 1.5kW bidirectional TVS (clamps both
           polarities at the connector) -> D1 100V reverse-polarity Schottky
           -> C16 100V bulk electrolytic + 100V ceramics -> U1 LMR38010 buck
           (4.2-80V in, 85V abs max, 3.3V / 1A out).
           USB-C VBUS also feeds the buck through D2, so the board runs from
           either the boat or a laptop (diode-OR).
           Rated 12/24V DC nominal, 32V continuous max (TVS standoff is 36V).
           rev 1.1: input stage redesigned after external EE review -- the
           AP63203's 35V abs max sat too close to the old TVS clamp.
           rev 1.2: second review -- C16 to 100V, 4th input ceramic, 220k sense
           resistor, 60V output MOSFET + 33V drain TVS, split paste on U1's EP.
           rev 1.3: third review -- SIGNAL wired to +12V would have burned the
           2N7002K; Q1 is now a protected switch that current-limits instead.
  Brain:   U2 ESP32-C3-MINI-1 (WiFi AP for the boot window + OTA)
  GPS:     U3 ATGM336H-5N31, J2 u.FL for the existing active antenna,
           antenna bias from VCC_RF through L2. L2 is a Murata LQW18AN27NG
           wirewound (SRF 3.7GHz, characterized at L1) rather than the
           datasheet's generic 47nH, whose 1.2GHz SRF sits below 1.575GHz.
  Output:  Q1 ZXMS6004FF 60V self-protected low-side switch (current limit,
           thermal shutdown, active clamp) -- same role as the IRLZ44N on the old hat,
           D5 series Schottky so a swapped +12V/GND can't back-drive the dash
           input through Q1's body diode (adds ~0.3V to the low level, like a
           real Hall sensor), D4 33V TVS clamps the drain (safe with a 24V
           pull-up on the line), optional 4.7k pull-up
           to +12V via the
           JP1 solder jumper (ships OPEN -- a real Airmar paddlewheel is
           open-collector and the dash provides the pull-up).
"""

# --------------------------------------------------------------------------
# Nets
# --------------------------------------------------------------------------
GND      = "GND"
VIN12    = "+12V_IN"     # straight off the screw terminal
VIN_F    = "+12V_F"      # after the polyfuse
VBUCK    = "VIN_BUCK"    # after the reverse-polarity / USB OR-ing diodes
BUCK_RT  = "BUCK_RT"
BUCK_FB  = "BUCK_FB"
SIG_DRAIN = "SIGNAL_DRAIN"  # MOSFET drain, behind the D5 series diode
VBUS     = "VBUS"        # USB-C 5V
V3V3     = "+3V3"
V3V3_GPS = "+3V3_GPS"    # ferrite-filtered supply for the GPS (<50mVpp ripple)
SW       = "BUCK_SW"
BST      = "BUCK_BST"
EN       = "ESP_EN"
BOOT     = "ESP_BOOT"    # GPIO9
STRAP2   = "ESP_IO2"
STRAP8   = "ESP_IO8"
USB_DP   = "USB_D+"
USB_DN   = "USB_D-"
CC1      = "USB_CC1"
CC2      = "USB_CC2"
GPS_TX   = "GPS_TX"      # GPS TXD -> ESP RX (GPIO20 / RXD0)
GPS_RX   = "GPS_RX"      # ESP TX (GPIO21 / TXD0) -> GPS RXD
GPS_PPS  = "GPS_PPS"
GPS_NRST = "GPS_NRST"
GPS_ONOFF= "GPS_ON"
RF_ANT   = "GPS_RF"
VCC_RF   = "GPS_VCC_RF"
VSENSE   = "VIN_SENSE"
PULSE    = "PULSE_IO"    # ESP GPIO10 (LEDC hardware PWM)
GATE     = "PULSE_GATE"
SIG_OUT  = "SIGNAL_OUT"  # J1 pin 3 -> PerfectPass paddlewheel input
PU_EN    = "PULLUP_JP"
LED_FIX  = "LED_FIX_IO"
LED_WIFI = "LED_WIFI_IO"
LED_FIX_A  = "LED_FIX_A"
LED_WIFI_A = "LED_WIFI_A"
LED_PWR_A  = "LED_PWR_A"
IO6      = "ESP_IO6"
IO7      = "ESP_IO7"

POWER_NETS = {GND, VIN12, VIN_F, VBUCK, VBUS, V3V3, V3V3_GPS, SW}

# ESP32-C3 GPIO map -- mirrored as constants in firmware/gpsspeed_v4/pins.h
GPIO = {
    "GPS_NRST": 0, "GPS_PPS": 1, "VSENSE": 3,
    "LED_FIX": 4, "LED_WIFI": 5, "SPARE6": 6, "SPARE7": 7,
    "BOOT": 9, "PULSE": 10, "GPS_RX_IN": 20, "GPS_TX_OUT": 21,
}

# --------------------------------------------------------------------------
# Parts
#   sym  : symbol in lib/gpsspeed.kicad_sym, or "Lib:Name" from KiCad stock
#   fp   : only needed for stock-library symbols (JLC symbols carry their own)
#   lcsc : JLC part number -- None means "not assembled by JLC" (test points,
#          mounting holes, solder jumper)
#   pins : pin name or number -> net.  Anything unlisted is marked no-connect.
#   grp  : schematic block the symbol is drawn in
# --------------------------------------------------------------------------
R = lambda ref, sym, lcsc, val, a, b, grp: dict(
    ref=ref, sym=sym, lcsc=lcsc, value=val, pins={"1": a, "2": b}, grp=grp)

PARTS = [
    # ---------------- Boat connector ----------------
    dict(ref="J1", sym="KF301-5.0-3P", lcsc="C474882", value="12V / GND / SIG",
         pins={"1": VIN12, "2": GND, "3": SIG_OUT}, grp="conn"),

    # ---------------- Power input + protection ----------------
    dict(ref="F1", sym="1812L050_60GR", lcsc="C19078719", value="500mA PTC 60V",
         pins={"1": VIN12, "2": VIN_F}, grp="power"),
    # Bidirectional, right at the connector: clamps positive surges at ~58V
    # (well under the buck's 85V abs max) and negative spikes at ~-58V (well
    # under D1's 100V). 36V standoff = never conducts on a 12V or 24V system.
    dict(ref="D3", sym="SMCJ36CA_C19077609", lcsc="C19077609", value="SMCJ36CA 1.5kW",
         pins={"1": VIN_F, "2": GND}, grp="power"),
    dict(ref="D1", sym="SS310_C7420364", lcsc="C7420364", value="SS310 rev-pol",
         pins={"A": VIN_F, "K": VBUCK}, grp="power"),
    dict(ref="D2", sym="SS310_C7420364", lcsc="C7420364", value="SS310 USB OR",
         pins={"A": VBUS, "K": VBUCK}, grp="power"),
    # Bulk electrolytic after D1 (so reverse wiring can't reverse-bias it):
    # damps harness ringing. 100V so the 58V TVS clamp is well inside its
    # rating (rev 1.2; was 47uF/63V). Same series and 6.3x7.7 can. Pad 1 is +.
    R("C16", "RV100V22M6X8", "C48971012", "22uF 100V", VBUCK, GND, "power"),
    # LMR38010 wants >=4.7uF effective ceramic rated past max VIN, plus
    # 100nF 100V right at the pins (datasheet 9.2.2.6). Four 2.2uF so the
    # DC-bias-derated total stays above 4.7uF at 24-32V (rev 1.2).
    R("C1", "CL31B225KCHSNNE", "C170101", "2.2uF 100V", VBUCK, GND, "power"),
    R("C2", "CL31B225KCHSNNE", "C170101", "2.2uF 100V", VBUCK, GND, "power"),
    R("C14", "CL31B225KCHSNNE", "C170101", "2.2uF 100V", VBUCK, GND, "power"),
    R("C17", "CL31B225KCHSNNE", "C170101", "2.2uF 100V", VBUCK, GND, "power"),
    R("C3", "CL21B104KCFNNNE", "C28233", "100nF 100V", VBUCK, GND, "power"),

    # ---------------- 4.2-80V -> 3.3V buck (LMR38010, datasheet 9.2) -----
    dict(ref="U1", sym="LMR38010SDDAR", lcsc="C5219310", value="LMR38010SDDAR",
         pins={"VIN": VBUCK, "EN": VBUCK, "GND": GND, "EP": GND, "SW": SW,
               "BOOT": BST, "FB": BUCK_FB, "RT/SYNC": BUCK_RT}, grp="power"),
    R("C4", "CC0603KRX7R9BB104", "C14663", "100nF", BST, SW, "power"),
    # 51k -> ~513 kHz (Eq.2). At the 36V standoff, t_on = 180ns > 131ns max
    # t_on_min, so no frequency foldback in normal operation.
    R("R16", "0603WAF5102T5E", "C23196", "51k", BUCK_RT, GND, "power"),
    # Vout = 1.0V x (1 + 47k/20k) = 3.35V. Both basic parts (43k would add a fee).
    R("R17", "0603WAF4702T5E", "C25819", "47k", V3V3, BUCK_FB, "power"),
    R("R18", "0603WAF2002T5E", "C4184", "20k", BUCK_FB, GND, "power"),
    # 15uH: ~34% ripple at 14V in; 2.2A sat > 1.9A max high-side limit.
    R("L1", "FNR5040S150MT", "C167969", "15uH 2.2A", SW, V3V3, "power"),
    R("C5", "CL21A226MAQNNNE", "C45783", "22uF", V3V3, GND, "power"),
    R("C6", "CL21A226MAQNNNE", "C45783", "22uF", V3V3, GND, "power"),
    R("C15", "CL21A226MAQNNNE", "C45783", "22uF", V3V3, GND, "power"),

    # ---------------- Battery voltage sense (220k/10k, x23) ---------------
    # Tapped after D1 so it never sees negative spikes; reads ~0.35V low on
    # boat power (firmware adds the diode drop back). 32V -> 1.4V, and the
    # 58V clamp -> 2.5V, inside the ESP32-C3 ADC's accurate range (rev 1.2).
    R("R1", "0603WAF2203T5E", "C22961", "220k", VBUCK, VSENSE, "power"),
    R("R2", "0603WAF1002T5E", "C25804", "10k", VSENSE, GND, "power"),
    R("C7", "CC0603KRX7R9BB104", "C14663", "100nF", VSENSE, GND, "power"),

    # ---------------- USB-C (programming + bench power) ----------------
    dict(ref="J3", sym="TYPE-C16PIN2MD", lcsc="C2765186", value="USB-C",
         pins={"VBUS": VBUS, "GND": GND, "EH": GND, "CC1": CC1, "CC2": CC2,
               "Dp1": USB_DP, "Dp2": USB_DP, "Dn1": USB_DN, "Dn2": USB_DN},
         grp="usb"),
    R("R3", "0603WAF5101T5E", "C23186", "5.1k", CC1, GND, "usb"),
    R("R4", "0603WAF5101T5E", "C23186", "5.1k", CC2, GND, "usb"),
    dict(ref="U4", sym="USBLC6-2SC6_C2687116", lcsc="C2687116", value="USBLC6-2SC6",
         pins={"I/O1": USB_DP, "I/O2": USB_DN, "GND": GND, "VBUS": VBUS},
         grp="usb"),

    # ---------------- ESP32-C3 (datasheet section 9 reference) ----------
    # Ordered as the -H4 (C2934569): same footprint/pinout/4 MB flash, rated
    # to 105 C. The -N4 (C2838502) is flagged "Standard PCBA only" at JLC.
    dict(ref="U2", sym="ESP32-C3-MINI-1-N4", lcsc="C2934569", value="ESP32-C3-MINI-1-H4",
         pins={"GND": GND, "3V3": V3V3, "EN": EN,
               "IO0": GPS_NRST, "IO1": GPS_PPS, "IO2": STRAP2, "IO3": VSENSE,
               "IO4": LED_FIX, "IO5": LED_WIFI, "IO6": IO6, "IO7": IO7,
               "IO8": STRAP8, "IO9": BOOT, "IO10": PULSE,
               "IO18": USB_DN, "IO19": USB_DP,
               "RXD0": GPS_TX, "TXD0": GPS_RX},
         grp="mcu"),
    R("C8", "CL21A106KAYNNNE", "C15850", "10uF", V3V3, GND, "mcu"),
    R("C9", "CC0603KRX7R9BB104", "C14663", "100nF", V3V3, GND, "mcu"),
    # EN RC delay: 10k + 1uF (Espressif recommendation)
    R("R5", "0603WAF1002T5E", "C25804", "10k", V3V3, EN, "mcu"),
    R("C10", "CL10A105KB8NNNC", "C15849", "1uF", EN, GND, "mcu"),
    # Strapping pins: GPIO2 and GPIO8 float by default -> pull high for SPI boot
    R("R6", "0603WAF1002T5E", "C25804", "10k", V3V3, STRAP2, "mcu"),
    R("R7", "0603WAF1002T5E", "C25804", "10k", V3V3, STRAP8, "mcu"),
    R("R8", "0603WAF1002T5E", "C25804", "10k", V3V3, BOOT, "mcu"),
    # TS-1187A: A-B shorted internally, C-D shorted, switch between the pairs
    dict(ref="SW1", sym="TS-1187A-B-A-B", lcsc="C318884", value="RESET",
         pins={"A": EN, "B": EN, "C": GND, "D": GND}, grp="mcu"),
    dict(ref="SW2", sym="TS-1187A-B-A-B", lcsc="C318884", value="BOOT",
         pins={"A": BOOT, "B": BOOT, "C": GND, "D": GND}, grp="mcu"),

    # ---------------- Status LEDs ----------------
    # Red Vf ~2.0V -> 1k.  Green Vf ~2.7-3.1V at low current -> 100R so it is
    # still visible off a 3.3V GPIO.
    R("R9",  "0603WAF1001T5E", "C21190", "1k", V3V3, LED_PWR_A, "led"),
    dict(ref="LED1", sym="KT-0603R", lcsc="C2286", value="PWR red",
         pins={"A": LED_PWR_A, "K": GND}, grp="led"),
    R("R10", "0603WAF1000T5E", "C22775", "100R", LED_FIX, LED_FIX_A, "led"),
    dict(ref="LED2", sym="0603Green509-620mcd", lcsc="C12624", value="GPS FIX green",
         pins={"A": LED_FIX_A, "K": GND}, grp="led"),
    R("R11", "0603WAF1000T5E", "C22775", "100R", LED_WIFI, LED_WIFI_A, "led"),
    dict(ref="LED3", sym="0603Green509-620mcd", lcsc="C12624", value="WIFI green",
         pins={"A": LED_WIFI_A, "K": GND}, grp="led"),

    # ---------------- GPS (ATGM336H datasheet 2.7.1 active antenna) ------
    R("FB1", "GZ1608D601TF", "C1002", "600R@100MHz", V3V3, V3V3_GPS, "gps"),
    R("C11", "CL21A106KAYNNNE", "C15850", "10uF", V3V3_GPS, GND, "gps"),
    R("C12", "CC0603KRX7R9BB104", "C14663", "100nF", V3V3_GPS, GND, "gps"),
    dict(ref="U3", sym="ATGM336H-5N31", lcsc="C90770", value="ATGM336H-5N31",
         pins={"GND": GND, "TXD": GPS_TX, "RXD": GPS_RX, "1PPS": GPS_PPS,
               "ON/OFF": GPS_ONOFF, "VBAT": V3V3_GPS, "VCC": V3V3_GPS,
               "NRESET": GPS_NRST, "RF_IN": RF_ANT, "VCC_RF": VCC_RF},
         grp="gps"),
    # ON/OFF is "low level effective" shutdown -> hold it high
    R("R12", "0603WAF1002T5E", "C25804", "10k", V3V3_GPS, GPS_ONOFF, "gps"),
    # Active antenna bias: VCC_RF -> L2 -> antenna line. Murata wirewound,
    # SRF 3.7 GHz, so it is genuinely inductive (~270 ohm) at GPS L1 -- the
    # generic 47nH it replaced self-resonated at 1.2 GHz. Remove L2 to run
    # a passive antenna. VCC_RF is current-limited to 50mA by the module.
    R("L2", "LQW18AN27NG00D", "C148128", "27nH LQW18AN", VCC_RF, RF_ANT, "gps"),
    dict(ref="J2", sym="U.FL-R-SMT-1", lcsc="C88374", value="u.FL GPS ANT",
         pins={"SIG": RF_ANT, "GND": GND}, grp="gps"),

    # ---------------- Paddlewheel output stage ----------------
    R("R13", "0603WAF1000T5E", "C22775", "100R", PULSE, GATE, "out"),
    # Gate pull-down: MOSFET stays OFF while the ESP32 boots / is being flashed
    R("R14", "0603WAF1003T5E", "C25803", "100k", GATE, GND, "out"),
    # Self-protected low-side switch (rev 1.3; was a bare 2N7002K). If SIGNAL
    # is ever wired to +12V, Q1 current-limits at 0.7-1.7A and thermal-cycles
    # instead of burning (short-circuit protected to VDS 36V > our 32V max).
    # Specified for 3.3V logic: VIH 3V, 0.6 ohm max at VIN=3V. Active clamp at
    # 60-70V behind D4. Pins: 1 = IN, 2 = S, 3 = D (same order as a MOSFET).
    dict(ref="Q1", sym="ZXMS6004FFTA", lcsc="C95063", value="ZXMS6004FF 60V prot",
         pins={"G": GATE, "S": GND, "D": SIG_DRAIN}, grp="out"),
    # Series Schottky: conducts when Q1 sinks the line, blocks the reverse
    # current a swapped +12V/GND would push through Q1's body diode.
    dict(ref="D5", sym="SS310_C7420364", lcsc="C7420364", value="SS310 sig block",
         pins={"A": SIG_OUT, "K": SIG_DRAIN}, grp="out"),
    # Drain clamp: 33V standoff never conducts on a 24V (28.8V charging)
    # pull-up; 53.3V max clamp < Q1's 60V min active clamp. Pin 1 = K (bar), same SOD-123FL
    # land pattern as the SMF15A it replaced.
    dict(ref="D4", sym="SMF33A_C726914", lcsc="C726914", value="SMF33A TVS",
         pins={"1": SIG_DRAIN, "2": GND}, grp="out"),
    # Optional pull-up to +12V. JP1 ships OPEN; bridge it only if the dash
    # does not provide its own pull-up.
    R("R15", "0603WAF4701T5E", "C23162", "4.7k", SIG_OUT, PU_EN, "out"),
    dict(ref="JP1", sym="Jumper:SolderJumper_2_Open", lcsc=None, value="PULLUP (open)",
         fp="Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm",
         pins={"1": PU_EN, "2": VIN_F}, grp="out"),

    # ---------------- Test points (not assembled) ----------------
    dict(ref="TP1", sym="Connector:TestPoint", lcsc=None, value="GND",
         fp="TestPoint:TestPoint_Pad_D1.5mm", pins={"1": GND}, grp="tp"),
    dict(ref="TP2", sym="Connector:TestPoint", lcsc=None, value="3V3",
         fp="TestPoint:TestPoint_Pad_D1.5mm", pins={"1": V3V3}, grp="tp"),
    dict(ref="TP3", sym="Connector:TestPoint", lcsc=None, value="PULSE",
         fp="TestPoint:TestPoint_Pad_D1.5mm", pins={"1": GATE}, grp="tp"),
    dict(ref="TP4", sym="Connector:TestPoint", lcsc=None, value="GPS_TX",
         fp="TestPoint:TestPoint_Pad_D1.5mm", pins={"1": GPS_TX}, grp="tp"),
    dict(ref="TP5", sym="Connector:TestPoint", lcsc=None, value="IO6",
         fp="TestPoint:TestPoint_Pad_D1.5mm", pins={"1": IO6}, grp="tp"),
    dict(ref="TP6", sym="Connector:TestPoint", lcsc=None, value="IO7",
         fp="TestPoint:TestPoint_Pad_D1.5mm", pins={"1": IO7}, grp="tp"),

    # ---------------- Mounting ----------------
    dict(ref="H1", sym="Mechanical:MountingHole", lcsc=None, value="M3",
         fp="MountingHole:MountingHole_3.2mm_M3", pins={}, grp="mech"),
    dict(ref="H2", sym="Mechanical:MountingHole", lcsc=None, value="M3",
         fp="MountingHole:MountingHole_3.2mm_M3", pins={}, grp="mech"),
    dict(ref="H3", sym="Mechanical:MountingHole", lcsc=None, value="M3",
         fp="MountingHole:MountingHole_3.2mm_M3", pins={}, grp="mech"),
    dict(ref="H4", sym="Mechanical:MountingHole", lcsc=None, value="M3",
         fp="MountingHole:MountingHole_3.2mm_M3", pins={}, grp="mech"),
]
