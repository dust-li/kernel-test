#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#define SHT_TYPE_MAX_LEN 	64
#define SHT_AVAI_TYPE_MAX_LEN 	(SHT_TYPE_MAX_LEN * 		\
				sizeof(sht_available_type) / sizeof(char*))

static struct ctl_table_header *sht_table_header;
static struct timer_list sht_timer;

static const char *sht_available_type[] = {
	"thread",
	"timer",
};

/* sysctl variables */
static char sht_current_type[SHT_TYPE_MAX_LEN];
static int delay_us = 5000;

static char *format_delay(int us)
{
	int index = 0;
	static char buff[32];
	const char *time_format[] = {
		"us",
		"ms",
		"s"
	};

	for (;;) {
		if (us < 1000)
			break;
		us /= 1000;
		index ++;
	}

	snprintf(buff, 32, "%d%s", us, time_format[index]);
	return buff;
}

static int proc_do_sht_available_type(struct ctl_table *table, int write,
				      void *buffer, size_t *lenp, loff_t *ppos)
{
	int i;
	int ret;
	int nr_type = sizeof(sht_available_type)/sizeof(char *);
	int maxlen = SHT_TYPE_MAX_LEN * nr_type;
	struct ctl_table tbl = { .maxlen = maxlen, };
	size_t off = 0;

	char *buf = kmalloc(maxlen, GFP_USER);
	if (!buf)
		return -ENOMEM;
	tbl.data = buf;

	for (i = 0; i < nr_type; i++) {
		off += snprintf(buf + off, SHT_TYPE_MAX_LEN, "%s%s",
			 sht_available_type[i], i == nr_type - 1 ? "" : " ");
	}
	ret = proc_dostring(&tbl, write, buffer, lenp, ppos);
	kfree(buf);
	return ret;
}

static int proc_do_sht_type(struct ctl_table *table, int write, void *buffer,
		            size_t *lenp, loff_t *ppos)
{
	char val[SHT_TYPE_MAX_LEN];
	int nr_type = sizeof(sht_available_type)/sizeof(char *);
	struct ctl_table tbl = {
		.data = val,
		.maxlen = SHT_TYPE_MAX_LEN,
	};
	int ret;
	int i;

	if (write) {
		ret = proc_dostring(&tbl, write, buffer, lenp, ppos);
		if (ret)
			return ret;

		for (i = 0; i < nr_type; i++)
			if (!strncmp(val, sht_available_type[i], SHT_TYPE_MAX_LEN)) {
				strncpy(sht_current_type, sht_available_type[i],
						SHT_TYPE_MAX_LEN);
				return 0;
			}
		return -EINVAL;
	} else {
		tbl.data = sht_current_type;
		ret = proc_dostring(&tbl, write, buffer, lenp, ppos);
	}

	return ret;
}

static void hang_for_us(int time_in_us)
{
	unsigned long long end;

	end = sched_clock() + time_in_us * 1000;

	/* Stupid busy waitting WITHOUT sched_cond() */
	while (sched_clock() < end) ;
}

static void start_thread_hang(void)
{
	int cpu = smp_processor_id();
	printk("start thread hang for %s on CPU: %d\n", format_delay(delay_us), cpu);
	hang_for_us(delay_us);
}

static void sht_timer_fn(struct timer_list *t)
{
	int cpu = smp_processor_id();
	printk("start timer hang for %s on CPU:%d\n", format_delay(delay_us), cpu);
	hang_for_us(delay_us);
}

static void start_timer_hang(void)
{
	mod_timer(&sht_timer, jiffies);
}

static int proc_do_sht_delay(struct ctl_table *table, int write, void *buffer,
		             size_t *lenp, loff_t *ppos)
{
	proc_dointvec(table, write, buffer, lenp, ppos);

	if (write) {
		if (delay_us < 1)
			return 0;

		if (strncmp(sht_current_type, "thread", SHT_TYPE_MAX_LEN) == 0)
			start_thread_hang();
		else if (strncmp(sht_current_type, "timer", SHT_TYPE_MAX_LEN) == 0)
			start_timer_hang();
		else
			return -EINVAL;
	}

	return 0;
}

static struct ctl_table sht_tunable_table[] = {
	{
		.procname       = "current_type",
		.data           = sht_current_type,
		.maxlen         = SHT_TYPE_MAX_LEN,
		.mode           = 0644,
		.proc_handler   = proc_do_sht_type,
	},
	{
		.procname       = "available_type",
		.maxlen         = SHT_AVAI_TYPE_MAX_LEN,
		.mode           = 0444,
		.proc_handler   = proc_do_sht_available_type,
	},
	{
		.procname       = "delay_us",
		.maxlen         = sizeof(int),
		.data           = &delay_us,
		.mode           = 0644,
		.proc_handler   = proc_do_sht_delay,
	},
	{ },
};

static struct ctl_table sht_table[] = {
	{
		.procname 	= "simu_hang_task",
		.mode 		= 0555,
		.child 		= sht_tunable_table,
	},
	{ },
};

static int prepare_sysctl(void)
{
	if (!sht_table_header)
		sht_table_header = register_sysctl_table(sht_table);
	if (!sht_table_header)
		return -1;

	strncpy(sht_current_type, sht_available_type[0], SHT_TYPE_MAX_LEN);

	return 0;
}

static int __init sht_init(void)
{
	printk(KERN_WARNING "Init simu_hang_task module, This MUST NOT be a "
			"online server, run at your OWN RISK\n");
	if (prepare_sysctl()) {
		printk(KERN_ERR "simulation hang task init Failed\n");
		return 1;
	}

	/* Use TIMER_PINNED to make sure the timer always run on
	 * the sysctl CPU, so that use application can use taskset
	 * to control which CPU he want the timer will be run on */
	timer_setup(&sht_timer, sht_timer_fn, TIMER_PINNED);

	printk(KERN_INFO "simulation hang task init success\n");
	return 0;
}

static void __exit sht_exit(void)
{
	if (sht_table_header)
		unregister_sysctl_table(sht_table_header);

	/* Always delete the timer after sysctl has been unregistered
	 * to prevent race */
	del_timer_sync(&sht_timer);
	printk("Bye: simulation hang task!\n");
}

module_init(sht_init);
module_exit(sht_exit);
MODULE_DESCRIPTION("simulate hang task module");
MODULE_AUTHOR("dust.li@alibaba.linux.com");
MODULE_LICENSE("GPL");
