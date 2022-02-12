#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_address.h>

#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LEDON 1
#define LEDOFF 0

/*使用了设备树的led驱动*/

/*设备书中添加的成员imx6ull-14x14-evk.dts   最原始的方式，
无法发挥设备书真正的优势，引出pinctrl和gpio子系统

pf_dts_led{
	#address-cell = <1>;
	#size-cells = <1>;
	compatible = "peifeng-led";
	status = "okay";
	reg = < 0X020C406C 0X04 // CCM_CCGR1_BASE 
			0X020E0068 0X04 // SW_MUX_GPIO1_IO03_BASE
			0X020E02F4 0X04 // SW_PAD_GPIO1_IO03_BASE
			0X0209C000 0X04 // GPIO1_DR_BASE 
			0X0209C004 0X04 // GPIO1_GDIR_BASE 
	>;
};

*/


/* 寄存器物理地址 */
/*
#define CCM_CCGR1_BASE				(0X020C406C)	
#define SW_MUX_GPIO1_IO03_BASE		(0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE		(0X020E02F4)
#define GPIO1_DR_BASE				(0X0209C000)
#define GPIO1_GDIR_BASE				(0X0209C004)
*/
/* 映射后的寄存器虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct newchrled_dev{
	dev_t devid;
	int major;
	int minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *dev_nod;
	struct property *propert;
	const char *str;
};

struct newchrled_dev *newchrled;//这里定义了结构体指针，需要为其分配空间。若定义为普通结构体变量，则不需要分配空间

void led_switch(u8 sta){
	u32 val = 0;
	if(sta == LEDON) {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);	
		writel(val, GPIO1_DR);
	}else if(sta == LEDOFF) {
		val = readl(GPIO1_DR);
		val|= (1 << 3);	
		writel(val, GPIO1_DR);
	}

}

static int newchrled_open (struct inode *inod, struct file *filp){
	printk("----------------%s-------------\n",__FUNCTION__);

	//设备文件，file结构体有个叫做private_data的成员变量,				  一般在open的时候将private_data指向设备结构体。
	filp->private_data = newchrled;
	return 0;
}
static ssize_t newchrled_read (struct file *filp , char __user * buf, size_t size, loff_t *offt){
	printk("----------------%s-------------\n",__FUNCTION__);

	return 0;

}
static ssize_t newchrled_write (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt){
	
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	
	printk("----------------%s-------------\n",__FUNCTION__);

	retvalue = copy_from_user(databuf,buf,cnt);
	if(retvalue<0){
		printk("kernel write failed!!\n");
		return -EFAULT;
	}

	ledstat = databuf[0];
	if(ledstat == LEDON){
		led_switch(LEDON);
	}else if(ledstat == LEDOFF){
		led_switch(LEDOFF);
	}
	
	return 0;

}
static int newchrled_release (struct inode *inod, struct file *filp){
	printk("----------------%s-------------\n",__FUNCTION__);

	return 0;

}


static struct file_operations newchrled_fops = {
	.owner = THIS_MODULE,
	.open = newchrled_open,
	.read = newchrled_read,
	.write = newchrled_write,
	.release = newchrled_release,

};


static int __init led_drv_init(void){
	printk("----------------%s-------------\n",__FUNCTION__);

	u32 val = 0;
	//const char *str = NULL;
	int ret;
	u32 regdata[10];
	newchrled = (struct newchrled_dev *)kzalloc(sizeof(struct newchrled_dev), GFP_KERNEL);

	//001.通过路径获取设备树节点信息--pf_dts_led,,在/proc/device-tree下可看到pf_dts_led
	newchrled->dev_nod = of_find_node_by_path("/pf_dts_led");
	if(newchrled->dev_nod == NULL){
		printk("pf_dts_led can not found!!!\n");
		return -EINVAL;
	}else{
		printk("pf_dts_led have been found!!\n");
	}

	//002.获取设备书节点属性--compatible
	newchrled->propert = of_find_property(newchrled->dev_nod, "compatible", NULL);
	if(newchrled->propert == NULL){
		printk("compatible property can not find!!\n");
		return -EINVAL;
	}else{
		printk("compatible property have been found = %s!!\n",(char *)newchrled->propert->value);

	}

	//003.获取status属性内容
	ret = of_property_read_string(newchrled->dev_nod, "status", &newchrled->str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n",newchrled->str);
	}

	//004.获取reg属性内容
	ret = of_property_read_u32_array(newchrled->dev_nod, "reg", regdata, 10);
	if(ret < 0){
		printk("reg read failed!\r\n");
	} else {
		u8 i = 0;
		printk("reg data:\r\n");
		for(i=0;i<10;i++)
			printk("%#X ",regdata[i]);
		printk("\r\n");
	}

	/*
	//01.寄存器映射
	IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
  	SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
	GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
	GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);
	*/

	//01.寄存器映射
#if 0
	IMX6U_CCM_CCGR1 = ioremap(regdata[0], regdata[1]);
	SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
  	SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
	GPIO1_DR = ioremap(regdata[6], regdata[7]);
	GPIO1_GDIR = ioremap(regdata[8], regdata[9]);
#else
	IMX6U_CCM_CCGR1 = of_iomap(newchrled->dev_nod, 0);
	SW_MUX_GPIO1_IO03 = of_iomap(newchrled->dev_nod, 1);
  	SW_PAD_GPIO1_IO03 = of_iomap(newchrled->dev_nod, 2);
	GPIO1_DR = of_iomap(newchrled->dev_nod, 3);
	GPIO1_GDIR = of_iomap(newchrled->dev_nod, 4);
#endif
	//02.使能GPIO时钟
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);	/* 清楚以前的设置 */
	val |= (3 << 26);	/* 设置新值 */
	writel(val, IMX6U_CCM_CCGR1);

	//03.设置GPIO1_io3的复用功能，将其复用为GPIO2_IO3, 
	writel(5, SW_MUX_GPIO1_IO03);

	//04.设置IO属性
	/*寄存器SW_PAD_GPIO1_IO03设置IO属性
	 *bit 16:0 HYS关闭
	 *bit [15:14]: 00 默认下拉
     *bit [13]: 0 kepper功能
     *bit [12]: 1 pull/keeper使能
     *bit [11]: 0 关闭开路输出
     *bit [7:6]: 10 速度100Mhz
     *bit [5:3]: 110 R0/6驱动能力
     *bit [0]: 0 低转换率
	 */
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	//05.设置GPIO1_IO3为输出
	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3);	/* 清除以前的设置 */
	val |= (1 << 3);	/* 设置为输出 */
	writel(val, GPIO1_GDIR);

	
	//06.默认关闭LED
	val = readl(GPIO1_DR);
	val |= (1 << 3);	
	writel(val, GPIO1_DR);

	printk("Creat chardev!!!!\n");

	//该语句被放到入口函数的最前边，因为获取设备树数据时，用到个指针property和str。若不分配空间，就会出现段错误
	//newchrled = (struct newchrled_dev *)kzalloc(sizeof(struct newchrled_dev), GFP_KERNEL);


	printk("have divide ram for struct !!\n");
	//注册字符设备驱动
	//1.像系统申请设备号
	if(newchrled->major){
		//定义了设备号
		newchrled->devid = MKDEV(newchrled->major, 0);
		register_chrdev_region(newchrled->devid, 1, "Pf_led");

	}else{
		//向系统申请设备号
		alloc_chrdev_region(&newchrled->devid, 0, 1, "Pf_led");
		newchrled->major = MAJOR(newchrled->devid);
		newchrled->minor = MINOR(newchrled->devid);
	}
	printk("newchrled major = %d,minor = %d",newchrled->major,newchrled->minor);

	//2.初始化cdev
	newchrled->cdev.owner = THIS_MODULE;
	cdev_init(&newchrled->cdev, &newchrled_fops);

	//3.添加一个字符设备cdev
	cdev_add(&newchrled->cdev, newchrled->devid, 1);

	//4.创建设备
	newchrled->class = class_create(THIS_MODULE, "Pf_led");
	if(IS_ERR(newchrled->class)){
		return PTR_ERR(newchrled->class); 
	}
	newchrled->device = device_create(newchrled->class, NULL, newchrled->devid, NULL, "Pf_led");
	if(IS_ERR(newchrled->device)){
		return PTR_ERR(newchrled->device); 
	}
	return 0;
	
}

static void __exit led_drv_exit(void){
	printk("----------------%s-------------\n",__FUNCTION__);


	//取消内存映射
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	//注销字符设备驱动
	cdev_del(&newchrled->cdev);
	unregister_chrdev_region(newchrled->devid, 1);

	device_destroy(newchrled->class, newchrled->devid);
	class_destroy(newchrled->class);
	kfree(newchrled);
}

module_init(led_drv_init);
module_exit(led_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peifeng");

