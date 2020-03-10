#include <stdio.h>
#include "libusb.h"
int ControlDevice(int vid, int pid, char requesttype, char request, short value, short index, char *buff, int len);
int switchReport(int vid, int pid, unsigned char *buffer, int buffer_size, unsigned char *returnbuffer, int returnbuffer_size);

struct userDevice
{
	/*Device descriptor*/
	/** USB-IF vendor ID */
	uint16_t idVendor;
	/** USB-IF product ID */
	uint16_t idProduct;
	/*Interface descriptor*/
	/** USB-IF class code for this interface. See \ref libusb_class_code. */
	unsigned char bInterfaceClass;
	/** USB-IF subclass code for this interface, qualified by the
	 * bInterfaceClass value */
	unsigned char bInterfaceSubClass;
	/*Endpoint descriptor*/
	/** Attributes which apply to the endpoint when it is configured using
	 * the bConfigurationValue. Bits 0:1 determine the transfer type and
	 * correspond to \ref libusb_transfer_type. Bits 2:3 are only used for
	 * isochronous endpoints and correspond to \ref libusb_iso_sync_type.
	 * Bits 4:5 are also only used for isochronous endpoints and correspond to
	 * \ref libusb_iso_usage_type. Bits 6:7 are reserved.
	 */
	unsigned char bmAttributes;
	/*save parameter*/
	libusb_device *dev;
	unsigned char bInEndpointAddress;
	unsigned char bOutEndpointAddress;
	/* Number of this interface */
	unsigned char bInterfaceNumber;
};

int init_libusb(void)
{
	/*1. init libusb lib*/
	int rv = libusb_init(NULL);
	if (rv < 0)
	{
		printf("*** initial USB lib failed! \n");
		return -1;
	}
	return rv;
}

int get_device_descriptor(struct libusb_device_descriptor *dev_desc, struct userDevice *user_device)
{
	int rv = -2, i = 0, j, k;
	ssize_t cnt;
	libusb_device **devs;
	libusb_device *dev;
	struct libusb_config_descriptor *conf_desc;
	unsigned char isFind = 0;
	cnt = libusb_get_device_list(NULL, &devs); //check the device number
	if (cnt < 0)
		return (int)cnt;
	while ((dev = devs[i++]) != NULL)
	{
		rv = libusb_get_device_descriptor(dev, dev_desc);
		if (rv < 0)
		{
			printf("*** libusb_get_device_descriptor failed! i:%d \n", i);
			return -1;
		}
		if (dev_desc->idProduct == user_device->idProduct && dev_desc->idVendor == user_device->idVendor)
		{
			user_device->dev = dev;
			rv = 0;
			break;
		}
	}

	//return 0;
	for (i = 0; i < dev_desc->bNumConfigurations; i++)
	{
		if (user_device->dev != NULL)
			rv = libusb_get_config_descriptor(user_device->dev, i, &conf_desc);
		if (rv < 0)
		{
			printf("*** libusb_get_config_descriptor failed! \n");
			return -1;
		}
		for (j = 0; j < conf_desc->bNumInterfaces; j++)
			for (k = 0; k < conf_desc->interface[j].num_altsetting; k++)
			{
				//枚举找到端点描述符
				if (conf_desc->interface[j].altsetting[k].bInterfaceClass != user_device->bInterfaceClass)
					continue;
				if (!match_with_endpoint(&(conf_desc->interface[j].altsetting[k]), user_device))
					continue;
				user_device->bInterfaceNumber = conf_desc->interface[j].altsetting[k].bInterfaceNumber;
				libusb_free_config_descriptor(conf_desc);
				rv = 0;
				return rv;
			}
	}
	//return 0;
	libusb_free_device_list(devs, 1);
	return rv;
}

int match_with_endpoint(const struct libusb_interface_descriptor *interface, struct userDevice *user_device)
{
	int i, ret = 0;
	for (i = 0; i < interface->bNumEndpoints; i++)
	{
		if ((interface->endpoint[i].bmAttributes & 0x03) != user_device->bmAttributes)
			continue;																					//transfer type :bulk ,control, interrupt
		if (interface->endpoint[i].bEndpointAddress & 0x80) //out endpoint & in endpoint
		{
			ret |= 1;
			user_device->bInEndpointAddress = interface->endpoint[i].bEndpointAddress;
		}
		else
		{
			ret |= 2;
			user_device->bOutEndpointAddress = interface->endpoint[i].bEndpointAddress;
		}
	}
	return (ret == 3 ? 1 : 0);
}

int testidcard()
{
	unsigned char buff[512] = {0xaa, 0xaa, 0xaa, 0x96, 0x69, 0x00, 0x03, 0x20, 0x01, 0x21};
	unsigned char retbuff[0x50];
	int retlen;
	switchReportBulk(0x400,0xc35a,buff,10,retbuff,&retlen);
	return 0;
}

void testk80()
{
	unsigned char buff[11] = {0x1b, 0x40, 0x1b, 0x3c, 0x1b, 0x33, 0x18, 0x30, 0x31, 0x32, 0x0A};
	WriteDevice(0x0DD4, 0x0237, buff, 11);
}

int main()
{
	printf("000\r\n");
	testidcard();
	//testk80();
	return 0;
}

int switchReport(int vid, int pid, unsigned char *buffer, int buffer_size, unsigned char *returnbuffer, int returnbuffer_size)
{
	int rv;
	init_libusb();
	libusb_device_handle *usb_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if (usb_handle == NULL)
	{
		printf("*** Permission denied or Can not find the USB Device!\n");
		return -1;
	}
	rv = libusb_control_transfer(usb_handle, 0x21, 0x09, 0x0301, 0x00, buffer, buffer_size, 1000);
	if (rv < 0)
		printf("*** write failed! %d\n", rv);
	return -1;
	rv = libusb_control_transfer(usb_handle, 0xA1, 0x01, 0x0100, 0x00, returnbuffer, returnbuffer_size, 1000);
	if (rv < 0)
	{
		printf("*** read failed! %d\n", rv);
		return -1;
	}
	libusb_close(usb_handle);
	libusb_exit(NULL);
	return rv;
}

int switchReportBulk(int vid, int pid, unsigned char *buffer, int buffer_size, unsigned char *returnbuffer, int* returnbuffer_size)
{
	int rv = -2, i = 0, j, k, l, length;
	ssize_t iret;
	libusb_context *ctx = NULL;
	libusb_device **devs;
	libusb_device_handle *g_usb_handle;					//设备句柄
	struct libusb_config_descriptor *conf_desc; //配置描述符
	struct libusb_device_descriptor dev_desc;		//设备描述符
	struct libusb_interface_descriptor interface;
	struct userDevice user_device;

	libusb_init(&ctx);
	//libusb_set_debug(ctx, 4);
	iret = libusb_get_device_list(NULL, &devs); //check the device number
	if (iret < 0)
	{
		libusb_exit(ctx);
		return (int)iret;
	}

	{ //获取设备描述符.对比vid pid
		unsigned char isFound = 0;
		while ((user_device.dev = devs[i++]) != NULL)
		{
			iret = libusb_get_device_descriptor(user_device.dev, &dev_desc);
			if (iret < 0)
				continue;
			if (dev_desc.idVendor == vid && dev_desc.idProduct == pid)
			{
				isFound = 1;
				break;
			}
		}

		if (isFound == 0)
		{
			libusb_exit(ctx);
			libusb_free_device_list(devs, 1);
			return -2;
		}
	}
	libusb_free_device_list(devs, 1);

	int isdetached = 0;
	{ //打开设备
		iret = libusb_open(user_device.dev, &g_usb_handle);
		if (iret < 0)
		{
			libusb_exit(ctx);
			return -2;
		}

		{																												 //声明接口
			if (libusb_kernel_driver_active(g_usb_handle, 0) == 1) //?
			{
				if (libusb_detach_kernel_driver(g_usb_handle, 0) < 0)
				{
					libusb_close(g_usb_handle);
					libusb_exit(ctx);
					return -3;
				}
				isdetached = 1;
			}
			if (libusb_claim_interface(g_usb_handle, 0) < 0)
			{
				if (libusb_detach_kernel_driver(g_usb_handle, 0) < 0)
				{
					libusb_close(g_usb_handle);
					libusb_exit(ctx);
					return -3;
				}
				isdetached = 1;
				if (libusb_claim_interface(g_usb_handle, 0) < 0)
				{
					libusb_close(g_usb_handle);
					libusb_exit(ctx);
					return -3;
				}
			}
		}
	}

	{ //声明接口
		//枚举设备有多少个配置描述符
		for (i = 0; i < dev_desc.bNumConfigurations; i++)
		{
			//读取配置描述符
			if (libusb_get_config_descriptor(user_device.dev, i, &conf_desc) < 0)
				continue;
			iret = 0;
			// 此配置所支持的接口数量
			for (j = 0; j < conf_desc->bNumInterfaces; j++)
			{
				// 根据接口的设置数量枚举
				for (k = 0; k < conf_desc->interface[j].num_altsetting; k++)
				{
					interface = conf_desc->interface[j].altsetting[k];
					//枚举找到端点描述符
					for (l = 0; l < interface.bNumEndpoints; l++)
					{
						if ((interface.endpoint[l].bmAttributes & LIBUSB_TRANSFER_TYPE_BULK) == 0)
							continue;																				 //判断是否支持指定类型的传输方式
						if (interface.endpoint[l].bEndpointAddress & 0x80) //out endpoint & in endpoint
						{
							iret |= 1;
							user_device.bInEndpointAddress = interface.endpoint[l].bEndpointAddress;
						}
						else
						{
							iret |= 2;
							user_device.bOutEndpointAddress = interface.endpoint[l].bEndpointAddress;
						}
					}
					user_device.bInterfaceNumber = interface.bInterfaceNumber;
				}
			}
			libusb_free_config_descriptor(conf_desc);
		}
	}

	if (isdetached == 1)
		libusb_attach_kernel_driver(g_usb_handle, 0);

	if (iret != 3)
	{
		printf("*** endpoint not enough! \n");
		libusb_close(g_usb_handle);
		libusb_exit(ctx);
		return -3;
	}
	//printf("3\r\n");

	rv = libusb_bulk_transfer(g_usb_handle, user_device.bOutEndpointAddress, buffer, buffer_size, &length, 1000);
	if (rv < 0)
	{
		printf("*** bulk_transfer failed! \n");
		libusb_close(g_usb_handle);
		libusb_exit(ctx);
		return -1;
	}

	//printf("writed\r\n");
	rv = libusb_bulk_transfer(g_usb_handle, user_device.bInEndpointAddress, returnbuffer, 64, &length, 100);
	libusb_close(g_usb_handle);
	//int rv = switchReportBulk(0x0400, 0Xc35A, buff, 10, retbuff, 0x40);
	if (rv < 0)
	{
		printf("*** bulk_transfer recv failed! rv=%s\n", libusb_error_name(rv));
		libusb_exit(ctx);
		return -1;
	}
	returnbuffer_size = (returnbuffer[5] * 0x100) + returnbuffer[6] + 5;
	for (i = length; i <= returnbuffer_size;)
	{
		rv = libusb_bulk_transfer(g_usb_handle, user_device.bInEndpointAddress, &(returnbuffer[i]), 64, &length, 100);
		if (rv < 0)
		{
			printf("*** bulk_transfer recv2 failed! rv=%s\n", libusb_error_name(rv));
			libusb_exit(ctx);
			return -1;
		}
		printf("i=%d\n",i);
		i += length;
	}

	printf("%d %d\r\n",length, returnbuffer_size);

	libusb_exit(ctx);
	return rv;
}

int WriteDevice(int vid, int pid, char *buff, int len)
{
	int rv, length;
	libusb_device_handle *g_usb_handle;
	struct userDevice user_device;
	struct libusb_device_descriptor dev_desc;
	user_device.idProduct = pid;
	user_device.idVendor = vid;
	user_device.bInterfaceClass = LIBUSB_CLASS_PRINTER;
	user_device.bInterfaceSubClass = LIBUSB_CLASS_PRINTER;
	user_device.bmAttributes = LIBUSB_TRANSFER_TYPE_BULK;
	user_device.dev = NULL;
	//printf("%d %d\n",user_device.bInEndpointAddress,user_device.bOutEndpointAddress);
	init_libusb();
	rv = get_device_descriptor(&dev_desc, &user_device);
	if (rv < 0)
	{
		printf("*** get_device_descriptor failed! \n");
		return -1;
	}
	//printf("%d %d\n",user_device.bInEndpointAddress,user_device.bOutEndpointAddress);
	g_usb_handle = libusb_open_device_with_vid_pid(NULL, user_device.idVendor, user_device.idProduct);
	if (g_usb_handle == NULL)
	{
		printf("*** Permission denied or Can not find the USB board (Maybe the USB driver has not been installed correctly), quit!\n");
		return -1;
	}
	rv = libusb_claim_interface(g_usb_handle, user_device.bInterfaceNumber);
	if (rv < 0)
	{
		rv = libusb_detach_kernel_driver(g_usb_handle, user_device.bInterfaceNumber);
		if (rv < 0)
		{
			printf("*** libusb_detach_kernel_driver failed! rv%d\n", rv);
			return -1;
		}
		rv = libusb_claim_interface(g_usb_handle, user_device.bInterfaceNumber);
		if (rv < 0)
		{
			printf("*** libusb_claim_interface failed! rv%d\n", rv);
			return -1;
		}
	}
	rv = libusb_bulk_transfer(g_usb_handle, user_device.bOutEndpointAddress, buff, len, &length, 1000);
	if (rv < 0)
	{
		printf("*** bulk_transfer failed! \n");
		return -1;
	}
	libusb_close(g_usb_handle);
	libusb_release_interface(g_usb_handle, user_device.bInterfaceNumber);
	libusb_exit(NULL);
	return 0;
}

int ReadDevice(int vid, int pid, char *buff, int len)
{
	int rv, length;
	libusb_device_handle *g_usb_handle;
	struct userDevice user_device;
	struct libusb_device_descriptor dev_desc;
	user_device.idProduct = pid;
	user_device.idVendor = vid;
	user_device.bInterfaceClass = LIBUSB_CLASS_PRINTER;
	user_device.bInterfaceSubClass = LIBUSB_CLASS_PRINTER;
	user_device.bmAttributes = LIBUSB_TRANSFER_TYPE_BULK;
	user_device.dev = NULL;
	//printf("%d %d\n",user_device.bInEndpointAddress,user_device.bOutEndpointAddress);
	init_libusb();
	rv = get_device_descriptor(&dev_desc, &user_device);
	if (rv < 0)
	{
		printf("*** get_device_descriptor failed! \n");
		return -1;
	}
	//printf("%d %d\n",user_device.bInEndpointAddress,user_device.bOutEndpointAddress);
	g_usb_handle = libusb_open_device_with_vid_pid(NULL, user_device.idVendor, user_device.idProduct);
	if (g_usb_handle == NULL)
	{
		printf("*** Permission denied or Can not find the USB board (Maybe the USB driver has not been installed correctly), quit!\n");
		return -1;
	}
	rv = libusb_claim_interface(g_usb_handle, user_device.bInterfaceNumber);
	if (rv < 0)
	{
		rv = libusb_detach_kernel_driver(g_usb_handle, user_device.bInterfaceNumber);
		if (rv < 0)
		{
			printf("*** libusb_detach_kernel_driver failed! rv%d\n", rv);
			return -1;
		}
		rv = libusb_claim_interface(g_usb_handle, user_device.bInterfaceNumber);
		if (rv < 0)
		{
			printf("*** libusb_claim_interface failed! rv%d\n", rv);
			return -1;
		}
	}
	rv = libusb_bulk_transfer(g_usb_handle, user_device.bInEndpointAddress, buff, len, &length, 1000);
	if (rv < 0)
	{
		printf("*** bulk_transfer failed! \n");
		return -1;
	}
	libusb_close(g_usb_handle);
	libusb_release_interface(g_usb_handle, user_device.bInterfaceNumber);
	libusb_exit(NULL);
	return 0;
}

int ControlDevice(int vid, int pid, char requesttype, char request, short value, short index, char *buff, int len)
{
	int rv, length;
	libusb_device_handle *g_usb_handle;
	init_libusb();
	g_usb_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if (g_usb_handle == NULL)
	{
		printf("*** Permission denied or Can not find the USB board (Maybe the USB driver has not been installed correctly), quit!\n");
		libusb_exit(NULL);
		return -1;
	}
	rv = libusb_control_transfer(g_usb_handle, requesttype, request, value, index, buff, len, 1000);
	if (rv < 0)
	{
		printf("***  bulk_transfer failed! %d\n", rv);
		return -1;
	}
	libusb_close(g_usb_handle);
	//libusb_release_interface(g_usb_handle,LIBUSB_CLASS_PRINTER);
	libusb_exit(NULL);
	return rv;
}
