(Exported by FreeCAD)
(Post Processor: grbl_post)
(Output Time:2026-03-01 19:50:12.415868)
(Begin preamble)
G17 G90
G21
(Begin operation: Fixture)
(Path: Fixture)
G54
(Finish operation: Fixture)
(Begin operation: TC: 1/4" Endmill)
(Path: TC: 1/4" Endmill)
(TC: 1/4" Endmill)
(Begin toolchange)
( M6 T4 )
M3 S18000
(Finish operation: TC: 1/4" Endmill)
(Begin operation: Profile002)
(Path: Profile002)
(Profile002)
(Compensated Tool Path. Diameter: 6.35)
G0 Z5.000
G0 X153.055 Y73.670
G0 Z3.000
G1 X153.055 Y73.670 Z-2.100 F400.000
G2 X153.985 Y71.424 Z-2.100 I-2.245 J-2.245 K0.000 F400.000
G1 X153.985 Y0.024 Z-2.100 F400.000
G2 X150.810 Y-3.151 Z-2.100 I-3.175 J0.000 K0.000 F400.000
G1 X0.025 Y-3.151 Z-2.100 F400.000
G2 X-3.150 Y0.024 Z-2.100 I0.000 J3.175 K0.000 F400.000
G1 X-3.150 Y71.424 Z-2.100 F400.000
G2 X0.025 Y74.599 Z-2.100 I3.175 J-0.000 K0.000 F400.000
G1 X150.810 Y74.599 Z-2.100 F400.000
G2 X153.055 Y73.670 Z-2.100 I-0.000 J-3.175 K0.000 F400.000
G0 Z5.000
G0 Z5.000
(Finish operation: Profile002)
(Begin postamble)
M5
G17 G90
M2
