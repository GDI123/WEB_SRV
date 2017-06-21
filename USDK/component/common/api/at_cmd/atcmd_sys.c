#include <platform_opts.h>
#ifdef CONFIG_AT_SYS

#include "platform_stdlib.h"
//#include "platform_autoconf.h"
//#include "main.h"
#include "autoconf.h"
#include "hal_adc.h"
#include "gpio_api.h"   // mbed
#include "sys_api.h"
#include "rtl8195a.h"
#include "flash_api.h"
#include "rtl_lib.h"
#include "build_info.h"
#include "analogin_api.h"
#include "log_service.h"
#include "atcmd_sys.h"
#include "osdep_api.h"
#include "atcmd_wifi.h"
#include "tcm_heap.h"
#if CONFIG_OTA_UPDATE
#include "update.h"
#endif

#ifndef ATCMD_VER
#define ATVER_1 1
#define ATVER_2 2
#define ATCMD_VER ATVER_2
#if CONFIG_EXAMPLE_UART_ATCMD
#define ATCMD_VER ATVER_2
#else
#define ATCMD_VER ATVER_1
#endif
#endif

#if defined(configUSE_WAKELOCK_PMU) && (configUSE_WAKELOCK_PMU == 1)
#include "freertos_pmu.h"
#endif

extern u32 ConfigDebugErr;
extern u32 ConfigDebugInfo;
extern u32 ConfigDebugWarn;
extern u32 CmdDumpWord(IN u16 argc, IN u8 *argv[]);
extern u32 CmdWriteWord(IN u16 argc, IN u8 *argv[]);
#if CONFIG_UART_XMODEM
extern void OTU_FW_Update(u8, u8, u32);
#endif

struct _dev_id2name {
	u8 id;
	u8 *name;
};

struct _dev_id2name dev_id2name[] = { { UART0, "UART0" }, { UART1, "UART1" }, {
UART2, "UART2" }, { SPI0, "SPI0" }, { SPI1, "SPI1" }, { SPI2, "SPI2" }, {
SPI0_MCS, "SPI0_MCS" }, { I2C0, "I2C0" }, { I2C1, "I2C1" }, { I2C2, "I2C2" }, {
I2C3, "I2C3" }, { I2S0, "I2S0" }, { I2S1, "I2S1" }, { PCM0, "PCM0" }, {
PCM1, "PCM1" }, { ADC0, "ADC0" }, { DAC0, "DAC0" }, { DAC1, "DAC1" }, {
SDIOD, "SDIOD" }, { SDIOH, "SDIOH" }, { USBOTG, "USBOTG" }, { MII, "MII" }, {
WL_LED, "WL_LED" }, { WL_ANT0, "WL_ANT0" }, { WL_ANT1, "WL_ANT1" }, {
WL_BTCOEX, "WL_BTCOEX" }, { WL_BTCMD, "WL_BTCMD" }, { NFC, "NFC" }, {
PWM0, "PWM0" }, { PWM1, "PWM1" }, { PWM2, "PWM2" }, { PWM3, "PWM3" }, {
ETE0, "ETE0" }, { ETE1, "ETE1" }, { ETE2, "ETE2" }, { ETE3, "ETE3" }, {
EGTIM, "EGTIM" }, { SPI_FLASH, "SPI_FLASH" }, { SDR, "SDR" }, { JTAG, "JTAG" },
		{ TRACE, "TRACE" }, { LOG_UART, "LOG_UART" }, {
		LOG_UART_IR, "LOG_UART_IR" }, { SIC, "SIC" }, { EEPROM, "EEPROM" }, {
		DEBUG, "DEBUG" }, { 255, "" } };

void fATSI(void *arg) {
	uint32 x = 0;
	int i;
	u8 * s;
	for (i = 0; dev_id2name[i].id != 255; i++) {
		ReadHWPwrState(dev_id2name[i].id, &x);
		s = "?";
		switch (x) {
		case HWACT:
			s = "ACT";
			break;
		case HWCG:
			s = "CG";
			break;
		case HWINACT:
			s = "WACT";
			break;
		case UNDEF:
			s = "UNDEF";
			break;
		case ALLMET:
			s = "ALLMET";
			break;
		}
		printf("Dev %s, state = %s\n", dev_id2name[i].name, s);
	}
	for (i = 0; i < _PORT_MAX; i++)
		printf("Port %c state: 0x%04x\n", i + 'A', GPIOState[i]);
}

//-------- AT SYS commands ---------------------------------------------------------------
void fATSD(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };
	AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS,
			"[ATSD]: _AT_SYSTEM_DUMP_REGISTER_");
	if (!arg) {
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSD] Usage: ATSD=REGISTER");
		return;
	}
	argc = parse_param(arg, argv);
	if (argc == 2 || argc == 3)
		CmdDumpWord(argc - 1, (unsigned char**) (argv + 1));
}

#if ATCMD_VER == ATVER_2
void fATXD(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };

	AT_DBG_MSG(AT_FLAG_EDIT, AT_DBG_ALWAYS,
			"[ATXD]: _AT_SYSTEM_WRITE_REGISTER_");
	if (!arg) {
		AT_DBG_MSG(AT_FLAG_EDIT, AT_DBG_ALWAYS,
				"[ATXD] Usage: ATXD=REGISTER,VALUE");
		return;
	}
	argc = parse_param(arg, argv);
	if (argc == 3)
		CmdWriteWord(argc - 1, (unsigned char**) (argv + 1));
}
#endif

#if ATCMD_VER == ATVER_1

void fATSC(void *arg)
{
	AT_DBG_MSG(AT_FLAG_OTA, AT_DBG_ALWAYS, "[ATSC]: _AT_SYSTEM_CLEAR_OTA_SIGNATURE_");
	sys_clear_ota_signature();
}

void fATSR(void *arg)
{
	AT_DBG_MSG(AT_FLAG_OTA, AT_DBG_ALWAYS, "[ATSR]: _AT_SYSTEM_RECOVER_OTA_SIGNATURE_");
	sys_recover_ota_signature();
}

#if CONFIG_UART_XMODEM
void fATSY(void *arg)
{
	if (HalGetChipId() < CHIP_ID_8195AM) {
		OTU_FW_Update(0, 0, 115200);
	} else {
		// use xmodem to update, RX: PA_6, TX: PA_7, baudrate: 1M
		OTU_FW_Update(0, 2, 115200);
	}
}
#endif

#if SUPPORT_MP_MODE
void fATSA(void *arg)
{
	u32 tConfigDebugInfo = ConfigDebugInfo;
	int argc = 0, channel;
	char *argv[MAX_ARGC] = {0}, *ptmp;
	u16 offset, gain;

	AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA]: _AT_SYSTEM_ADC_TEST_");
	if(!arg) {
		AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] Usage: ATSA=CHANNEL(0~2)");
		AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] Usage: ATSA=k_get");
		AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] Usage: ATSA=k_set[offet(hex),gain(hex)]");
		return;
	}

	argc = parse_param(arg, argv);
	if(strcmp(argv[1], "k_get") == 0) {
		sys_adc_calibration(0, &offset, &gain);
//		AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] offset = 0x%04X, gain = 0x%04X", offset, gain);
	} else if(strcmp(argv[1], "k_set") == 0) {
		if(argc != 4) {
			AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] Usage: ATSA=k_set[offet(hex),gain(hex)]");
			return;
		}
		offset = strtoul(argv[2], &ptmp, 16);
		gain = strtoul(argv[3], &ptmp, 16);
		sys_adc_calibration(1, &offset, &gain);
//		AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] offset = 0x%04X, gain = 0x%04X", offset, gain);
	} else {
		channel = atoi(argv[1]);
		if(channel < 0 || channel > 2) {
			AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] Usage: ATSA=CHANNEL(0~2)");
			return;
		}
		analogin_t adc;
		u16 adcdat;

		// Remove debug info massage
		ConfigDebugInfo = 0;
		if(channel == 0)
		analogin_init(&adc, AD_1);
		else if(channel == 1)
		analogin_init(&adc, AD_2);
		else
		analogin_init(&adc, AD_3);
		adcdat = analogin_read_u16(&adc)>>4;
		analogin_deinit(&adc);
		// Recover debug info massage
		ConfigDebugInfo = tConfigDebugInfo;

		AT_DBG_MSG(AT_FLAG_ADC, AT_DBG_ALWAYS, "[ATSA] A%d = 0x%04X", channel, adcdat);
	}
}

void fATSG(void *arg)
{
	gpio_t gpio_test;
	int argc = 0, val;
	char *argv[MAX_ARGC] = {0}, port, num;
	PinName pin = NC;
	u32 tConfigDebugInfo = ConfigDebugInfo;

	AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSG]: _AT_SYSTEM_GPIO_TEST_");
	if(!arg) {
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSG] Usage: ATSG=PINNAME(ex:A0)");
		return;
	} else {
		argc = parse_param(arg, argv);
		if(argc != 2) {
			AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSG] Usage: ATSG=PINNAME(ex:A0)");
			return;
		}
	}
	port = argv[1][0];
	num = argv[1][1];
	if(port >= 'a' && port <= 'z')
	port -= ('a' - 'A');
	if(num >= 'a' && num <= 'z')
	num -= ('a' - 'A');
	switch(port) {
		case 'A':
		switch(num) {
			case '0': pin = PA_0; break; case '1': pin = PA_1; break; case '2': pin = PA_2; break; case '3': pin = PA_3; break;
			case '4': pin = PA_4; break; case '5': pin = PA_5; break; case '6': pin = PA_6; break; case '7': pin = PA_7; break;
		}
		break;
		case 'B':
		switch(num) {
			case '0': pin = PB_0; break; case '1': pin = PB_1; break; case '2': pin = PB_2; break; case '3': pin = PB_3; break;
			case '4': pin = PB_4; break; case '5': pin = PB_5; break; case '6': pin = PB_6; break; case '7': pin = PB_7; break;
		}
		break;
		case 'C':
		switch(num) {
			case '0': pin = PC_0; break; case '1': pin = PC_1; break; case '2': pin = PC_2; break; case '3': pin = PC_3; break;
			case '4': pin = PC_4; break; case '5': pin = PC_5; break; case '6': pin = PC_6; break; case '7': pin = PC_7; break;
			case '8': pin = PC_8; break; case '9': pin = PC_9; break;
		}
		break;
		case 'D':
		switch(num) {
			case '0': pin = PD_0; break; case '1': pin = PD_1; break; case '2': pin = PD_2; break; case '3': pin = PD_3; break;
			case '4': pin = PD_4; break; case '5': pin = PD_5; break; case '6': pin = PD_6; break; case '7': pin = PD_7; break;
			case '8': pin = PD_8; break; case '9': pin = PD_9; break;
		}
		break;
		case 'E':
		switch(num) {
			case '0': pin = PE_0; break; case '1': pin = PE_1; break; case '2': pin = PE_2; break; case '3': pin = PE_3; break;
			case '4': pin = PE_4; break; case '5': pin = PE_5; break; case '6': pin = PE_6; break; case '7': pin = PE_7; break;
			case '8': pin = PE_8; break; case '9': pin = PE_9; break; case 'A': pin = PE_A; break;
		}
		break;
		case 'F':
		switch(num) {
			case '0': pin = PF_0; break; case '1': pin = PF_1; break; case '2': pin = PF_2; break; case '3': pin = PF_3; break;
			case '4': pin = PF_4; break; case '5': pin = PF_5; break;
		}
		break;
		case 'G':
		switch(num) {
			case '0': pin = PG_0; break; case '1': pin = PG_1; break; case '2': pin = PG_2; break; case '3': pin = PG_3; break;
			case '4': pin = PG_4; break; case '5': pin = PG_5; break; case '6': pin = PG_6; break; case '7': pin = PG_7; break;
		}
		break;
		case 'H':
		switch(num) {
			case '0': pin = PH_0; break; case '1': pin = PH_1; break; case '2': pin = PH_2; break; case '3': pin = PH_3; break;
			case '4': pin = PH_4; break; case '5': pin = PH_5; break; case '6': pin = PH_6; break; case '7': pin = PH_7; break;
		}
		break;
		case 'I':
		switch(num) {
			case '0': pin = PI_0; break; case '1': pin = PI_1; break; case '2': pin = PI_2; break; case '3': pin = PI_3; break;
			case '4': pin = PI_4; break; case '5': pin = PI_5; break; case '6': pin = PI_6; break; case '7': pin = PI_7; break;
		}
		break;
		case 'J':
		switch(num) {
			case '0': pin = PJ_0; break; case '1': pin = PJ_1; break; case '2': pin = PJ_2; break; case '3': pin = PJ_3; break;
			case '4': pin = PJ_4; break; case '5': pin = PJ_5; break; case '6': pin = PJ_6; break;
		}
		break;
		case 'K':
		switch(num) {
			case '0': pin = PK_0; break; case '1': pin = PK_1; break; case '2': pin = PK_2; break; case '3': pin = PK_3; break;
			case '4': pin = PK_4; break; case '5': pin = PK_5; break; case '6': pin = PK_6; break;
		}
		break;
	}
	if(pin == NC) {
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSG]: Invalid Pin Name");
		return;
	}
	// Remove debug info massage
	ConfigDebugInfo = 0;
	// Initial input control pin
	gpio_init(&gpio_test, pin);
	gpio_dir(&gpio_test, PIN_INPUT);// Direction: Input
	gpio_mode(&gpio_test, PullUp);// Pull-High
	val = gpio_read(&gpio_test);
	// Recover debug info massage
	ConfigDebugInfo = tConfigDebugInfo;
	AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSG] %c%c = %d", port, num, val);
}

void fATSP(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	unsigned long timeout; // ms
	unsigned long time_begin, time_current;

	gpio_t gpiob_1;
	int val_old, val_new;

	int expected_zerocount, zerocount;
	int test_result;

	// parameter check
	AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSP]: _AT_SYSTEM_POWER_PIN_TEST_");
	if(!arg) {
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSP]: Usage: ATSP=gpiob1[timeout,zerocount]");
	} else {
		argc = parse_param(arg, argv);
		if (argc < 2) {
			AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSP]: Usage: ATSP=gpiob1[timeout,zerocount]");
			return;
		}
	}

	if ( strcmp(argv[1], "gpiob1" ) == 0 ) {
		if (argc < 4) {
			AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSP]: Usage: ATSP=gpiob1[timeout,zerocount]");
			return;
		}

		// init gpiob1 test
		test_result = 0;
		timeout = strtoul(argv[2], NULL, 10);
		expected_zerocount = atoi(argv[3]);
		zerocount = 0;
		val_old = 1;

		sys_log_uart_off();

		gpio_init(&gpiob_1, PB_1);
		gpio_dir(&gpiob_1, PIN_INPUT);
		gpio_mode(&gpiob_1, PullDown);

		// gpiob1 test ++
		time_begin = time_current = xTaskGetTickCount();
		while (time_current < time_begin + timeout) {
			val_new = gpio_read(&gpiob_1);

			if (val_new != val_old && val_new == 0) {

				zerocount ++;
				if (zerocount == expected_zerocount) {
					test_result = 1;
					break;
				}
			}

			val_old = val_new;
			time_current = xTaskGetTickCount();
		}
		// gpio test --

		sys_log_uart_on();

		if (test_result == 1) {
			AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSP]: success");
		} else {
			AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSP]: fail, it only got %d zeros", zerocount);
		}
	}
}

int write_otu_to_system_data(flash_t *flash, uint32_t otu_addr)
{
	uint32_t data, i = 0;
	flash_read_word(flash, FLASH_SYSTEM_DATA_ADDR+0xc, &data);
	//printf("\n\r[%s] data 0x%x otu_addr 0x%x", __FUNCTION__, data, otu_addr);
	AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: data 0x%x otu_addr 0x%x", data, otu_addr);
	if(data == ~0x0) {
		flash_write_word(flash, FLASH_SYSTEM_DATA_ADDR+0xc, otu_addr);
	} else {
		//erase backup sector
		flash_erase_sector(flash, FLASH_RESERVED_DATA_BASE);
		//backup system data to backup sector
		for(i = 0; i < 0x1000; i+= 4) {
			flash_read_word(flash, FLASH_SYSTEM_DATA_ADDR + i, &data);
			if(i == 0xc)
			data = otu_addr;
			flash_write_word(flash, FLASH_RESERVED_DATA_BASE + i,data);
		}
		//erase system data
		flash_erase_sector(flash, FLASH_SYSTEM_DATA_ADDR);
		//write data back to system data
		for(i = 0; i < 0x1000; i+= 4) {
			flash_read_word(flash, FLASH_RESERVED_DATA_BASE + i, &data);
			flash_write_word(flash, FLASH_SYSTEM_DATA_ADDR + i,data);
		}
		//erase backup sector
		flash_erase_sector(flash, FLASH_RESERVED_DATA_BASE);
	}
	return 0;
}

void fATSB(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	u32 boot_gpio, rb_boot_gpio;
	u8 gpio_pin;
	u8 uart_port, uart_index;
	u8 gpio_pin_bar;
	u8 uart_port_bar;
	flash_t flash;

	// parameter check
	AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: _AT_SYSTEM_BOOT_OTU_PIN_SET_");
	if(!arg) {
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: Usage: ATSB=[GPIO_PIN, TRIGER_MODE, UART]");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: GPIO_PIN: PB_1, PC_4 ....");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: TRIGER_MODE: low_trigger, high_trigger");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: UART: UART0, UART2");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: example: ATSB=[PC_2, low_trigger, UART2]");

	} else {
		argc = parse_param(arg, argv);
		if (argc != 4 ) {
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: Usage: ATSB=[GPIO_PIN, TRIGER_MODE, UART]");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: GPIO_PIN: PB_1, PC_4 ....");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: TRIGER_MODE: low_trigger, high_trigger");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: UART: UART0, UART2");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: example: ATSB=[PC_2, low_trigger, UART2]");
			return;
		}
	}

	if ( strncmp(argv[1], "P", 1) == 0 && strlen(argv[1]) == 4
			&& (strcmp(argv[2], "low_trigger") == 0 || strcmp(argv[2], "high_trigger") == 0)
			&& strncmp(argv[3], "UART", 4) == 0 && strlen(argv[3]) == 5) {
		if((0x41 <= argv[1][1] <= 0x45) && (0x30 <= argv[1][3] <= 0x39) &&(0x30 <= argv[1][4] <= 0x32)) {
			if(strcmp(argv[2], "high_trigger") == 0)
			gpio_pin = 1<< 7 | ((argv[1][1]-0x41)<<4) | (argv[1][3] - 0x30);
			else
			gpio_pin = ((argv[1][1]-0x41)<<4) | (argv[1][3] - 0x30);
			gpio_pin_bar = ~gpio_pin;
			uart_index = argv[3][4] - 0x30;
			if(uart_index == 0)
			uart_port = (uart_index<<4)|2;
			else if(uart_index == 2)
			uart_port = (uart_index<<4)|0;
			else {
				AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: Input UART index error. Please choose UART0 or UART2.");
				AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: example: ATSB=[PC_2, low_trigger, UART2]");
				return;
			}
			uart_port_bar = ~uart_port;
			boot_gpio = uart_port_bar<<24 | uart_port<<16 | gpio_pin_bar<<8 | gpio_pin;
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]:gpio_pin 0x%x", gpio_pin);
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]:gpio_pin_bar 0x%x", gpio_pin_bar);
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]:uart_port 0x%x", uart_port);
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]:uart_port_bar 0x%x", uart_port_bar);
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]:boot_gpio 0x%x", boot_gpio);
			write_otu_to_system_data(&flash, boot_gpio);
			flash_read_word(&flash, FLASH_SYSTEM_DATA_ADDR+0x0c, &rb_boot_gpio);
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]:Read 0x900c 0x%x", rb_boot_gpio);
		} else {
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: Usage: ATSB=[GPIO_PIN, TRIGER_MODE, UART]");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: GPIO_PIN: PB_1, PC_4 ....");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: TRIGER_MODE: low_trigger, high_trigger");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: UART: UART0, UART2");
			AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: example: ATSB=[PC_2, low_trigger, UART2]");
		}
	} else {
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: Usage: ATSB=[GPIO_PIN, TRIGER_MODE, UART]");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: GPIO_PIN: PB_1, PC_4 ....");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: TRIGER_MODE: low_trigger, high_trigger");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: UART: UART0, UART2");
		AT_DBG_MSG(AT_FLAG_DUMP, AT_DBG_ALWAYS, "[ATSB]: example: ATSB=[PC_2, low_trigger, UART2]");
		return;
	}
}
#endif

#if (configGENERATE_RUN_TIME_STATS == 1)
void fATSS(void *arg)	// Show CPU stats
{
	AT_PRINTK("[ATSS]: _AT_SYSTEM_CPU_STATS_");
	char *cBuffer = pvPortMalloc(512);
	if(cBuffer != NULL) {
		vTaskGetRunTimeStats((char *)cBuffer);
		AT_PRINTK("%s", cBuffer);
	}
	vPortFree(cBuffer);
}
#endif

void fATSs(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	AT_PRINTK("[ATS@]: _AT_SYSTEM_DBG_SETTING_");
	if(!arg) {
		AT_PRINTK("[ATS@] Usage: ATS@=[LEVLE,FLAG]");
	} else {
		argc = parse_param(arg, argv);
		if(argc == 3) {
			char *ptmp;
			gDbgLevel = atoi(argv[1]);
			gDbgFlag = strtoul(argv[2], &ptmp, 16);
		}
	}
	AT_PRINTK("[ATS@] level = %d, flag = 0x%08X", gDbgLevel, gDbgFlag);
}

void fATSc(void *arg)
{
	int argc = 0, config = 0;
	char *argv[MAX_ARGC] = {0};

	AT_PRINTK("[ATS!]: _AT_SYSTEM_CONFIG_SETTING_");
	if(!arg) {
		AT_PRINTK("[ATS!] Usage: ATS!=[CONFIG(0,1,2),FLAG]");
	} else {
		argc = parse_param(arg, argv);
		if(argc == 3) {
			char *ptmp;
			config = atoi(argv[1]);
			if(config == 0)
			ConfigDebugErr = strtoul(argv[2], &ptmp, 16);
			if(config == 1)
			ConfigDebugInfo = strtoul(argv[2], &ptmp, 16);
			if(config == 2)
			ConfigDebugWarn = strtoul(argv[2], &ptmp, 16);
		}
	}
	AT_PRINTK("[ATS!] ConfigDebugErr  = 0x%08X", ConfigDebugErr);
	AT_PRINTK("[ATS!] ConfigDebugInfo = 0x%08X", ConfigDebugInfo);
	AT_PRINTK("[ATS!] ConfigDebugWarn = 0x%08X", ConfigDebugWarn);
}

#define SUPPORT_CP_TEST 0
#if SUPPORT_CP_TEST
extern void MFi_auth_test(void);
void fATSM(void *arg)
{
	AT_PRINTK("[ATSM]: _AT_SYSTEM_CP_");
	MFi_auth_test();
}
#endif

void fATSt(void *arg)
{
	AT_PRINTK("[ATS#]: _AT_SYSTEM_TEST_");
	DBG_8195A("\nCLK CPU\t\t%d Hz\nRAM heap\t%d bytes\nTCM heap\t%d bytes\n",
			HalGetCpuClk(), xPortGetFreeHeapSize(), tcm_heap_freeSpace());
	dump_mem_block_list();
	tcm_heap_dump();
	DBG_8195A("\n");
}

void fATSJ(void *arg)
{
	int argc = 0, config = 0;
	char *argv[MAX_ARGC] = {0};
	AT_PRINTK("[ATSJ]: _AT_SYSTEM_JTAG_");
	if(!arg) {
		AT_PRINTK("[ATS!] Usage: ATSJ=off");
	} else {
		argc = parse_param(arg, argv);
		if (strcmp(argv[1], "off" ) == 0)
		sys_jtag_off();
		else
		AT_PRINTK("ATSL=%s is not supported!", argv[1]);
	}
}

void fATSx(void *arg)
{
//	uint32_t ability = 0;
	char buf[64];

	AT_PRINTK("[ATS?]: _AT_SYSTEM_HELP_");
	AT_PRINTK("[ATS?]: COMPILE TIME: %s", RTL8195AFW_COMPILE_TIME);
//	wifi_get_drv_ability(&ability);
	strcpy(buf, "v");
//	if(ability & 0x1)
//		strcat(buf, "m");
	strcat(buf, ".3.5." RTL8195AFW_COMPILE_DATE);
	AT_PRINTK("[ATS?]: SW VERSION: %s", buf);
}
#elif ATCMD_VER == ATVER_2

#define ATCMD_VERSION		"v2" //ATCMD MAJOR VERSION, AT FORMAT CHANGED
#define ATCMD_SUBVERSION	"2" //ATCMD MINOR VERSION, NEW COMMAND ADDED
#define ATCMD_REVISION		"2" //ATCMD FIX BUG REVISION
#define SDK_VERSION		"v3.5" //SDK VERSION
extern void sys_reset(void);
void print_system_at(void *arg);
extern void print_wifi_at(void *arg);
extern void print_tcpip_at(void *arg);

// uart version 2 echo info
extern unsigned char gAT_Echo;

void fATS0(void *arg) {
	at_printf("\r\n[AT] OK");
}

void fATSh(void *arg) {
	// print common AT command
	at_printf("\r\n[ATS?] ");
	at_printf("\r\nCommon AT Command:");
	print_system_at(arg);
#if CONFIG_WLAN
	at_printf("\r\nWi-Fi AT Command:");
	print_wifi_at(arg);
#endif

#if CONFIG_TRANSPORT
	at_printf("\r\nTCP/IP AT Command:");
	print_tcpip_at(arg);
#endif

	at_printf("\r\n[ATS?] OK");
}

void fATSR(void *arg) {
	at_printf("\r\n[ATSR] OK");
	sys_reset();
}

void fATSV(void *arg) {
	char at_buf[32];
	char fw_buf[32];
	char cspimode[4] = { 'S', 'D', 'Q', '?' };

	if (fspic_isinit == 0) {
		flash_turnon();
		flash_init(&flashobj);
		SpicDisableRtl8195A();
	}
	printf(
			"DeviceID: %02X, Flash Size: %d bytes, FlashID: %02X%02X%02X/%d,  SpicMode: %cIO\n",
			HalGetChipId(), (u32) (1 << flashobj.SpicInitPara.id[2]),
			flashobj.SpicInitPara.id[0], flashobj.SpicInitPara.id[1],
			flashobj.SpicInitPara.id[2], flashobj.SpicInitPara.flashtype,
			cspimode[flashobj.SpicInitPara.Mode.BitMode]);
	// get at version
	strcpy(at_buf, ATCMD_VERSION"."ATCMD_SUBVERSION"."ATCMD_REVISION);

	// get fw version
	strcpy(fw_buf, SDK_VERSION);
	printf("%s,%s(%s)\n", at_buf, fw_buf, RTL8195AFW_COMPILE_TIME);
	at_printf("\r\n[ATSV] OK:%s,%s(%s)", at_buf, fw_buf,
			RTL8195AFW_COMPILE_TIME);
}

#if defined(configUSE_WAKELOCK_PMU) && (configUSE_WAKELOCK_PMU == 1)

void fATSP(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };

	uint32_t lock_id;
	uint32_t bitmap;

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR,
				"\r\n[ATSP] Usage: ATSP=<a/r/?>");
		at_printf("\r\n[ATSP] ERROR:1");
		return;
	} else {
		if ((argc = parse_param(arg, argv)) != 2) {
			AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR,
					"\r\n[ATSP] Usage: ATSP=<a/r/?>");
			at_printf("\r\n[ATSP] ERROR:1");
			return;
		}
	}

	switch (argv[1][0]) {
	case 'a': // acquire
	{
		pmu_acquire_wakelock(WAKELOCK_OS);
		//at_printf("\r\n[ATSP] wakelock:0x%08x", get_wakelock_status());
		break;
	}

	case 'r': // release
	{
		pmu_release_wakelock(WAKELOCK_OS);
		//at_printf("\r\n[ATSP] wakelock:0x%08x", get_wakelock_status());
		break;
	}

	case '?': // get status
		break;
	default:
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR,
				"\r\n[ATSP] Usage: ATSP=<a/r/?>");
		at_printf("\r\n[ATSP] ERROR:2");
		return;
	}
	bitmap = pmu_get_wakelock_status();
	at_printf("\r\n[ATSP] OK:%s", (bitmap&WAKELOCK_OS)?"1":"0");
}
#endif

void fATSE(void *arg) {
	int argc = 0;
	int echo = 0, mask = gDbgFlag, dbg_lv = gDbgLevel;
	char *argv[MAX_ARGC] = { 0 };
	int err_no = 0;

	AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ALWAYS,
			"[ATSE]: _AT_SYSTEM_ECHO_DBG_SETTING");
	if (!arg) {
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR,
				"[ATSE] Usage: ATSE=<echo>,<dbg_msk>,<dbg_lv>");
		err_no = 1;
		goto exit;
	}

	argc = parse_param(arg, argv);

	if (argc < 2 || argc > 4) {
		err_no = 2;
		goto exit;
	}

#if CONFIG_EXAMPLE_UART_ATCMD
	if (argv[1] != NULL) {
		echo = atoi(argv[1]);
		if (echo > 1 || echo < 0) {
			err_no = 3;
			goto exit;
		}
		gAT_Echo = echo ? 1 : 0;
	}
#endif

	if ((argc > 2) && (argv[2] != NULL)) {
		mask = strtoul(argv[2], NULL, 0);
		at_set_debug_mask(mask);
	}

	if ((argc == 4) && (argv[3] != NULL)) {
		dbg_lv = strtoul(argv[3], NULL, 0);
		at_set_debug_level(dbg_lv);
	}

	exit: if (err_no)
		at_printf("\r\n[ATSE] ERROR:%d", err_no);
	else
		at_printf("\r\n[ATSE] OK");
	return;
}
#if CONFIG_WLAN
#if CONFIG_WEBSERVER
#include "wifi_structures.h"
#include "wifi_constants.h"
extern rtw_wifi_setting_t wifi_setting;
void fATSW(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR, "\r\n[ATSW] Usage: ATSW=<c/s>");
		at_printf("\r\n[ATSW] ERROR:1");
		return;
	} else {
		if((argc = parse_param(arg, argv)) != 2) {
			AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR, "\r\n[ATSW] Usage: ATSW=<c/s>");
			at_printf("\r\n[ATSW] ERROR:1");
			return;
		}
	}

	if(argv[1][0]!='c'&&argv[1][0]!='s') {
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR, "\r\n[ATSW] Usage: ATSW=<c/s>");
		at_printf("\r\n[ATSW] ERROR:2");
		return;
	}

	// make sure AP mode
	LoadWifiConfig();
	if(wifi_setting.mode != RTW_MODE_AP) {
		at_printf("\r\n[ATSW] ERROR:3");
		return;
	}

	switch(argv[1][0]) {
		case 'c': // create webserver
		{
			start_web_server();
			break;
		}
		case 's': // stop webserver
		{
			stop_web_server();
			break;
		}
	}
	at_printf("\r\n[ATSW] OK");
}
#endif

extern int EraseApinfo();
//extern int Erase_Fastconnect_data();

void fATSY(void *arg) {
#if CONFIG_EXAMPLE_WLAN_FAST_CONNECT
//	Erase_Fastconnect_data();
#endif

#if CONFIG_WEBSERVER
	EraseApinfo();
#endif

#if CONFIG_EXAMPLE_UART_ATCMD
	extern int reset_uart_atcmd_setting(void);
	reset_uart_atcmd_setting();
#endif

#if CONFIG_OTA_UPDATE
	// Reset ota image  signature
	cmd_ota_image(0);
#endif	

	at_printf("\r\n[ATSY] OK");
	// reboot
	sys_reset();
}

#if CONFIG_OTA_UPDATE
extern int wifi_is_connected_to_ap(void);
void fATSO(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_OTA, AT_DBG_ERROR,
				"\r\n[ATSO] Usage: ATSO=<ip>,<port>");
		at_printf("\r\n[ATSO] ERROR:1");
		return;
	}
	argv[0] = "update";
	if ((argc = parse_param(arg, argv)) != 3) {
		AT_DBG_MSG(AT_FLAG_OTA, AT_DBG_ERROR,
				"\r\n[ATSO] Usage: ATSO=<ip>,<port>");
		at_printf("\r\n[ATSO] ERROR:1");
		return;
	}

	// check wifi connect first
	if (wifi_is_connected_to_ap() == 0) {
		cmd_update(argc, argv);
		at_printf("\r\n[ATSO] OK");

	} else {
		at_printf("\r\n[ATSO] ERROR:3");
	}
}

void fATSC(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };
	int cmd = 0;

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_OTA, AT_DBG_ERROR, "\r\n[ATSC] Usage: ATSC=<0/1>");
		at_printf("\r\n[ATSC] ERROR:1");
		return;
	}
	if ((argc = parse_param(arg, argv)) != 2) {
		AT_DBG_MSG(AT_FLAG_OTA, AT_DBG_ERROR, "\r\n[ATSC] Usage: ATSC=<0/1>");
		at_printf("\r\n[ATSC] ERROR:1");
		return;
	}

	cmd = atoi(argv[1]);

	if ((cmd != 0) && (cmd != 1)) {
		at_printf("\r\n[ATSC] ERROR:2");
		return;
	}

	at_printf("\r\n[ATSC] OK");

	if (cmd == 1) {
		cmd_ota_image(1);
	} else {
		cmd_ota_image(0);
	}
	// reboot
	sys_reset();
}
#endif

#if CONFIG_EXAMPLE_UART_ATCMD
extern const u32 log_uart_support_rate[];

void fATSU(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };
	u32 baud = 0;
	u8 databits = 0;
	u8 stopbits = 0;
	u8 parity = 0;
	u8 flowcontrol = 0;
	u8 configmode = 0;
	int i;
	UART_LOG_CONF uartconf;

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR,
				"[ATSU] Usage: ATSU=<baud>,<databits>,<stopbits>,<parity>,<flowcontrol>,<configmode>");
		at_printf("\r\n[ATSU] ERROR:1");
		return;
	}
	if ((argc = parse_param(arg, argv)) != 7) {
		if(argv[1][0] == '?') {
			read_uart_atcmd_setting_from_system_data(&uartconf);
			at_printf("\r\n");
			at_printf( "AT_UART_CONF: %d,%d,%d,%d,%d",
					uartconf.BaudRate, uartconf.DataBits, uartconf.StopBits,
					uartconf.Parity, uartconf.FlowControl);
//			return;
		}
		else {
			AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ERROR,
					"[ATSU] Usage: ATSU=<baud>,<databits>,<stopbits>,<parity>,<flowcontrol>,<configmode>");
			at_printf("\r\n[ATSU] ERROR:1");
			return;
		}
	}
	else {
		baud = atoi(argv[1]);
		databits = atoi(argv[2]);
		stopbits = atoi(argv[3]);
		parity = atoi(argv[4]);
		flowcontrol = atoi(argv[5]);
		configmode = atoi(argv[6]);
		/*
		 // Check Baud rate
		 for (i=0; log_uart_support_rate[i]!=0xFFFFFF; i++) {
		 if (log_uart_support_rate[i] == baud) {
		 break;
		 }
		 }

		 if (log_uart_support_rate[i]== 0xFFFFFF) {
		 at_printf("\r\n[ATSU] ERROR:2");
		 return;
		 }
		 */
		if (((databits < 5) || (databits > 8)) || ((stopbits < 1) || (stopbits > 2))
				|| ((parity < 0) || (parity > 2))
				|| ((flowcontrol < 0) || (flowcontrol > 1))
				|| ((configmode < 0) || (configmode > 3)) ) {
			at_printf("\r\n[ATSU] ERROR:2");
			return;
		}
		memset((void*) &uartconf, 0, sizeof(UART_LOG_CONF));
		uartconf.BaudRate = baud;
		uartconf.DataBits = databits;
		uartconf.StopBits = stopbits;
		uartconf.Parity = parity;
		uartconf.FlowControl = flowcontrol;
		AT_DBG_MSG(AT_FLAG_COMMON, AT_DBG_ALWAYS, "AT_UART_CONF: %d,%d,%d,%d,%d",
				uartconf.BaudRate, uartconf.DataBits, uartconf.StopBits,
				uartconf.Parity, uartconf.FlowControl);
		switch (configmode) {
		case 0: // set current configuration, won't save
			uart_atcmd_reinit(&uartconf);
			break;
		case 1: // set current configuration, and save
			write_uart_atcmd_setting_to_system_data(&uartconf);
			uart_atcmd_reinit(&uartconf);
			break;
		case 2: // set configuration, reboot to take effect
			write_uart_atcmd_setting_to_system_data(&uartconf);
			break;
		}
	}
	at_printf("\r\n[ATSU] OK");
}
#endif //#if CONFIG_EXAMPLE_UART_ATCMD
#endif //#if CONFIG_WLAN

void fATSG(void *arg) {
	gpio_t gpio_ctrl;
	int argc = 0, val, error_no = 0;
	char *argv[MAX_ARGC] = { 0 }, port, num;
	PinName pin = NC;

	AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "[ATSG]: _AT_SYSTEM_GPIO_CTRL_");

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ERROR,
				"[ATSG] Usage: ATSG=<R/W>,<PORT>,<DATA>,<DIR>,<PULL>");
		error_no = 1;
		goto exit;
	}
	if ((argc = parse_param(arg, argv)) < 3) {
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ERROR,
				"[ATSG] Usage: ATSG=<R/W>,<PORT>,<DATA>,<DIR>,<PULL>");
		error_no = 2;
		goto exit;
	}

	port = argv[2][1];
	num = strtoul(&argv[2][3], NULL, 0);
	port -= 'A';
	pin = (port << 4 | num);
	AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "PORT: %s[%d]", argv[2], pin);

	if (gpio_set(pin) == 0xff) {
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ERROR, "[ATSG]: Invalid Pin Name [%d]",
				pin);
		error_no = 3;
		goto exit;
	}

	gpio_init(&gpio_ctrl, pin);
	if (argv[4]) {
		int dir = atoi(argv[4]);
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "DIR: %s", argv[4]);
		gpio_dir(&gpio_ctrl, dir);
	}
	if (argv[5]) {
		int pull = atoi(argv[5]);
		AT_DBG_MSG(AT_FLAG_GPIO, AT_DBG_ALWAYS, "PULL: %s", argv[5]);
		gpio_mode(&gpio_ctrl, pull);
	}
	if (argv[1][0] == 'R') {
		val = gpio_read(&gpio_ctrl);
	} else {
		val = atoi(argv[3]);
		gpio_write(&gpio_ctrl, val);
	}

	exit: if (error_no) {
		at_printf("\r\n[ATSG] ERROR:%d", error_no);
	} else {
		at_printf("\r\n[ATSG] OK:%d", val);
	}
}

#endif //#elif ATCMD_VER == ATVER_2
#if defined(configUSE_WAKELOCK_PMU) && (configUSE_WAKELOCK_PMU == 1)
/*
 * bitmask:
 * bit0: OS
 * bit1: WLAN
 * bit2: LOGUART
 * bit3: SDIO
 */
void fATSL(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };
	int err_no = 0;
	uint32_t lock_id;

	AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS, "[ATSL]: _AT_SYS_WAKELOCK_TEST_");

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS,
				"[ATSL] Usage ATSL=[a/r/?][bitmask]");
		err_no = 1;
		goto exit;
	} else {
		argc = parse_param(arg, argv);
		if (argc < 2) {
			AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS,
					"[ATSL] Usage ATSL=[a/r/?][bitmask]");
			err_no = 2;
			goto exit;
		}
	}

	switch (argv[1][0]) {
	case 'a': // acquire
	{
		if (argc == 3) {
			lock_id = strtoul(argv[2], NULL, 16);
			pmu_acquire_wakelock(lock_id);
		}
		AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS, "[ATSL] wakelock:0x%08x",
				pmu_get_wakelock_status());
		break;
	}

	case 'r': // release
	{
		if (argc == 3) {
			lock_id = strtoul(argv[2], NULL, 16);
			pmu_release_wakelock(lock_id);
		}
		AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS, "[ATSL] wakelock:0x%08x",
				pmu_get_wakelock_status());
		break;
	}

	case '?': // get status
		AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS, "[ATSL] wakelock:0x%08x",
				pmu_get_wakelock_status());
#if (configGENERATE_RUN_TIME_STATS == 1)
		char *cBuffer = pvPortMalloc(512);
		if (cBuffer != NULL) {
			pmu_get_wakelock_hold_stats((char *) cBuffer);
			AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS, "%s", cBuffer);
		}
		vPortFree(cBuffer);
#endif
		break;

#if (configGENERATE_RUN_TIME_STATS == 1)
	case 'c': // clean wakelock info (for recalculate wakelock hold time)
		AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS, "[ATSL] clean wakelock stat");
		pmu_clean_wakelock_stat();
		break;
#endif
	default:
		AT_DBG_MSG(AT_FLAG_OS, AT_DBG_ALWAYS,
				"[ATSL] Usage ATSL=[a/r/?][bitmask]");
		err_no = 3;
		break;
	}
	exit:
#if ATCMD_VER == ATVER_2
	if (err_no)
		at_printf("\r\n[ATSL] ERROR:%d", err_no);
	else
		at_printf("\r\n[ATSL] OK:0x%08x", pmu_get_wakelock_status());
#endif
	return;
}

#if CONFIG_UART_XMODEM
void fATSX(void *arg)
{
	if (HalGetChipId() < CHIP_ID_8195AM) {

		// use xmodem to update, RX: PC_0, TX: PC_3, baudrate: 1M
		OTU_FW_Update(0, 0, 115200);
		// use xmodem to update, RX: PE_3, TX: PE_0, baudrate: 1M
		//  JTAG Off!
		//	OTU_FW_Update(0, 1, 115200);
	}
	else {	
		// use xmodem to update, RX: PA_6, TX: PA_7, baudrate: 1M
		OTU_FW_Update(0, 2, 115200);
	};
	at_printf("\r\n[ATSX] OK");
}
#endif

#endif

void print_hex_dump(uint8_t *buf, int len, unsigned char k) {
	uint32_t ss[2];
	ss[0] = 0x78323025; // "%02x"
	ss[1] = k;	// ","...'\0'
	uint8_t * ptr = buf;
	while (len--) {
		if (len == 0)
			ss[1] = 0;
		printf((uint8_t *) &ss[0], *ptr++);
	}
}

//ATFD - Flash Data Dump
void fATFD(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };
#if	DEBUG_AT_USER_LEVEL > 3
	printf("ATFD: _AT_FLASH_DUMP_\n");
#endif
	if (!arg) {
		printf("Usage: ATFD=faddr(HEX),[size]\n");
	} else {
		argc = parse_param(arg, argv);
		if (argc >= 1) {
			int addr;
			sscanf(argv[1], "%x", &addr);
			int size = 0;
			if (argc > 2)
				size = atoi(argv[2]);
			if (size <= 0 || size > 16384)
				size = 1;
			u32 symbs_line = 32;
			u32 rdsize = 8 * symbs_line;
			uint8_t *flash_data = (uint8_t *) malloc(rdsize);
			while (size) {
				if (size < rdsize)
					rdsize = size;
				else
					rdsize = 8 * symbs_line;
				flash_stream_read(&flashobj, addr, rdsize, flash_data);
				uint8_t *ptr = flash_data;
				while (ptr < flash_data + rdsize) {
					if (symbs_line > size)
						symbs_line = size;
					printf("%08X ", addr);
					print_hex_dump(ptr, symbs_line, ' ');
					printf("\r\n");
					addr += symbs_line;
					ptr += symbs_line;
					size -= symbs_line;
					if (size == 0)
						break;
				}
			}
			free(flash_data);
		}
	}
}

void fATFO(void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = { 0 };
#if	DEBUG_AT_USER_LEVEL > 3
	printf("ATFO: _AT_FLASH_OTP_DUMP_\n");
#endif
	if (!arg) {
		printf("Usage: ATFO=faddr(HEX),[size]\n");
	} else {
		argc = parse_param(arg, argv);
		if (argc >= 1) {
			int addr;
			sscanf(argv[1], "%x", &addr);
			int size = 0;
			if (argc > 2)
				size = atoi(argv[2]);
			if (size <= 0 || size > 16384)
				size = 1;
			u32 symbs_line = 32;
			u32 rdsize = 8 * symbs_line;
			uint8_t *flash_data = (uint8_t *) malloc(rdsize);
			while (size) {
				if (size < rdsize)
					rdsize = size;
				else
					rdsize = 8 * symbs_line;
				flash_otp_read(&flashobj, addr, rdsize, flash_data);
				uint8_t *ptr = flash_data;
				while (ptr < flash_data + rdsize) {
					if (symbs_line > size)
						symbs_line = size;
					printf("%08X ", addr);
					print_hex_dump(ptr, symbs_line, ' ');
					printf("\r\n");
					addr += symbs_line;
					ptr += symbs_line;
					size -= symbs_line;
					if (size == 0)
						break;
				}
			}
			free(flash_data);
		}
	}
}

// Mem info
void fATST(void *arg) {
	extern void dump_mem_block_list(void); // heap_5.c
	printf("\nCLK CPU\t\t%d Hz\nRAM heap\t%d bytes\nTCM heap\t%d bytes\n",
			HalGetCpuClk(), xPortGetFreeHeapSize(), tcm_heap_freeSpace());
#if CONFIG_DEBUG_LOG > 1
	dump_mem_block_list();
	tcm_heap_dump();
#endif;
	printf("\n");
#if (configGENERATE_RUN_TIME_STATS == 1)
	char *cBuffer = pvPortMalloc(512);
	if(cBuffer != NULL) {
		vTaskGetRunTimeStats((char *)cBuffer);
		printf("%s", cBuffer);
	}
	vPortFree(cBuffer);
#endif
}

#if 0
#if 1
#include "drv_types.h" // or #include "wlan_lib.h"
#else
#include "wifi_constants.h"
#include "wifi_structures.h"
#include "wlan_lib.h" // or #include "drv_types.h"
#endif
#include "hal_com_reg.h"
// extern Rltk_wlan_t rltk_wlan_info[2];
void fATXT(void *arg)
{
#if	DEBUG_AT_USER_LEVEL > 3
	printf("ATWT: _AT_CFG_DUMP_\n");
#endif
	int size = 512;
	int addr = 0;
	uint8_t *blk_data = (uint8_t *)malloc(size);
	memset(blk_data, 0xff, size);
	if(blk_data) {
		uint8_t * ptr = blk_data;
		Hal_ReadEFuse(*(_adapter **)(rltk_wlan_info->priv), 0, 0, 512, ptr, 1);
		//rtw_flash_map_update(*(_adapter **)(rltk_wlan_info->priv), 512);
		u32 symbs_line = 32;
		while(addr < size) {
			if(symbs_line > size) symbs_line = size;
			printf("%08X ", addr);
			print_hex_dump(ptr, symbs_line, ' ');
			printf("\r\n");
			addr += symbs_line;
			ptr += symbs_line;
			size -= symbs_line;
			if(size == 0) break;
		}
		free(blk_data);
	}
}
#endif
log_item_t at_sys_items[] = {
#if (ATCMD_VER == ATVER_1)
		{	"ATSD", fATSD},	// Dump register
		{	"ATSE", fATSE},	// Edit register
		{	"ATSC", fATSC},	// Clear OTA signature
		{	"ATSR", fATSR},	// Recover OTA signature
#if CONFIG_UART_XMODEM
		{	"ATSY", fATSY},	// uart ymodem upgrade
#endif
#if SUPPORT_MP_MODE
		{	"ATSA", fATSA},	// MP ADC test
		{	"ATSG", fATSG},	// MP GPIO test
		{	"ATSP", fATSP},	// MP Power related test
		{	"ATSB", fATSB},	// OTU PIN setup
#endif
#if (configGENERATE_RUN_TIME_STATS == 1)
		{	"ATSS", fATSS},	// Show CPU stats
#endif
#if SUPPORT_CP_TEST
		{	"ATSM", fATSM},	// Apple CP test
#endif
		{	"ATSJ", fATSJ}, //trun off JTAG
		{	"ATS@", fATSs},	// Debug message setting
		{	"ATS!", fATSc},	// Debug config setting
		{	"ATS#", fATSt},	// test command
		{	"ATS?", fATSx},	// Help
#elif ATCMD_VER == ATVER_2 //#if ATCMD_VER == ATVER_1
		{ "AT", fATS0 },	// test AT command ready
		{ "ATS?", fATSh },	// list all AT command
		{ "ATSR", fATSR },	// system restart
		{ "ATSV", fATSV },	// show version info
		{ "ATSP", fATSP },	// power saving mode
		{ "ATSE", fATSE },	// enable and disable echo
#if CONFIG_WLAN
#if CONFIG_WEBSERVER
		{ "ATSW", fATSW},	// start webserver
#endif
		{ "ATSY", fATSY },	// factory reset
#if CONFIG_OTA_UPDATE
		{ "ATSO", fATSO },	// ota upgrate
		{ "ATSC", fATSC },	// chose the activited image
#endif
#if CONFIG_EXAMPLE_UART_ATCMD
		{ "ATSU", fATSU },	// AT uart configuration
#endif
#endif
		{ "ATSG", fATSG },	// GPIO control
#if CONFIG_UART_XMODEM
		{ "ATSX", fATSX},	// uart xmodem upgrade
#endif
		{ "ATSD", fATSD },	// Dump register
		{ "ATXD", fATXD },	// Write register
#endif // end of #if ATCMD_VER == ATVER_1

// Following commands exist in two versions
#if defined(configUSE_WAKELOCK_PMU) && (configUSE_WAKELOCK_PMU == 1)
		{ "ATSL", fATSL },	 // wakelock test
#endif
		{ "ATFD", fATFD }, 	// Flash Data Damp
		{ "ATFO", fATFO }, 	// Flash OTP Damp
		{ "ATST", fATST },	// add pvvx: mem info
//		{ "ATXT", fATXT },	// add pvvx: cfg_wifi
		{ "ATSI", fATSI }	// Dev/Ports Info
};

#if ATCMD_VER == ATVER_2
void print_system_at(void *arg) {
	int index;
	int cmd_len = 0;

	cmd_len = sizeof(at_sys_items) / sizeof(at_sys_items[0]);
	for (index = 0; index < cmd_len; index++)
		at_printf("\r\n%s", at_sys_items[index].log_cmd);
}
#endif
void at_sys_init(void) {
	log_service_add_table(at_sys_items,
			sizeof(at_sys_items) / sizeof(at_sys_items[0]));
}

#if SUPPORT_LOG_SERVICE
log_module_init(at_sys_init);
#endif

#endif //#ifdef CONFIG_AT_SYS