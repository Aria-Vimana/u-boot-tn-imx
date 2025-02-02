/*
 * Copyright (C) 2018 TechNexion Ltd.
 *
 * Author: Richard Hu <richard.hu@technexion.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/delay.h>
#include <command.h>
#include <ipu_pixfmt.h>
#include <asm/arch/clock.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/arch/mxc_hdmi.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/video.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/io.h>
#include <linux/sizes.h>
#include <common.h>
#include <fsl_esdhc_imx.h>
#include <miiphy.h>
#include <netdev.h>
#include <usb.h>
#include <phy.h>
#include <power/pmic.h>
#include <power/pfuze100_pmic.h>
#ifdef CONFIG_CMD_SATA
#include <asm/mach-imx/sata.h>
#endif
#include <splash.h>
#include <dm/uclass-internal.h>

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PUS_47K_UP |			\
	PAD_CTL_SPEED_LOW | PAD_CTL_DSE_80ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS)

#define I2C_PAD_CTRL	(PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS |	\
	PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define USDHC1_CD_GPIO		IMX_GPIO_NR(1, 2)
#define USDHC3_CD_GPIO		IMX_GPIO_NR(3, 9)
#define ETH_PHY_RESET		IMX_GPIO_NR(3, 29)
#define ETH_PHY_POWER		IMX_GPIO_NR(7, 13)
#define VERSION_DET_R1310	IMX_GPIO_NR(5, 31)
#define VERSION_DET_R1311	IMX_GPIO_NR(6, 0)

static bool with_pmic = true;

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();

	return 0;
}

static iomux_v3_cfg_t const uart1_pads[] = {
	IOMUX_PADS(PAD_CSI0_DAT10__UART1_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_CSI0_DAT11__UART1_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
};

static iomux_v3_cfg_t const usdhc1_pads[] = {
	IOMUX_PADS(PAD_SD1_CLK__SD1_CLK    | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_CMD__SD1_CMD    | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DAT0__SD1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DAT1__SD1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DAT2__SD1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DAT3__SD1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	/* Carrier MicroSD Card Detect */
	IOMUX_PADS(PAD_GPIO_2__GPIO1_IO02  | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usdhc3_pads[] = {
	IOMUX_PADS(PAD_SD3_CLK__SD3_CLK    | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_CMD__SD3_CMD    | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT1__SD3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT2__SD3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DAT3__SD3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	/* SOM MicroSD Card Detect */
	IOMUX_PADS(PAD_EIM_DA9__GPIO3_IO09  | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads[] = {
	IOMUX_PADS(PAD_ENET_MDIO__ENET_MDIO  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_MDC__ENET_MDC    | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TXC__RGMII_TXC  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD0__RGMII_TD0  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD1__RGMII_TD1  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD2__RGMII_TD2  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD3__RGMII_TD3  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TX_CTL__RGMII_TX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_REF_CLK__ENET_TX_CLK  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RXC__RGMII_RXC  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD0__RGMII_RD0  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD1__RGMII_RD1  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD2__RGMII_RD2  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD3__RGMII_RD3  | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RX_CTL__RGMII_RX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	/* AR8031/AR8035 PHY Reset */
	IOMUX_PADS(PAD_EIM_D29__GPIO3_IO29    | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_ar8035_power_pads[] = {
	/* AR8035 POWER */
	IOMUX_PADS(PAD_GPIO_18__GPIO7_IO13    | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const version_detection_pads[] = {
	/* R1310 */
	IOMUX_PADS(PAD_CSI0_DAT13__GPIO5_IO31  | MUX_PAD_CTRL(NO_PAD_CTRL)),
	/* R1311 */
	IOMUX_PADS(PAD_CSI0_DAT14__GPIO6_IO00  | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static void setup_iomux_uart(void)
{
	SETUP_IOMUX_PADS(uart1_pads);
}

static void setup_iomux_enet(void)
{
	SETUP_IOMUX_PADS(enet_pads);

	if (with_pmic) {
		SETUP_IOMUX_PADS(enet_ar8035_power_pads);
		/* enable AR8035 POWER */
		gpio_request(ETH_PHY_POWER, "enet_phy_power");
		gpio_direction_output(ETH_PHY_POWER, 0);
	}
	/* wait until 3.3V of PHY and clock become stable */
	mdelay(10);
	/* Reset AR8031/AR8035 PHY */
	gpio_request(ETH_PHY_RESET, "enet_phy_reset");
	gpio_direction_output(ETH_PHY_RESET, 0);
	mdelay(10);
	gpio_set_value(ETH_PHY_RESET, 1);
	udelay(100);
}

static void setup_iomux_version_detection(void)
{
	SETUP_IOMUX_PADS(version_detection_pads);
}

static struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{USDHC3_BASE_ADDR},
	{USDHC1_BASE_ADDR},
};

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC1_BASE_ADDR:
		ret = !gpio_get_value(USDHC1_CD_GPIO);
		break;
	case USDHC3_BASE_ADDR:
		ret = !gpio_get_value(USDHC3_CD_GPIO);
		break;
	}

	return ret;
}

int board_mmc_init(struct bd_info *bis)
{
	int ret;
	u32 index = 0;

	/*
	 * Following map is done:
	 * (USDHC)	(Physical Port)
	 * usdhc3	SOM MicroSD/MMC
	 * usdhc1	Carrier board MicroSD
	 * Always set boot USDHC as mmc0
	 */

	SETUP_IOMUX_PADS(usdhc3_pads);
	gpio_direction_input(USDHC3_CD_GPIO);

	SETUP_IOMUX_PADS(usdhc1_pads);
	gpio_direction_input(USDHC1_CD_GPIO);

	switch (get_boot_device()) {
	case SD1_BOOT:
		usdhc_cfg[0].esdhc_base = USDHC1_BASE_ADDR;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		usdhc_cfg[0].max_bus_width = 4;
		usdhc_cfg[1].esdhc_base = USDHC3_BASE_ADDR;
		usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		usdhc_cfg[1].max_bus_width = 1;
		break;
	case SD3_BOOT:
	default:
		usdhc_cfg[0].esdhc_base = USDHC3_BASE_ADDR;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		usdhc_cfg[0].max_bus_width = 1;
		usdhc_cfg[1].esdhc_base = USDHC1_BASE_ADDR;
		usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		usdhc_cfg[1].max_bus_width = 4;
		break;
	}

	for (index = 0; index < CONFIG_SYS_FSL_USDHC_NUM; ++index) {
		ret = fsl_esdhc_initialize(bis, &usdhc_cfg[index]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	unsigned short val;

	/* To enable AR8031/AR8035 output a 125MHz clk from CLK_25M */
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x7);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, 0x8016);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x4007);

	val = phy_read(phydev, MDIO_DEVAD_NONE, 0xe);
	if (with_pmic) {
		/* AR8035 */
		val &= 0xffe7;
		val |= 0x18;
	} else {
		/* AR8031 */
		val &= 0xffe3;
		val |= 0x18;
	}
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, val);

	/* introduce tx clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x5);
	val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1e);
	val |= 0x0100;
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, val);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

struct i2c_pads_info mx6q_i2c1_pad_info = {
	.scl = {
		.i2c_mode = MX6Q_PAD_EIM_D21__I2C1_SCL
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6Q_PAD_EIM_D21__GPIO3_IO21
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(3, 21)
	},
	.sda = {
		.i2c_mode = MX6Q_PAD_EIM_D28__I2C1_SDA
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6Q_PAD_EIM_D28__GPIO3_IO28
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(3, 28)
	}
};

struct i2c_pads_info mx6dl_i2c1_pad_info = {
	.scl = {
		.i2c_mode = MX6DL_PAD_EIM_D21__I2C1_SCL
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6DL_PAD_EIM_D21__GPIO3_IO21
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(3, 21)
	},
	.sda = {
		.i2c_mode = MX6DL_PAD_EIM_D28__I2C1_SDA
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6DL_PAD_EIM_D28__GPIO3_IO28
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(3, 28)
	}
};

struct i2c_pads_info mx6q_i2c3_pad_info = {
	.scl = {
		.i2c_mode = MX6Q_PAD_GPIO_5__I2C3_SCL
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6Q_PAD_GPIO_5__GPIO1_IO05
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(1, 5)
	},
	.sda = {
		.i2c_mode = MX6Q_PAD_GPIO_16__I2C3_SDA
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6Q_PAD_GPIO_16__GPIO7_IO11
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(7, 11)
	}
};

struct i2c_pads_info mx6dl_i2c3_pad_info = {
	.scl = {
		.i2c_mode = MX6DL_PAD_GPIO_5__I2C3_SCL
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6DL_PAD_GPIO_5__GPIO1_IO05
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(1, 5)
	},
	.sda = {
		.i2c_mode = MX6DL_PAD_GPIO_16__I2C3_SDA
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6DL_PAD_GPIO_16__GPIO7_IO11
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(7, 11)
	}
};

#if defined(CONFIG_DM_VIDEO)
struct i2c_pads_info mx6q_i2c2_pad_info = {
	.scl = {
		.i2c_mode = MX6Q_PAD_KEY_COL3__I2C2_SCL
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6Q_PAD_KEY_COL3__GPIO4_IO12
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(4, 12)
	},
	.sda = {
		.i2c_mode = MX6Q_PAD_KEY_ROW3__I2C2_SDA
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6Q_PAD_KEY_ROW3__GPIO4_IO13
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(4, 13)
	}
};

struct i2c_pads_info mx6dl_i2c2_pad_info = {
	.scl = {
		.i2c_mode = MX6DL_PAD_KEY_COL3__I2C2_SCL
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6DL_PAD_KEY_COL3__GPIO4_IO12
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(4, 12)
	},
	.sda = {
		.i2c_mode = MX6DL_PAD_KEY_ROW3__I2C2_SDA
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gpio_mode = MX6DL_PAD_KEY_ROW3__GPIO4_IO13
			| MUX_PAD_CTRL(I2C_PAD_CTRL),
		.gp = IMX_GPIO_NR(4, 13)
	}
};

static iomux_v3_cfg_t const fwadapt_7wvga_pads[] = {
	IOMUX_PADS(PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK),
	IOMUX_PADS(PAD_DI0_PIN2__IPU1_DI0_PIN02), /* HSync */
	IOMUX_PADS(PAD_DI0_PIN3__IPU1_DI0_PIN03), /* VSync */
	IOMUX_PADS(PAD_DI0_PIN4__IPU1_DI0_PIN04	| MUX_PAD_CTRL(PAD_CTL_DSE_120ohm)), /* Contrast */
	IOMUX_PADS(PAD_DI0_PIN15__IPU1_DI0_PIN15), /* DISP0_DRDY */
	IOMUX_PADS(PAD_DISP0_DAT0__IPU1_DISP0_DATA00),
	IOMUX_PADS(PAD_DISP0_DAT1__IPU1_DISP0_DATA01),
	IOMUX_PADS(PAD_DISP0_DAT2__IPU1_DISP0_DATA02),
	IOMUX_PADS(PAD_DISP0_DAT3__IPU1_DISP0_DATA03),
	IOMUX_PADS(PAD_DISP0_DAT4__IPU1_DISP0_DATA04),
	IOMUX_PADS(PAD_DISP0_DAT5__IPU1_DISP0_DATA05),
	IOMUX_PADS(PAD_DISP0_DAT6__IPU1_DISP0_DATA06),
	IOMUX_PADS(PAD_DISP0_DAT7__IPU1_DISP0_DATA07),
	IOMUX_PADS(PAD_DISP0_DAT8__IPU1_DISP0_DATA08),
	IOMUX_PADS(PAD_DISP0_DAT9__IPU1_DISP0_DATA09),
	IOMUX_PADS(PAD_DISP0_DAT10__IPU1_DISP0_DATA10),
	IOMUX_PADS(PAD_DISP0_DAT11__IPU1_DISP0_DATA11),
	IOMUX_PADS(PAD_DISP0_DAT12__IPU1_DISP0_DATA12),
	IOMUX_PADS(PAD_DISP0_DAT13__IPU1_DISP0_DATA13),
	IOMUX_PADS(PAD_DISP0_DAT14__IPU1_DISP0_DATA14),
	IOMUX_PADS(PAD_DISP0_DAT15__IPU1_DISP0_DATA15),
	IOMUX_PADS(PAD_DISP0_DAT16__IPU1_DISP0_DATA16),
	IOMUX_PADS(PAD_DISP0_DAT17__IPU1_DISP0_DATA17),
	IOMUX_PADS(PAD_SD4_DAT2__GPIO2_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL)), /* DISP0_BKLEN */
	IOMUX_PADS(PAD_SD4_DAT3__GPIO2_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL)), /* DISP0_VDDEN */
};

static void setup_iomux_i2c(void)
{
	if (is_cpu_type(MXC_CPU_MX6Q) || is_cpu_type(MXC_CPU_MX6D) ||
		is_cpu_type(MXC_CPU_MX6QP) || is_cpu_type(MXC_CPU_MX6DP)) {
		//setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6q_i2c1_pad_info);
		setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6q_i2c2_pad_info);
		setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6q_i2c3_pad_info);
	} else {
		//setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c1_pad_info);
		setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c2_pad_info);
		setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c3_pad_info);
	}
}

static const char *sbc_type(void)
{
	gpio_request(VERSION_DET_R1310, "VER_DET_R1310");
	gpio_direction_input(VERSION_DET_R1310);
	gpio_request(VERSION_DET_R1311, "VER_DET_R1311");
	gpio_direction_input(VERSION_DET_R1311);
	udelay(500);

	/* Detection for TEK series
	       VERSION_DET_R1310 VERSION_DET_R1311
	   TEK3            0                 0
	   TEK5            1                 1
	*/
	if (gpio_get_value(VERSION_DET_R1310) && gpio_get_value(VERSION_DET_R1310))
		return "tep5";
	else
		return "tek3";

}

/* setup board specific PMIC */
int power_init_board(void)
{
	struct pmic *p;
	u32 reg;

	/* configure PFUZE100 PMIC */
	power_pfuze100_init(I2C_PMIC_BUS);
	p = pmic_get("PFUZE100");
	if (p && !pmic_probe(p)) {
		pmic_reg_read(p, PFUZE100_DEVICEID, &reg);
		printf("PMIC:  PFUZE100 ID=0x%02x\n", reg);

		/* Set VGEN2 to 1.5V and enable */
		pmic_reg_read(p, PFUZE100_VGEN2VOL, &reg);
		reg &= ~(LDO_VOL_MASK);
		reg |= (LDOA_1_50V | (1 << (LDO_EN)));
		pmic_reg_write(p, PFUZE100_VGEN2VOL, reg);
	} else
		with_pmic = false;

	return 0;
}

static void do_enable_hdmi(struct display_info_t const *dev)
{
	imx_enable_hdmi_phy();
}

static int detect_i2c(struct display_info_t const *dev)
{
	return (0 == i2c_set_bus_num(dev->bus)) &&
			(0 == i2c_probe(dev->addr));
}

struct touth_device {
	u16 idVendor;
	char prod[24];
};

static struct touth_device touch_matches[] = {
	{0x0eef, "eGalaxTouch EXC3146"},
	{0x0eef, "eGalaxTouch EXC3160-15"},
	{0x0eef, "eGalaxTouch P80H60 1563"},
	{0x0eef, "eGalaxTouch EXC3000-08"},
};

#ifndef CONFIG_DM_USB
static char usb_inited = -1;
#else
static int usb_inited_dm = -1;
#endif

static int detect_usb(struct display_info_t const *dev)
{
#ifndef CONFIG_DM_USB
	void *ctrl;
	struct usb_device *udev = NULL;
	int ret, i;

	if (usb_inited != dev->bus) {
		usb_hub_reset();
		ret = usb_lowlevel_init(dev->bus, USB_INIT_HOST, &ctrl);
		if ((ret == -ENODEV) || ret)	/* No such device or other error*/
			return 0;

		ret = usb_alloc_new_device(ctrl, &udev);
		if (ret)
			return 0;

		ret = usb_new_device(udev);
		if (ret)
			usb_free_device(udev->controller);

		usb_inited = dev->bus;
	}

	for (i = 0; i < USB_MAX_DEVICE; i++) {
		udev = usb_get_dev_index(i);
		if (udev == NULL)
			continue;

		if (udev->descriptor.idVendor != touch_matches[dev->addr].idVendor)
			continue;

		if (udev->descriptor.bDescriptorType == USB_DT_DEVICE)
			if (strncmp(udev->prod, touch_matches[dev->addr].prod, strlen(touch_matches[dev->addr].prod)) == 0)
				return 1;
	}
#else
	struct udevice *bus, *child;
	struct usb_bus_priv *priv;
	struct usb_device *udev;
	int device_num;

	char *s = strdup(sbc_type());
	if (!strcmp(s, "tep5")) {
		if (usb_inited_dm != 1) {
			usb_init();
			usb_inited_dm = 1;
		}

		uclass_find_first_device(UCLASS_USB, &bus);
		while (bus) {
			priv = dev_get_uclass_priv(bus);
			device_num = priv->next_addr;

			for (int i = 0 ; i < device_num; i++) {
				if (i == 0)
					device_find_first_child(bus, &child);
				else
					device_find_first_child(child, &child);
				if (child) {
					udev = dev_get_parent_priv(child);	//Get usb_device pointer
					if (udev->descriptor.idVendor != touch_matches[dev->addr].idVendor)
						continue;

					if (udev->descriptor.bDescriptorType == USB_DT_DEVICE)
						if (strncmp(udev->prod, touch_matches[dev->addr].prod, strlen(touch_matches[dev->addr].prod)) == 0)
							return 1;
				}
			}
			//Find next bus
			uclass_find_next_device(&bus);
		}
	}
#endif /* !define(CONFIG_DM_USB) */
	return 0;
}

static void enable_lvds(struct display_info_t const *dev)
{
	struct iomuxc *iomux = (struct iomuxc *)
				IOMUXC_BASE_ADDR;

	/* set CH0 data width to 24bit (IOMUXC_GPR2:5 0=18bit, 1=24bit) */
	u32 reg = readl(&iomux->gpr[2]);
	reg |= IOMUXC_GPR2_DATA_WIDTH_CH0_24BIT;
	writel(reg, &iomux->gpr[2]);

	/* Enable Backlight - use GPIO for Brightness adjustment */
	SETUP_IOMUX_PAD(PAD_SD4_DAT1__GPIO2_IO09 | MUX_PAD_CTRL(NO_PAD_CTRL));
	gpio_request(IMX_GPIO_NR(2, 9), "backlight_enable");
	gpio_direction_output(IMX_GPIO_NR(2, 9), 1);

	SETUP_IOMUX_PAD(PAD_SD4_DAT0__GPIO2_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL));
	gpio_request(IMX_GPIO_NR(2, 8), "brightness");
	gpio_direction_output(IMX_GPIO_NR(2, 8), 1);
	gpio_set_value(IMX_GPIO_NR(2, 8), 1);
	mdelay(100);
	gpio_set_value(IMX_GPIO_NR(2, 8), 0);
	mdelay(100);
	gpio_set_value(IMX_GPIO_NR(2, 8), 1);
}

static void enable_fwadapt_7wvga(struct display_info_t const *dev)
{
	SETUP_IOMUX_PADS(fwadapt_7wvga_pads);

	gpio_request(IMX_GPIO_NR(2, 10), "parallel_enable");
	gpio_request(IMX_GPIO_NR(2, 11), "parallel_brightness");
	gpio_direction_output(IMX_GPIO_NR(2, 10), 1);
	gpio_direction_output(IMX_GPIO_NR(2, 11), 1);
}

struct display_info_t const displays[] = {{
	.bus	= 1,
	.addr	= 0,
	.pixfmt = IPU_PIX_FMT_RGB24,
	.detect = NULL,
	.enable = enable_lvds,
	.mode	= {
		.name           = "10inch_v01",
		.refresh        = 60,
		.xres           = 1280,
		.yres           = 800,
		.pixclock       = 1000000000000ULL/71100000,
		.left_margin    = 40,
		.right_margin   = 40,
		.upper_margin   = 10,
		.lower_margin   = 3,
		.hsync_len      = 80,
		.vsync_len      = 10,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= 1,
	.addr	= 1,
	.pixfmt = IPU_PIX_FMT_RGB24,
	.detect = detect_usb,
	.enable = enable_lvds,
	.mode	= {
		.name           = "LVDS_SIN_1368x768",
		.refresh        = 60,
		.xres           = 1368,
		.yres           = 768,
		.pixclock       = 13158,
		.left_margin    = 90,
		.right_margin   = 90,
		.upper_margin   = 17,
		.lower_margin   = 17,
		.hsync_len      = 20,
		.vsync_len      = 4,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= 1,
	.addr	= 2,
	.pixfmt = IPU_PIX_FMT_RGB24,
	.detect = NULL,
	.enable = enable_lvds,
	.mode	= {
		.name           = "LVDS_VXT_VL156-13676YL",
		.refresh        = 60,
		.xres           = 1368,
		.yres           = 768,
		.pixclock       = 13158,
		.left_margin    = 90,
		.right_margin   = 90,
		.upper_margin   = 17,
		.lower_margin   = 17,
		.hsync_len      = 20,
		.vsync_len      = 4,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= 1,
	.addr	= 3,
	.pixfmt = IPU_PIX_FMT_RGB24,
	.detect = NULL,
	.enable = enable_lvds,
	.mode	= {
		.name           = "10inch_v00",
		.refresh        = 60,
		.xres           = 1280,
		.yres           = 800,
		.pixclock       = 1000000000000ULL/71100000,
		.left_margin    = 40,
		.right_margin   = 40,
		.upper_margin   = 10,
		.lower_margin   = 3,
		.hsync_len      = 80,
		.vsync_len      = 10,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= -1,
	.addr	= 0,
	.pixfmt	= IPU_PIX_FMT_RGB24,
	.detect	= detect_hdmi,
	.enable	= do_enable_hdmi,
	.mode	= {
		.name           = "HDMI",
		.refresh        = 60,
		.xres           = 1024,
		.yres           = 768,
		.pixclock       = 15385,
		.left_margin    = 220,
		.right_margin   = 40,
		.upper_margin   = 21,
		.lower_margin   = 7,
		.hsync_len      = 60,
		.vsync_len      = 10,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} }, {
	.bus	= 1,
	.addr	= 0x10,
	.pixfmt	= IPU_PIX_FMT_RGB666,
	.detect	= detect_i2c,
	.enable	= enable_fwadapt_7wvga,
	.mode	= {
		.name           = "FWBADAPT-LCD-F07A-0102",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 480,
		.pixclock       = 33260,
		.left_margin    = 128,
		.right_margin   = 128,
		.upper_margin   = 22,
		.lower_margin   = 22,
		.hsync_len      = 1,
		.vsync_len      = 1,
		.sync           = 0,
		.vmode          = FB_VMODE_NONINTERLACED
} } };
size_t display_count = ARRAY_SIZE(displays);

#ifndef CONFIG_SPL_BUILD
static void setup_display(void)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct iomuxc *iomux = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int reg;

	enable_ipu_clock();
	imx_setup_hdmi();

	reg = __raw_readl(&mxc_ccm->CCGR3);
	reg |=  MXC_CCM_CCGR3_LDB_DI0_MASK | MXC_CCM_CCGR3_LDB_DI1_MASK;
	writel(reg, &mxc_ccm->CCGR3);

	/* set LDB0, LDB1 clk select to 011/011 */
	reg = readl(&mxc_ccm->cs2cdr);
	reg &= ~(MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_MASK
		| MXC_CCM_CS2CDR_LDB_DI1_CLK_SEL_MASK);
	reg |= (3 << MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_OFFSET)
		 | (3 << MXC_CCM_CS2CDR_LDB_DI1_CLK_SEL_OFFSET);
	writel(reg, &mxc_ccm->cs2cdr);

	reg = readl(&mxc_ccm->cscmr2);
	reg |= MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV | MXC_CCM_CSCMR2_LDB_DI1_IPU_DIV;
	writel(reg, &mxc_ccm->cscmr2);

	reg = readl(&mxc_ccm->chsccdr);
	reg |= (CHSCCDR_CLK_SEL_LDB_DI0
		<< MXC_CCM_CHSCCDR_IPU1_DI0_CLK_SEL_OFFSET);
	reg |= (CHSCCDR_CLK_SEL_LDB_DI0
		<< MXC_CCM_CHSCCDR_IPU1_DI1_CLK_SEL_OFFSET);
	writel(reg, &mxc_ccm->chsccdr);

	 reg = IOMUXC_GPR2_BGREF_RRMODE_EXTERNAL_RES
		| IOMUXC_GPR2_DI1_VS_POLARITY_ACTIVE_LOW
		| IOMUXC_GPR2_DI0_VS_POLARITY_ACTIVE_LOW
		| IOMUXC_GPR2_BIT_MAPPING_CH1_SPWG
		| IOMUXC_GPR2_DATA_WIDTH_CH1_24BIT
		| IOMUXC_GPR2_BIT_MAPPING_CH0_SPWG
		| IOMUXC_GPR2_DATA_WIDTH_CH0_24BIT
		| IOMUXC_GPR2_LVDS_CH1_MODE_ENABLED_DI0
		| IOMUXC_GPR2_LVDS_CH0_MODE_ENABLED_DI0;
	writel(reg, &iomux->gpr[2]);
	reg = readl(&iomux->gpr[3]);

	reg = (reg & ~(IOMUXC_GPR3_LVDS0_MUX_CTL_MASK
		| IOMUXC_GPR3_HDMI_MUX_CTL_MASK))
		| (IOMUXC_GPR3_MUX_SRC_IPU1_DI0
		<< IOMUXC_GPR3_LVDS0_MUX_CTL_OFFSET);

	writel(reg, &iomux->gpr[3]);
}

#ifdef CONFIG_SPLASH_SCREEN
static struct splash_location imx_splash_locations[] = {
	{
		.name = "sf",
		.storage = SPLASH_STORAGE_SF,
		.flags = SPLASH_STORAGE_RAW,
		.offset = 0x100000,
	},
	{
		.name = "mmc_fs",
		.storage = SPLASH_STORAGE_MMC,
		.flags = SPLASH_STORAGE_FS,
		.devpart = "0:1",
	},
	{
		.name = "usb_fs",
		.storage = SPLASH_STORAGE_USB,
		.flags = SPLASH_STORAGE_FS,
		.devpart = "0:1",
	},
	{
		.name = "sata_fs",
		.storage = SPLASH_STORAGE_SATA,
		.flags = SPLASH_STORAGE_FS,
		.devpart = "0:1",
	},
};

int splash_screen_prepare(void)
{
	imx_splash_locations[1].devpart[0] = mmc_get_env_dev() + '0';
	return splash_source_load(imx_splash_locations, ARRAY_SIZE(imx_splash_locations));
}
#endif /* CONFIG_SPLASH_SCREEN */
#endif /* CONFIG_SPL_BUILD */
#endif /* CONFIG_DM_VIDEO */

int board_eth_init(struct bd_info *bis)
{
	setup_iomux_enet();

	return cpu_eth_init(bis);
}

int board_early_init_f(void)
{
	setup_iomux_uart();
#ifndef CONFIG_SPL_BUILD
#ifdef CONFIG_DM_VIDEO
	setup_display();
#endif
#endif
	return 0;
}

#ifdef CONFIG_ENV_IS_IN_MMC
static int check_mmc_autodetect(void)
{
	char *autodetect_str = env_get("mmcautodetect");

	if ((autodetect_str != NULL) &&
		(strcmp(autodetect_str, "yes") == 0)) {
		return 1;
	}

	return 0;
}

/* This should be defined for each board */
__weak int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
}

void board_late_mmc_env_init(void)
{
	char cmd[32];
	char mmcblk[32];
	u32 dev_no = mmc_get_env_dev();

	if (!check_mmc_autodetect())
		return;

	env_set_ulong("mmcdev", dev_no);

	/* Set mmcblk env */
	sprintf(mmcblk, "/dev/mmcblk%dp2 rootwait rw",
		mmc_map_to_kernel_blk(dev_no));
	env_set("mmcroot", mmcblk);

	sprintf(cmd, "mmc dev %d", dev_no);
	run_command(cmd, 0);
}
#endif

/*
 * Do not overwrite the console
 * Use always serial for U-Boot console
 */
int overwrite_console(void)
{
	return 1;
}

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"sd0",	  MAKE_CFGVAL(0x40, 0x30, 0x00, 0x00)},
	{"mmc2",	  MAKE_CFGVAL(0x40, 0x20, 0x00, 0x00)},
	{NULL,	 0},
};
#endif

int board_late_init(void)
{
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	char *s, *board;
	char fdt_name[24];

	env_set("som", get_som_type());
	env_set("form", sbc_type());
	env_set("baseboard", sbc_type());

	s = env_get ("fdtfile_autodetect");
	board = env_get ("baseboard");
	if (s != NULL) {
		if (strncmp (s, "off", 3) != 0) {
			if (!strcmp (board, "tep5")) {
				if (detect_usb(&displays[1]) || detect_usb(&displays[2])) {		// detect 15-inch panel
					strncpy(s, "-15inch", 7);
				} else
					s[0] = 0;
			} else
				s[0] = 0;
			if (is_mx6dqp())
				sprintf(fdt_name, "imx6qp-%s%s.dtb", sbc_type(), s);
			else if (is_mx6dq())
				sprintf(fdt_name, "imx6q-%s%s.dtb", sbc_type(), s);
			else
				sprintf(fdt_name, "imx6dl-%s%s.dtb", sbc_type(), s);
		}
		env_set("fdtfile", fdt_name);
	}

	s = env_get ("bootdev_autodetect");
	if (s != NULL) {
		if (strncmp (s, "off", 3) != 0) {
			switch (get_boot_device()) {
			case MMC3_BOOT:
			case SD3_BOOT:
				env_set("bootdev", "MMC3");
				env_set("bootmedia", "mmc");
				break;
			case SD1_BOOT:
				env_set("bootdev", "SD1");
				env_set("bootmedia", "mmc");
				break;
			case SATA_BOOT:
				env_set("bootdev", "SATA");
				env_set("bootmedia", "sata");
				break;
			default:
				printf("Wrong boot device!\r\n");
			}
		}
	}
#endif
	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	setup_iomux_i2c();

	setup_iomux_version_detection();

#ifdef CONFIG_CMD_SATA
	setup_sata();
#endif

	return 0;
}

int checkboard(void)
{
	printf("Board: %s-imx6\n", sbc_type());

	return 0;
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	char *s = strdup(sbc_type());
	if (!strcmp(s, "tep5")) {
		if (is_mx6dq()) {
			if (!strcmp(name, "imx6q-tep5"))
			return 0;
		} else if (is_mx6dl() || is_mx6solo()) {
			if (!strcmp(name, "imx6dl-tep5"))
			return 0;
		}
	} else if (!strcmp(s, "tek3")) {
		if (is_mx6dq()) {
			if (!strcmp(name, "imx6q-tek"))
			return 0;
		} else if (is_mx6dl() || is_mx6solo()) {
			if (!strcmp(name, "imx6dl-tek"))
			return 0;
		}
	}
	return -EINVAL;
}
#endif

#ifdef CONFIG_LDO_BYPASS_CHECK
#ifdef CONFIG_POWER
void ldo_mode_set(int ldo_bypass)
{
	unsigned int value;
	struct pmic *p = pmic_get("PFUZE100");

	if (!p) {
		printf("No PMIC found!\n");
		return;
	}

	/* increase VDDARM/VDDSOC to support 1.2G chip */
	if (check_1_2G()) {
		ldo_bypass = 0;	/* ldo_enable on 1.2G chip */
		printf("1.2G chip, increase VDDARM_IN/VDDSOC_IN\n");

		if (is_mx6dqp()) {
			/* increase VDDARM to 1.425V */
			pmic_reg_read(p, PFUZE100_SW2VOL, &value);
			value &= ~0x3f;
			value |= 0x29;
			pmic_reg_write(p, PFUZE100_SW2VOL, value);
		} else {
			/* increase VDDARM to 1.425V */
			pmic_reg_read(p, PFUZE100_SW1ABVOL, &value);
			value &= ~0x3f;
			value |= 0x2d;
			pmic_reg_write(p, PFUZE100_SW1ABVOL, value);
		}
		/* increase VDDSOC to 1.425V */
		pmic_reg_read(p, PFUZE100_SW1CVOL, &value);
		value &= ~0x3f;
		value |= 0x2d;
		pmic_reg_write(p, PFUZE100_SW1CVOL, value);
	}
}
#elif defined(CONFIG_DM_PMIC_PFUZE100)
void ldo_mode_set(int ldo_bypass)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("pfuze100@8", &dev);
	if (ret == -ENODEV) {
		printf("No PMIC found!\n");
		return;
	}

	/* increase VDDARM/VDDSOC to support 1.2G chip */
	if (check_1_2G()) {
		ldo_bypass = 0; /* ldo_enable on 1.2G chip */
		printf("1.2G chip, increase VDDARM_IN/VDDSOC_IN\n");

		if (is_mx6dqp()) {
			/* increase VDDARM to 1.425V */
			pmic_clrsetbits(dev, PFUZE100_SW2VOL, 0x3f, 0x29);
		} else {
			/* increase VDDARM to 1.425V */
			pmic_clrsetbits(dev, PFUZE100_SW1ABVOL, 0x3f, 0x2d);
		}
		/* increase VDDSOC to 1.425V */
		pmic_clrsetbits(dev, PFUZE100_SW1CVOL, 0x3f, 0x2d);
	}
}
#endif
#endif

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY

int is_recovery_key_pressing(void)
{
	int button_pressed = 0;
	/* TODO recovery mode trigger function */

	return  button_pressed;
}

#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
