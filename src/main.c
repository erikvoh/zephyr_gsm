/*
 * Copyright (c) 2020, Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <sys/printk.h>
#include <shell/shell.h>
#include <drivers/uart.h>
#include <net/net_mgmt.h>
#include <net/net_event.h>
#include <net/net_conn_mgr.h>
#include <drivers/modem/gsm_ppp.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#include <logging/log.h>



LOG_MODULE_REGISTER(sample_gsm_ppp, LOG_LEVEL_DBG);

#define GSM_MODEM_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_gsm_ppp)
#define UART_NODE DT_BUS(GSM_MODEM_NODE)

static const struct device *const gsm_dev = DEVICE_DT_GET(GSM_MODEM_NODE);
static struct net_mgmt_event_callback mgmt_cb;
static bool starting = IS_ENABLED(CONFIG_GSM_PPP_AUTOSTART);

static int cmd_sample_modem_suspend(const struct shell *shell,
				    size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!starting) {
		shell_fprintf(shell, SHELL_NORMAL, "Modem is already stopped.\n");
		return -ENOEXEC;
	}

	gsm_ppp_stop(gsm_dev);
	starting = false;

	return 0;
}

static int cmd_sample_modem_resume(const struct shell *shell,
				   size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (starting) {
		shell_fprintf(shell, SHELL_NORMAL, "Modem is already started.\n");
		return -ENOEXEC;
	}

	gsm_ppp_start(gsm_dev);
	starting = true;

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sample_commands,
	SHELL_CMD(resume, NULL,
		  "Resume the modem\n",
		  cmd_sample_modem_resume),
	SHELL_CMD(suspend, NULL,
		  "Suspend the modem\n",
		  cmd_sample_modem_suspend),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sample, &sample_commands,
		   "Sample application commands", NULL);


static void event_handler(struct net_mgmt_event_callback *cb,
			  uint32_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if ((mgmt_event & (NET_EVENT_L4_CONNECTED
			   | NET_EVENT_L4_DISCONNECTED)) != mgmt_event) {
		return;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		printk("Network connected");
		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		printk("Network disconnected");
		return;
	}
}

static void modem_on_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_INF("GSM modem on callback fired");
}

static void modem_off_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_INF("GSM modem off callback fired");
}

int main(void)
{
	const struct device *modemon_gpio_dev;
	const struct device *sim_select_gpio_dev;
	int ret = 0;

#if DT_PHA_HAS_CELL(DT_ALIAS(simselect), gpios, pin) && \
    DT_NODE_HAS_PROP(DT_ALIAS(simselect), gpios)
	sim_select_gpio_dev = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(simselect), gpios));
	if (!sim_select_gpio_dev) {
		printk("SIM: Device driver not found.\n");
		return 0;
	}

	ret = gpio_pin_configure(sim_select_gpio_dev,
				 DT_GPIO_PIN(DT_ALIAS(simselect), gpios),
				 GPIO_OUTPUT_LOW |
				 DT_GPIO_FLAGS(DT_ALIAS(simselect), gpios));
	if (ret < 0) {
		printk("Error setting sim pin to output mode [%d]", ret);
	}
#endif


#if DT_PHA_HAS_CELL(DT_ALIAS(simselect), gpios, pin) && \
    DT_NODE_HAS_PROP(DT_ALIAS(simselect), gpios)

	if (!sim_select_gpio_dev) {
		printk("No MODEM ON GPIO device\n");
		return 0;
	}
		gpio_pin_set(sim_select_gpio_dev,
			     DT_GPIO_PIN(DT_ALIAS(simselect), gpios), 0);
#else
#error "DT error"				 
#endif


#if DT_PHA_HAS_CELL(DT_ALIAS(modemon), gpios, pin) && \
    DT_NODE_HAS_PROP(DT_ALIAS(modemon), gpios)
	modemon_gpio_dev = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(modemon), gpios));
	if (!modemon_gpio_dev) {
		printk("MODEM ON: Device driver not found.\n");
		return 0;
	}

	ret = gpio_pin_configure(modemon_gpio_dev,
				 DT_GPIO_PIN(DT_ALIAS(modemon), gpios),
				 GPIO_OUTPUT_HIGH |
				 DT_GPIO_FLAGS(DT_ALIAS(modemon), gpios));
	if (ret < 0) {
		printk("Error setting modem on pin to output mode [%d]", ret);
	}
#endif


#if DT_PHA_HAS_CELL(DT_ALIAS(modemon), gpios, pin) && \
    DT_NODE_HAS_PROP(DT_ALIAS(modemon), gpios)

	if (!modemon_gpio_dev) {
		printk("No MODEM ON GPIO device\n");
		return 0;
	}
		gpio_pin_set(modemon_gpio_dev,
			     DT_GPIO_PIN(DT_ALIAS(modemon), gpios), 1);
#else
#error "DT error"				 
#endif


	printk("\n\n\t\t Modem ON\n\n");


	k_msleep (5000);

	const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);

	/* Optional register modem power callbacks */
	// gsm_ppp_register_modem_power_callback(gsm_dev, modem_on_cb, modem_off_cb, NULL);

	printk("Board '%s' APN '%s' UART '%s' device %p (%s)",
		CONFIG_BOARD, CONFIG_MODEM_GSM_APN,
		uart_dev->name, uart_dev, gsm_dev->name);

	net_mgmt_init_event_callback(&mgmt_cb, event_handler,
				     NET_EVENT_L4_CONNECTED |
				     NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&mgmt_cb);

	// gsm_ppp_stop(gsm_dev);
	// k_msleep (5000);
	// gsm_ppp_start(gsm_dev);
	// k_msleep (5000);

	return 0;
}