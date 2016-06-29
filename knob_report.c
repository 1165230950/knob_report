#include "../../soc/lidbg_target.h"
#include "../../com_api/gpio/gpio.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <uapi/linux/input.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <uapi/linux/input.h>
#include <linux/workqueue.h>

#define BTN_h 4
#define BTN_l 70

extern int gpio_get_value(unsigned gpio);
extern bool SOC_IO_Input(u32 group, u32 index, u32 pull);
extern bool SOC_IO_ISR_Enable(u32 irq);
extern bool SOC_IO_ISR_Add(u32 irq, u32  interrupt_type, pinterrupt_isr func, void *dev);
extern void free_irq(unsigned int irq, void * dev_id);


#define CREATE_KTHREAD(func,data)\
	do{\
		struct task_struct *task;\
		printk(KERN_CRIT "create kthread %s\n","_"#func);\
		task = kthread_create(func, data, "_"#func);\
		if(IS_ERR(task))\
		{\
			printk(KERN_CRIT "Unable to start thread.\n");\
		}\
		else wake_up_process(task);\
	}while(0)\


static struct work_struct work_left_button1;
static struct work_struct work_left_button2;
static struct work_struct work_right_button1;
static struct work_struct work_right_button2;
static struct knob{
	unsigned int Inc;
	unsigned int Dec;
};
struct knob knob_dat;

static int val;

static spinlock_t knob_lock;
static spinlock_t knob_irq_lock;

static struct input_dev *knob_input_dev;

static void work_left_button1_fn(struct work_struct *work)
{
	knob_input_dev->key[BIT_WORD(BTN_h)] = 0;
//	knob_input_dev->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	input_report_key(knob_input_dev, BTN_h, 1);
	input_sync(knob_input_dev);
//	printk(KERN_CRIT "FUNC:%s LINE:%d right knob tune reverse\n",__func__,__LINE__);

}

static void work_left_button2_fn(struct work_struct *work)
{
	knob_input_dev->key[BIT_WORD(BTN_h)] = BIT_MASK(BTN_h);
//	knob_input_dev->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	input_report_key(knob_input_dev, BTN_h, 0);
	input_sync(knob_input_dev);
//	printk(KERN_CRIT "FUNC:%s LINE:%d right knob tune veering\n",__func__,__LINE__);
}

static void work_right_button1_fn(struct work_struct *work)
{
	knob_input_dev->key[BIT_WORD(BTN_l)] = 0;
//	knob_input_dev->keybit[BIT_WORD(BTN_1)] = BIT_MASK(BTN_1);
	input_report_key(knob_input_dev, BTN_l, 1);
	input_sync(knob_input_dev);
//	printk(KERN_CRIT "FUNC:%s LINE:%d left knob vol veering\n",__func__,__LINE__);
}

static void work_right_button2_fn(struct work_struct *work)
{
	knob_input_dev->key[BIT_WORD(BTN_l)] = BIT_MASK(BTN_l);
//	knob_input_dev->keybit[BIT_WORD(BTN_1)] = BIT_MASK(BTN_1);
	input_report_key(knob_input_dev, BTN_l, 0);
	input_sync(knob_input_dev);
//	printk(KERN_CRIT "FUNC:%s LINE:%d left knob vol reverse\n",__func__,__LINE__);
}

void knob_left(int num)
{
	if(num == 1){
		knob_data.Inc ++;
		if(knob_data.Inc <= knob_data.Dec){
			knob_data.Dec --;
			knob_data.Inc --;
			if(!work_pending(&work_left_button1))
				schedule_work(&work_left_button1);
		}
	}else{
		knob_data.Dec ++;
		if(knob_data.Dec <= knob_data.Inc){
			knob_data.Inc --;
			knob_data.Dec --;
			if(!work_pending(&work_left_button2))
				schedule_work(&work_left_button2);
		}
	}
}

irqreturn_t irq_left_button1(int irq, void *dev_id)
{/*
	unsigned long irqflags = 0;
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	disable_irq_nosync(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
	val = SOC_IO_Input(BUTTON_LEFT_2, BUTTON_LEFT_2, 0);
	if(val){
		if(!work_pending(&work_left_button1))
			schedule_work(&work_left_button1);
	}else {
		if(!work_pending(&work_left_button2))
			schedule_work(&work_left_button2);
	}
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	enable_irq(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
*/
	knob_left(1);
	return IRQ_HANDLED;

}
irqreturn_t irq_left_button2(int irq, void *dev_id)
{/*
	int i;
	unsigned long irqflags = 0;
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	disable_irq_nosync(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
	val = SOC_IO_Input(BUTTON_LEFT_1, BUTTON_LEFT_1, 0);
	if(val){
		if(!work_pending(&work_left_button1))
			schedule_work(&work_left_button1);
	}else {
		if(!work_pending(&work_left_button2))
			schedule_work(&work_left_button2);
	}
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	enable_irq(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
*/
	knob_left(2);
	return IRQ_HANDLED;
}




irqreturn_t irq_right_button1(int irq, void *dev_id)
{
	int i;
	unsigned long irqflags = 0;
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	disable_irq_nosync(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
	val = SOC_IO_Input(BUTTON_RIGHT_2, BUTTON_RIGHT_2, 0 /*GPIO_CFG_PULL_UP*/);
	if(val){
		if(!work_pending(&work_right_button1))
			schedule_work(&work_right_button1);
	}else {
		if(!work_pending(&work_right_button2))
			schedule_work(&work_right_button2);
	}
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	enable_irq(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
	return IRQ_HANDLED;
}
irqreturn_t irq_right_button2(int irq, void *dev_id)
{
	int i;
	unsigned long irqflags = 0;
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	disable_irq_nosync(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
	val = SOC_IO_Input(BUTTON_RIGHT_1, BUTTON_RIGHT_1, 0 /*GPIO_CFG_PULL_UP*/);
	if(val){
		if(!work_pending(&work_right_button1))
			schedule_work(&work_right_button1);
	}else{
		if(!work_pending(&work_right_button2))
			schedule_work(&work_right_button2);
	}
	spin_lock_irqsave(&knob_irq_lock, irqflags);
	enable_irq(irq);
	spin_unlock_irqrestore(&knob_irq_lock, irqflags);
	return IRQ_HANDLED;
}

static int knob_probe(void)
{
	int button_en = 1, ret;

	printk(KERN_CRIT "button_init\n");

	spin_lock_init(&knob_lock);
	spin_lock_init(&knob_irq_lock);
	
	if(button_en)
	{
		printk("init work\n");
		INIT_WORK(&work_left_button1, work_left_button1_fn);
		INIT_WORK(&work_left_button2, work_left_button2_fn);

		INIT_WORK(&work_right_button1, work_right_button1_fn);
		INIT_WORK(&work_right_button2, work_right_button2_fn);
		
		printk("request gpio\n");
		SOC_IO_Input(BUTTON_LEFT_1, BUTTON_LEFT_1, 0);
		SOC_IO_Input(BUTTON_LEFT_2, BUTTON_LEFT_2, 0);
		SOC_IO_Input(BUTTON_RIGHT_1, BUTTON_RIGHT_1, 0);
		SOC_IO_Input(BUTTON_RIGHT_2, BUTTON_RIGHT_2, 0);
		printk("request irq\n");
		SOC_IO_ISR_Add(BUTTON_LEFT_1, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, irq_left_button1, "hdj");
		SOC_IO_ISR_Add(BUTTON_LEFT_2, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, irq_left_button2, "hdj");
		SOC_IO_ISR_Add(BUTTON_RIGHT_1, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, irq_right_button1, "hdj");
		SOC_IO_ISR_Add(BUTTON_RIGHT_2, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, irq_right_button2, "hdj");
		
	}
}

static int __init button_dev_init(void)
{
	printk(KERN_WARNING "chdrv_init\n");
	printk(KERN_CRIT "hello_button\n");

	int ret;
	printk(KERN_WARNING "chdrv_init\n");
	printk(KERN_CRIT "hello_knob\n");
	knob_probe();
	printk("register input subsystem\n");
	knob_input_dev = input_allocate_device();
	if(!knob_input_dev){
		printk("knob_input_dev allocate failed\n");
		ret = -1;
		goto err_input_allocate;
	}
	knob_input_dev->evbit[BIT_WORD(EV_KEY)] = BIT_MASK(EV_KEY);
	knob_input_dev->keybit[BIT_WORD(BTN_h)] = BIT_MASK(BTN_h);
	knob_input_dev->keybit[BIT_WORD(BTN_l)] = BIT_MASK(BTN_l);
	if(input_register_device(knob_input_dev))
	{
		printk("knob input device register failed\n");
		ret = -1;
		goto err_input_dev;
	}
	printk("input subsystem register ok !\n");
	knob_data.Inc = 0;
	knob_data.Dec = 0;

	return 0;
	
err_input_dev:
	input_free_device(knob_input_dev);
err_input_allocate:
	
	return ret;

}

static void __exit button_dev_exit(void)
{
	printk("chdrv_exit\n");

	free_irq(262, "hdj");
	free_irq(263, "hdj");
	free_irq(264, "hdj");
	free_irq(265, "hdj");
	
	gpio_free(BUTTON_LEFT_1);
	gpio_free(BUTTON_LEFT_2);
	gpio_free(BUTTON_RIGHT_1);
	gpio_free(BUTTON_RIGHT_2);

	input_free_device(knob_input_dev);
}

//EXPORT_SYMBOL(button_suspend);
//EXPORT_SYMBOL(button_resume);
module_init(button_dev_init);
module_exit(button_dev_exit);



MODULE_AUTHOR("fly, <fly@gmail.com>");
MODULE_DESCRIPTION("Devices Driver");
MODULE_LICENSE("GPL");





