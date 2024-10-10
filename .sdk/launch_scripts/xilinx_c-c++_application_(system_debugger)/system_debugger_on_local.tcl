connect -url tcp:127.0.0.1:3121
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-SMT2 210251A08870" && level==0} -index 0
fpga -file C:/Users/LIUYAOHUI/Desktop/1249_3/top0822c_hw_platform_0/top0928.bit
configparams mdm-detect-bscan-mask 2
targets -set -nocase -filter {name =~ "microblaze*#0" && bscan=="USER2"  && jtag_cable_name =~ "Digilent JTAG-SMT2 210251A08870"} -index 0
rst -system
after 3000
targets -set -nocase -filter {name =~ "microblaze*#0" && bscan=="USER2"  && jtag_cable_name =~ "Digilent JTAG-SMT2 210251A08870"} -index 0
dow C:/Users/LIUYAOHUI/Desktop/1249_3/1249/Debug/1249.elf
bpadd -addr &main
