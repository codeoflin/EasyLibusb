#include <stdio.h>
#include "libusb.h"
#define BULK_ENDPOINT_OUT 2
#define BULK_ENDPOINT_IN 1
int ControlDevice(int vid, int pid, char requesttype, char request, short value, short index, char *buff, int len);
int switchReport(int vid, int pid, unsigned char *buffer, int buffer_size, unsigned char *returnbuffer, int returnbuffer_size);
int WriteDevice2(int vid, int pid, char *buff, int len);

struct userDevice
{
	/*Device descriptor*/
	/** USB-IF vendor ID */
	uint16_t idVendor;
	/** USB-IF product ID */
	uint16_t idProduct;
	/*Interface descriptor*/
	/** USB-IF class code for this interface. See \ref libusb_class_code. */
	uint8_t bInterfaceClass;
	/** USB-IF subclass code for this interface, qualified by the
	 * bInterfaceClass value */
	uint8_t bInterfaceSubClass;
	/*Endpoint descriptor*/
	/** Attributes which apply to the endpoint when it is configured using
	 * the bConfigurationValue. Bits 0:1 determine the transfer type and
	 * correspond to \ref libusb_transfer_type. Bits 2:3 are only used for
	 * isochronous endpoints and correspond to \ref libusb_iso_sync_type.
	 * Bits 4:5 are also only used for isochronous endpoints and correspond to
	 * \ref libusb_iso_usage_type. Bits 6:7 are reserved.
	 */
	uint8_t bmAttributes;
	/*save parameter*/
	libusb_device *dev;
	u_int8_t bInEndpointAddress;
	u_int8_t bOutEndpointAddress;
	/* Number of this interface */
	uint8_t bInterfaceNumber;
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
	u_int8_t isFind = 0;
	cnt = libusb_get_device_list(NULL, &devs);//check the device number
	if (cnt < 0)	return (int)cnt;
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
		if(user_device->dev != NULL)rv = libusb_get_config_descriptor(user_device->dev,i,&conf_desc);
		if(rv < 0) {printf("*** libusb_get_config_descriptor failed! \n");return -1;}
		for (j = 0; j < conf_desc->bNumInterfaces; j++)
			for (k = 0; k < conf_desc->interface[j].num_altsetting; k++)
			{
				//枚举找到端点描述符
				if (conf_desc->interface[j].altsetting[k].bInterfaceClass != user_device->bInterfaceClass)continue;
				if (!match_with_endpoint(&(conf_desc->interface[j].altsetting[k]), user_device))continue;
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
		if ((interface->endpoint[i].bmAttributes & 0x03) != user_device->bmAttributes)	continue;	//transfer type :bulk ,control, interrupt
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
	int rv = -2, i = 0, j, k,length;
	ssize_t iret;
	libusb_device **devs;
	struct libusb_config_descriptor conf_desc;
	struct libusb_device_descriptor dev_desc;
	struct userDevice user_device;
	libusb_device_handle *g_usb_handle;
	u_int8_t isFind = 0;
	init_libusb();
	
	printf("0\r\n");
	iret = libusb_get_device_list(NULL, &devs);//check the device number
	if (iret < 0)	return (int)iret;
	
	printf("01\r\n");
	while ((user_device.dev = devs[i++]) != NULL)
	{
		iret = libusb_get_device_descriptor(user_device.dev, &dev_desc);
		if (iret < 0)
		{
			printf("*** libusb_get_device_descriptor failed! i:%d \n", i);
			return -1;
		}
		if (dev_desc.idVendor == 0xdd4 && dev_desc.idProduct == 0x237  )
		{
			isFind = 1;
			break;
		}
	}
	
	if(isFind==0)return -2;
	printf("02\r\n");
	iret=libusb_open(user_device.dev,g_usb_handle);
	if(iret<0)
	{
		libusb_free_device_list(devs,1);
		return -2;
	}
	printf("1\r\n");
	libusb_get_config_descriptor(user_device.dev,i,&conf_desc);
	int isdetached=0;
	if(libusb_kernel_driver_active(g_usb_handle,dev_desc->bNumConfigurations)==1)//?
	{
		if(libusb_detach_kernel_driver(g_usb_handle,dev_desc->bNumConfigurations)>=0)isdetached=1;
	}
	printf("2\r\n");
	if(libusb_claim_interface(g_usb_handle, user_device.bInterfaceNumber)>=0)
	{
		for (i = 0; i < dev_desc->bNumConfigurations; i++)
		{
			if(rv < 0) {printf("*** libusb_get_config_descriptor failed! \n");return -1;}
			for (j = 0; j < conf_desc->bNumInterfaces; j++)
				for (k = 0; k < conf_desc->interface[j].num_altsetting; k++)
				{
					//枚举找到端点描述符
					if (conf_desc->interface[j].altsetting[k].bInterfaceClass != user_device.bInterfaceClass)continue;
					if (!match_with_endpoint(&(conf_desc->interface[j].altsetting[k]), &user_device))continue;
					user_device.bInterfaceNumber = conf_desc->interface[j].altsetting[k].bInterfaceNumber;
					libusb_free_config_descriptor(conf_desc);
					iret = 0;
					return rv;
				}
		}
	}
	printf("3\r\n");
	libusb_free_device_list(devs, 1);

	unsigned char buff[10] = {0xaa, 0xaa, 0xaa, 0x96, 0x69, 0x00, 0x03, 0x20, 0x01, 0x21};
	unsigned char retbuff[0x50];

	rv = libusb_bulk_transfer(g_usb_handle, user_device.bOutEndpointAddress, buff, 10, &length, 1000);
	if (rv < 0)
	{
		printf("*** bulk_transfer failed! \n");
		return -1;
	}

	//int rv = switchReportBulk(0x0400, 0Xc35A, buff, 10, retbuff, 0x40);

	printf("%d\r\n", rv);
	return 0;
}

void testk80()
{
	unsigned char buff[11] = {0x1b, 0x40, 0x1b, 0x3c, 0x1b, 0x33, 0x18, 0x30, 0x31, 0x32, 0x0A};
	WriteDevice(0x0DD4, 0x0237, buff, 11);
}

int main()
{
	printf("000");
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
	if (rv < 0)	printf("*** write failed! %d\n", rv);
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

int switchReportBulk(int vid, int pid, unsigned char *buffer, int buffer_size, unsigned char *returnbuffer, int returnbuffer_size)
{
	int rv;
	rv = WriteDevice2(vid, pid, buffer, buffer_size);
	if (rv < 0)	printf("*** write failed! %d\n", rv);
	return 0;
	/*
	libusb_device_handle* usb_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if(usb_handle == NULL){ printf("*** Permission denied or Can not find the USB Device!\n"); return -1; }
	rv = libusb_bulk_transfer(usb_handle,0x21,0x09,0x0301,0x00,buffer,buffer_size,1000);
	if(rv<0)printf("*** write failed! %d\n",rv); return -1;
	rv =libusb_control_transfer(usb_handle,0xA1,0x01,0x0100,0x00,returnbuffer,returnbuffer_size,1000);
	if(rv < 0) {printf("*** read failed! %d\n",rv); return -1;}
	libusb_close(usb_handle);
	libusb_exit(NULL);
	// */
	return rv;
}
int WriteDevice2(int vid, int pid, char *buff, int len)
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
	//return 0;
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
	libusb_attach_kernel_driver(g_usb_handle, user_device.bInterfaceNumber);
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
