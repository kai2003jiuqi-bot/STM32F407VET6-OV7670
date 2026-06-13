arm-none-eabi-objcopy -O ihex build/Debug/project.elf build/Debug/project.hex
STM32_Programmer_CLI.exe --connect port=COM3 baudrate=115200 --write build/Debug/project.hex --verify
STM32_Programmer_CLI.exe --connect port=COM3 baudrate=115200 --erase all

cls
openocd.exe -f interface/stlink.cfg -f target/stm32f4x.cfg -c "transport select swd" -c "program build/Debug/project.elf verify reset" -c "shutdown"

STM32_Programmer_CLI.exe --connect port=COM8 baud=460800 --write build/Debug/project.hex --verify

arm-none-eabi-objcopy -O ihex build/Debug/project.elf build/Debug/project.hex
STM32_Programmer_CLI.exe --c port=COM8 mode=UART br=460800 -w build/Debug/project.hex -v

STM32_Programmer_CLI.exe --c port=COM8,mode=UART,baud=460800 -w build/Debug/project.hex -v

STM32_Programmer_CLI.exe --connect port=COM8 br=230400

STM32_Programmer_CLI.exe --connect port=COM8 br=460800 --write build/Debug/project.hex --verify