 /******************************************************************************
 * Module: Port
 *
 * File Name: Port.c
 *
 * Description: Source file for TM4C123GH6PM Microcontroller - Port Driver.
 *
 * Author: Mohannad Ragab
 ******************************************************************************/

#include "Port.h"
#include "Port_Regs.h"

#if (PORT_DEV_ERROR_DETECT == STD_ON)

#include "Det.h"
/* AUTOSAR Version checking between Det and Dio Modules */
#if ((DET_AR_MAJOR_VERSION != PORT_AR_RELEASE_MAJOR_VERSION)\
 || (DET_AR_MINOR_VERSION != PORT_AR_RELEASE_MINOR_VERSION)\
 || (DET_AR_PATCH_VERSION != PORT_AR_RELEASE_PATCH_VERSION))
  #error "The AR version of Det.h does not match the expected version"
#endif

#endif

//STATIC const Port_ConfigChannel * Port_PortChannels = NULL_PTR;
STATIC uint8 Port_Status = PORT_NOT_INITIALIZED;


/************************************************************************************
* Service Name: Port_SetupGpioPin
* Sync/Async: Synchronous
* Reentrancy: reentrant
* Parameters (in): ConfigPtr - Pointer to post-build configuration data
* Parameters (inout): None
* Parameters (out): None
* Return value: None
* Description: Function to Setup the pin configuration:
*              - Setup the pin as Digital GPIO pin
*              - Setup the direction of the GPIO pin
*              - Provide initial value for o/p pin
*              - Setup the internal resistor for i/p pin
************************************************************************************/
void Port_Init(const Port_ConfigType* ConfigPtr)
{
#if (PORT_DEV_ERROR_DETECT == STD_ON)
	/* check if the input configuration pointer is not a NULL_PTR */
	if (NULL_PTR == ConfigPtr)
	{
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_INIT_SID,
						PORT_E_PARAM_CONFIG);
	}
	else
#endif
	{
		/*
		 * Set the module state to initialized and point to the PB configuration structure using a global pointer.
		 * This global pointer is global to be used by other functions to read the PB configuration structures
		 */
		Port_Status = PORT_INITIALIZED;
		volatile uint32 *PortGpio_Ptr = NULL_PTR; /* point to the required Port Registers base address */
		volatile uint32 delay = 0;

		for (uint8 counter = 0; counter < PORT_CONFIGURED_CHANNELS; counter++)
		{
			/* Check if the pin points to NULL so it can't be configured */
			if (ConfigPtr->Channels[counter] == NULL_PTR)
			{
				continue;
			}
			switch (ConfigPtr->Channels[counter]->port_num)
			/* Get the base Port address */
			{
			case 0:
				PortGpio_Ptr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
				break;
			case 1:
				PortGpio_Ptr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
				break;
			case 2:
				PortGpio_Ptr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
				break;
			case 3:
				PortGpio_Ptr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
				break;
			case 4:
				PortGpio_Ptr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
				break;
			case 5:
				PortGpio_Ptr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
				break;
			}

			/* Enable clock for PORT and allow time for clock to start*/
			SYSCTL_REGCGC2_REG |= (1 << ConfigPtr->Channels[counter]->port_num);
			delay = SYSCTL_REGCGC2_REG;

			/* Skip PC0 to PC3 JTAG Pins */
			if (ConfigPtr->Channels[counter]->port_num == 2 && ConfigPtr->Channels[counter]->pin_num <= 3)
			{
				continue;
			}
			/* Unlock PF0 & PD7 LOCK_REG to write 1 in the Commit rigester */
			else if (((ConfigPtr->Channels[counter]->port_num == 3) && (ConfigPtr->Channels[counter]->pin_num == 7)) || ((ConfigPtr->Channels[counter]->port_num == 5) && (ConfigPtr->Channels[counter]->pin_num == 0)))
			{
				/* Unlock the GPIOCR register */
				*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_LOCK_REG_OFFSET) = 0x4C4F434B;
				/* Set the corresponding bit in GPIOCR register to allow changes on this pin */
				SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_COMMIT_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
			}
			else
			{
				/* Do Nothing ... No need to unlock the commit register for this pin */
			}

			/* Set mode to DIO */
			if (ConfigPtr->Channels[counter]->mode == PORT_DIO_MODE)
			{
				/* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
				SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
				CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Disable Alternative function for this pin by clear the corresponding bit in GPIOAFSEL register */
				CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Clear the PMCx bits for this pin */
				*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (ConfigPtr->Channels[counter]->pin_num * 4));
			}
			/* Set mode to ANALOG */
			else if (ConfigPtr->Channels[counter]->mode == PORT_ANALOG_MODE)
			{
				/* Clear the corresponding bit in the GPIODEN register to disable digital functionality on this pin */
				CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Set the corresponding bit in the GPIOAMSEL register to enable analog functionality on this pin */
				SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
			}
			/* Set mode to Alternative Function */
			else if (ConfigPtr->Channels[counter]->mode > PORT_DIO_MODE && ConfigPtr->Channels[counter]->mode < PORT_ANALOG_MODE)
			{
				/* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
				SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
				CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Enable Alternative function for this pin by set the corresponding bit in GPIOAFSEL register */
				SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Clear the PMCx bits for this pin */
				*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (ConfigPtr->Channels[counter]->pin_num * 4));

				/* Put mode in the PMCx bits for this pin for the SPI functionalty */
				*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) |= (ConfigPtr->Channels[counter]->mode << (ConfigPtr->Channels[counter]->pin_num * 4));
			}
			else
			{
				/* Do Nothing */
			}

			/* Set direction and PULL UP/PULL DOWN resistors */
			if (ConfigPtr->Channels[counter]->direction == PORT_PIN_OUT)
			{
				/* Set the corresponding bit in the GPIODIR register to configure it as output pin */
				SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				/* Set initial value */
				if (ConfigPtr->Channels[counter]->initial_value == STD_HIGH)
				{
					/* Set the corresponding bit in the GPIODATA register to provide initial value 1 */
					SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DATA_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
				}
				else
				{
					/* Clear the corresponding bit in the GPIODATA register to provide initial value 0 */
					CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DATA_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
				}
			}
			else if (ConfigPtr->Channels[counter]->direction == PORT_PIN_IN)
			{
				/* Clear the corresponding bit in the GPIODIR register to configure it as input pin */
				CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);

				if (ConfigPtr->Channels[counter]->resistor == PULL_UP)
				{
					/* Set the corresponding bit in the GPIOPUR register to enable the internal pull up pin */
					SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_UP_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
				}
				else if (ConfigPtr->Channels[counter]->resistor == PULL_DOWN)
				{
					/* Set the corresponding bit in the GPIOPDR register to enable the internal pull down pin */
					SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_DOWN_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
				}
				else
				{
					/* Clear the corresponding bit in the GPIOPUR register to disable the internal pull up pin */
					CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_UP_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
					/* Clear the corresponding bit in the GPIOPDR register to disable the internal pull down pin */
					CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_PULL_DOWN_REG_OFFSET), ConfigPtr->Channels[counter]->pin_num);
				}
			}
			else
			{
				/* Do Nothing */
			}
		}
	}
}

/************************************************************************************
* Service Name: Port_SetPinDirection
* Service ID[hex]: 0x01
* Sync/Async: Synchronous
* Reentrancy: reentrant
* Parameters (in): Pin - Port Pin ID number,
*                  Direction - Port Pin Direction
* Parameters (inout): None
* Parameters (out): None
* Return value: None
* Description: Sets the port pin direction
************************************************************************************/
#if (PORT_SET_PIN_DIRECTION_API == STD_ON)
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
	Port_ConfigChannel Pin_Config = *Port_Configuration.Channels[Pin];
	volatile uint32 *PortGpio_Ptr = NULL_PTR;
	boolean Error = FALSE;
#if (PORT_DEV_ERROR_DETECT == STD_ON)
	if (Port_Status != PORT_INITIALIZED)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_DIRECTION_SID,
						PORT_E_UNINIT);
	}
	else
	{
		/* Do Nothing */
	}
	/* Check if this pin is available to be configured, or even EXIST! */
	if (Pin >= PORT_CONFIGURED_CHANNELS || Port_Configuration.Channels[Pin] == NULL_PTR)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_DIRECTION_SID,
						PORT_E_PARAM_PIN);
	}
	else
	{
		/* Do Nothing */
	}
	if (!(Pin_Config.Pin_direction_changeable))
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_DIRECTION_SID,
						PORT_E_DIRECTION_UNCHANGEABLE);
	}
	else
	{
		/* Do Nothing */
	}
#endif /* (PORT_DEV_ERROR_DETECT == STD_ON) */
	if (Error == FALSE)
	{
		switch (Pin_Config.port_num)
		{
		case 0:
			PortGpio_Ptr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
			break;
		case 1:
			PortGpio_Ptr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
			break;
		case 2:
			PortGpio_Ptr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
			break;
		case 3:
			PortGpio_Ptr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
			break;
		case 4:
			PortGpio_Ptr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
			break;
		case 5:
			PortGpio_Ptr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
			break;
		}
		if (Direction == PORT_PIN_IN)
		{
			/* Clear the corresponding bit in the GPIODIR register to configure it as input pin */
			CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET), Pin_Config.pin_num);
		}
		else if (Direction == PORT_PIN_OUT)
		{
			/* Set the corresponding bit in the GPIODIR register to configure it as output pin */
			SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET), Pin_Config.pin_num);
		}
		else
		{
			/* Do Nothing */
		}
	}
}
#endif /* (PORT_SET_PIN_DIRECTION_API == STD_ON) */

/************************************************************************************
* Service Name: Port_RefreshPortDirection
* Service ID[hex]: 0x02
* Sync/Async: Synchronous
* Reentrancy: Non reentrant
* Parameters (in): None
* Parameters (inout): None
* Parameters (out): None
* Return value: None
* Description: Refreshes port direction.
************************************************************************************/
void Port_RefreshPortDirection(void)
{
	volatile uint32 *PortGpio_Ptr = NULL_PTR;
#if (PORT_DEV_ERROR_DETECT == STD_ON)
	if (Port_Status != PORT_INITIALIZED)
	{
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_REFRESH_PORT_DIRECTION_SID,
						PORT_E_UNINIT);
	}
	else
#endif /* (PORT_DEV_ERROR_DETECT == STD_ON) */
	{
		for (Port_PinType Channel = 0; Channel < PORT_CONFIGURED_CHANNELS; Channel++)
		{
			if (Port_Configuration.Channels[Channel] == NULL_PTR)
			{
				/* Do Nothing */
			}
			else
			{
				/* get the configuration struct for this specific Port-Pin */
				Port_ConfigChannel ChannelConfig = *Port_Configuration.Channels[Channel];
				/* Get Port base address */
				switch (ChannelConfig.port_num)
				{
				case 0:
					PortGpio_Ptr = (volatile uint32 *)GPIO_PORTA_BASE_ADDRESS; /* PORTA Base Address */
					break;
				case 1:
					PortGpio_Ptr = (volatile uint32 *)GPIO_PORTB_BASE_ADDRESS; /* PORTB Base Address */
					break;
				case 2:
					PortGpio_Ptr = (volatile uint32 *)GPIO_PORTC_BASE_ADDRESS; /* PORTC Base Address */
					break;
				case 3:
					PortGpio_Ptr = (volatile uint32 *)GPIO_PORTD_BASE_ADDRESS; /* PORTD Base Address */
					break;
				case 4:
					PortGpio_Ptr = (volatile uint32 *)GPIO_PORTE_BASE_ADDRESS; /* PORTE Base Address */
					break;
				case 5:
					PortGpio_Ptr = (volatile uint32 *)GPIO_PORTF_BASE_ADDRESS; /* PORTF Base Address */
					break;
				}
				if (ChannelConfig.Pin_direction_changeable == STD_ON)
				{
					if (ChannelConfig.direction == PORT_PIN_IN)
					{
						/* Clear the corresponding bit in the GPIODIR register to configure it as input pin */
						CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET), ChannelConfig.pin_num);
					}
					else if (ChannelConfig.direction == PORT_PIN_OUT)
					{
						/* Set the corresponding bit in the GPIODIR register to configure it as output pin */
						SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIR_REG_OFFSET), ChannelConfig.pin_num);
					}
					else
					{
						/* Do Nothing */
					}
				}
				else
				{
					/* Do Nothing */
				}
			}
		}
	}
}

/************************************************************************************
* Service Name: Port_SetPinMode
* Service ID[hex]: 0x04
* Sync/Async: Synchronous
* Reentrancy: reentrant
* Parameters (in): Pin - Port Pin ID number,
                   Mode - New Port Pin mode to be set on port pin
* Parameters (inout): None
* Parameters (out): None
* Return value: None
* Description: Sets the port pin mode.
************************************************************************************/
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode)
{
	Port_ConfigChannel Pin_Config = *Port_Configuration.Channels[Pin];
	volatile uint32 *PortGpio_Ptr = NULL_PTR;
	boolean Error = FALSE;
#if (PORT_DEV_ERROR_DETECT == STD_ON)
	/* Check if Port is initialized or not */
	if (Port_Status != PORT_INITIALIZED)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_MODE_SID,
						PORT_E_UNINIT);
	}
	else
	{
		/* Do Nothing */
	}
	/* Check if this pin is available to be configured, or even EXIST! */
	if (Pin >= PORT_CONFIGURED_CHANNELS || Port_Configuration.Channels[Pin] == NULL_PTR)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_MODE_SID,
						PORT_E_PARAM_PIN);
	}
	else
	{
		/* Do Nothing */
	}
	/* Check if Pin mode is changeable or not */
	if (Pin_Config.Pin_mode_changeable == STD_OFF)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_MODE_SID,
						PORT_E_MODE_UNCHANGEABLE);
	}
	else
	{
		/* Do Nothing */
	}
	/* Check if mode is valid or not */
	if (Mode > PORT_ANALOG_MODE)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_SET_PIN_MODE_SID,
						PORT_E_PARAM_INVALID_MODE);
	}
	else
	{
		/* Do Nothing */
	}
#endif /* (PORT_DEV_ERROR_DETECT == STD_ON) */
	if (Error == FALSE)
	{
		if (Mode == PORT_DIO_MODE)
		{
			/* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
			SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET), Pin_Config.pin_num);

			/* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
			CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET), Pin_Config.pin_num);

			/* Disable Alternative function for this pin by clear the corresponding bit in GPIOAFSEL register */
			CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET), Pin_Config.pin_num);

			/* Clear the PMCx bits for this pin */
			*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (Pin_Config.pin_num * 4));
		}
		else if (Mode == PORT_ANALOG_MODE)
		{
			/* Clear the corresponding bit in the GPIODEN register to disable digital functionality on this pin */
			CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET), Pin_Config.pin_num);

			/* Set the corresponding bit in the GPIOAMSEL register to enable analog functionality on this pin */
			SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET), Pin_Config.pin_num);
		}
		else
		{
			/* Set the corresponding bit in the GPIODEN register to enable digital functionality on this pin */
			SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_DIGITAL_ENABLE_REG_OFFSET), Pin_Config.pin_num);

			/* Clear the corresponding bit in the GPIOAMSEL register to disable analog functionality on this pin */
			CLEAR_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ANALOG_MODE_SEL_REG_OFFSET), Pin_Config.pin_num);

			/* Enable Alternative function for this pin by set the corresponding bit in GPIOAFSEL register */
			SET_BIT(*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_ALT_FUNC_REG_OFFSET), Pin_Config.pin_num);

			/* Clear the PMCx bits for this pin */
			*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) &= ~(0x0000000F << (Pin_Config.pin_num * 4));

			/* Put mode in the PMCx bits for this pin for the SPI functionalty */
			*(volatile uint32 *)((volatile uint8 *)PortGpio_Ptr + PORT_CTL_REG_OFFSET) |= (Pin_Config.mode << (Pin_Config.pin_num * 4));
		}
	}
}

/************************************************************************************
* Service Name: Port_GetVersionInfo
* Service ID[hex]: 0x03
* Sync/Async: Synchronous
* Reentrancy: Non reentrant
* Parameters (in): None
* Parameters (inout): None
* Parameters (out): versioninfo - Pointer to where to store the version information of this module
* Return value: None
* Description: Returns the version information of this module.
************************************************************************************/
#if (PORT_VERSION_INFO_API == STD_ON)
void Port_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
	boolean Error = FALSE;
#if (PORT_DEV_ERROR_DETECT == STD_ON)
	/* Check if the sent pointer is not Null pointer */
	if (NULL_PTR == versioninfo)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_GET_VERSION_INFO_SID,
						PORT_E_PARAM_POINTER);
	}
	else
	{
		/* Do Nothing */
	}
	/* check if the Port driver initialized or Not */
	if (Port_Status != PORT_INITIALIZED)
	{
		Error = TRUE;
		Det_ReportError(PORT_MODULE_ID, PORT_INSTANCE_ID, PORT_GET_VERSION_INFO_SID,
						PORT_E_UNINIT);
	}
	else
	{
		/* Do Nothing */
	}
#endif /* (PORT_DEV_ERROR_DETECT == STD_ON) */
	if (Error == FALSE)
	{
		/* Copy the vendor Id */
        versioninfo->vendorID = (uint16)PORT_VENDOR_ID;
        /* Copy the module Id */
        versioninfo->moduleID = (uint16)PORT_MODULE_ID;
        /* Copy Software Major Version */
        versioninfo->sw_major_version = (uint8)PORT_SW_MAJOR_VERSION;
        /* Copy Software Minor Version */
        versioninfo->sw_minor_version = (uint8)PORT_SW_MINOR_VERSION;
        /* Copy Software Patch Version */
        versioninfo->sw_patch_version = (uint8)PORT_SW_PATCH_VERSION;
	}
}
#endif /* (PORT_VERSION_INFO_API == STD_ON) */
