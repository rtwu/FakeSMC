/*
 *  F718x.cpp
 *  HWSensors
 *
 *  Based on code from Open Hardware Monitor project by Michael Möller (C) 2011
 *
 *  Created by mozo on 16/10/10.
 *  Copyright 2010 mozodojo. All rights reserved.
 *
 */

/*
 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1
 
 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS" basis,
 WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 for the specific language governing rights and limitations under the License.
 
 The Original Code is the Open Hardware Monitor code.
 
 The Initial Developer of the Original Code is 
 Michael Möller <m.moeller@gmx.ch>.
 Portions created by the Initial Developer are Copyright (C) 2011
 the Initial Developer. All Rights Reserved.
 
 Contributor(s):
 
 Alternatively, the contents of this file may be used under the terms of
 either the GNU General Public License Version 2 or later (the "GPL"), or
 the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.
 
 */

#include "FintekF718x.h"

#include <architecture/i386/pio.h>
#include "FakeSMC.h"

#define Debug FALSE

#define LogPrefix "F718x: "
#define DebugLog(string, args...)	do { if (Debug) { IOLog (LogPrefix "[Debug] " string "\n", ## args); } } while(0)
#define WarningLog(string, args...) do { IOLog (LogPrefix "[Warning] " string "\n", ## args); } while(0)
#define InfoLog(string, args...)	do { IOLog (LogPrefix string "\n", ## args); } while(0)

#define super SuperIOMonitor
OSDefineMetaClassAndStructors(F718x, SuperIOMonitor)

UInt8 F718x::readByte(UInt8 reg) 
{
	outb(address + FINTEK_ADDRESS_REGISTER_OFFSET, reg);
	return inb(address + FINTEK_DATA_REGISTER_OFFSET);
} 

long F718x::readTemperature(unsigned long index)
{
	float value;
	
	switch (model) 
	{
		case F71858: 
		{
			int tableMode = 0x3 & readByte(FINTEK_TEMPERATURE_CONFIG_REG);
			int high = readByte(FINTEK_TEMPERATURE_BASE_REG + 2 * index);
			int low = readByte(FINTEK_TEMPERATURE_BASE_REG + 2 * index + 1);      
			
			if (high != 0xbb && high != 0xcc) 
			{
                int bits = 0;
				
                switch (tableMode) 
				{
					case 0: bits = 0; break;
					case 1: bits = 0; break;
					case 2: bits = (high & 0x80) << 8; break;
					case 3: bits = (low & 0x01) << 15; break;
                }
                bits |= high << 7;
                bits |= (low & 0xe0) >> 1;
				
                short val = (short)(bits & 0xfff0);
				
				return (float)val / 128.0f;
			} 
			else 
			{
                return 0;
			}
		} break;
		default: 
		{
            value = readByte(FINTEK_TEMPERATURE_BASE_REG + 2 * (index + 1));
		} break;
	}
	
	return value;
}

long F718x::readVoltage(unsigned long index)
{
	//UInt16 raw = readByte(FINTEK_VOLTAGE_BASE_REG + index);
	
	//if (index == 0) m_RawVCore = raw;
	
	float V = (index == 1 ? 0.5f : 1.0f) * (readByte(FINTEK_VOLTAGE_BASE_REG + index) << 4); // * 0.001f Exclude by trauma
	
	return V;
}

long F718x::readTachometer(unsigned long index)
{
	int value = readByte(FINTEK_FAN_TACHOMETER_REG[index]) << 8;
	value |= readByte(FINTEK_FAN_TACHOMETER_REG[index] + 1);
	
	if (value > 0)
		value = (value < 0x0fff) ? 1.5e6f / value : 0;
	
	return value;
}


void F718x::enter()
{
	outb(registerPort, 0x87);
	outb(registerPort, 0x87);
}

void F718x::exit()
{
	outb(registerPort, 0xAA);
	//outb(registerPort, SUPERIO_CONFIGURATION_CONTROL_REGISTER);
	//outb(valuePort, 0x02);
}

bool F718x::probePort()
{
	UInt8 logicalDeviceNumber = 0;
	
	UInt8 id = listenPortByte(SUPERIO_CHIP_ID_REGISTER);
	UInt8 revision = listenPortByte(SUPERIO_CHIP_REVISION_REGISTER);
	
	if (id == 0 || id == 0xff || revision == 0 || revision == 0xff)
		return false;
	
	switch (id) 
	{
		case 0x05:
		{
			switch (revision) 
			{
				case 0x07:
					model = F71858;
					logicalDeviceNumber = F71858_HARDWARE_MONITOR_LDN;
					break;
				case 0x41:
					model = F71882;
					logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
					break;              
			}
		} break;
		case 0x06:
		{
			switch (revision) 
			{
				case 0x01:
					model = F71862;
					logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
					break;              
			} 
		} break;
		case 0x07:
		{
			switch (revision)
			{
				case 0x23:
					model = F71889F;
					logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
					break;              
			} 
		} break;
		case 0x08:
		{
			switch (revision)
			{
				case 0x14:
					model = F71869;
					logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
					break;              
			}
		} break;
		case 0x09:
		{
			switch (revision)
			{
                case 0x01:                                                      /*Add F71808 */
                    model = F71808;                                         /*Add F71808 */
                    logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;         /*Add F71808 */
                    break;                                                    /*Add F71808 */
				case 0x09:
					model = F71889ED;
					logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
					break;              
			}
		} break;
	}
	
	if (!model)
	{
		DebugLog("found unsupported chip ID=0x%x REVISION=0x%x", id, revision);
		return false;
	} 
	
	selectLogicalDevice(logicalDeviceNumber);
    
    IOSleep(50);
	
	address = listenPortWord(SUPERIO_BASE_ADDRESS_REGISTER);
	
	IOSleep(250);
	
	if (address != listenPortWord(SUPERIO_BASE_ADDRESS_REGISTER)) {
        DebugLog("can't get monitoring logical device address");
		return false;
    }
    
    // some Fintek chips have address register offset 0x05 added already
    if ((address & 0x07) == 0x05)
        address &= 0xFFF8;
    
    if (address < 0x100 || (address & 0xF007) != 0)
		return false;
    
    IOSleep(50);
    
    UInt16 vendor = listenPortWord(FINTEK_VENDOR_ID_REGISTER);
    
    if (vendor != FINTEK_VENDOR_ID)
    {
        DebugLog("wrong vendor chip ID=0x%x REVISION=0x%x VENDORID=0x%x", id, revision, vendor);
        return false;
    }
	
	return true;
}

bool F718x::startPlugin()
{
    InfoLog("found Fintek %s", getModelName());
	
    OSDictionary* list = OSDynamicCast(OSDictionary, getProperty("Sensors Configuration"));
    IOService * fRoot = getServiceRoot();
    OSString *vendor=NULL, *product=NULL;
    OSDictionary *configuration=NULL; 
    
    
    if(fRoot)
    {
        vendor = vendorID( OSDynamicCast(OSString, fRoot->getProperty("oem-mb-manufacturer") ? fRoot->getProperty("oem-mb-manufacturer") :  (fRoot->getProperty("oem-manufacturer") ? fRoot->getProperty("oem-manufacturer") : NULL)));
        product = OSDynamicCast(OSString, fRoot->getProperty("oem-mb-product") ? fRoot->getProperty("oem-mb-product") :  (fRoot->getProperty("oem-product-name") ? fRoot->getProperty("oem-product-name") : NULL));
        
    }
    if (vendor)
        if (OSDictionary *link = OSDynamicCast(OSDictionary, list->getObject(vendor)))
            if(product)
                configuration = OSDynamicCast(OSDictionary, link->getObject(product));
    
    
    if (list && !configuration) 
        configuration = OSDynamicCast(OSDictionary, list->getObject("Default"));
	
	// Heatsink
	if (!addSensor(KEY_CPU_HEATSINK_TEMPERATURE, TYPE_SP78, 2, kSuperIOTemperatureSensor, 0))
		WarningLog("error adding heatsink temperature sensor");
	// Northbridge
	if (!addSensor(KEY_NORTHBRIDGE_TEMPERATURE, TYPE_SP78, 2, kSuperIOTemperatureSensor, 1))
		WarningLog("error adding system temperature sensor");
	
	// Voltage
	switch (model) 
	{
        case F71858:
			break;
        default:
			// CPU Vcore
			if (!addSensor(KEY_CPU_VRM_SUPPLY0, TYPE_FP2E, 2, kSuperIOVoltageSensor, 1))
				WarningLog("error adding CPU voltage sensor");
			break;
	}
	
	// Tachometers
	for (int i = 0; i < (model == F71882 ? 4 : 3); i++) {
		OSString* name = 0;
		
		if (configuration) {
			char key[7];
			
			snprintf(key, 7, "FANIN%X", i);
			
			name = OSDynamicCast(OSString, configuration->getObject(key));
		}
		
		UInt32 nameLength = name ? name->getLength() : 0;
		
		if (readTachometer(i) > 10 || nameLength > 0)
			if (!addTachometer(i, (nameLength > 0 ? name->getCStringNoCopy() : 0)))
				WarningLog("error adding tachometer sensor %d", i);
	}
	
	return true;
}

const char *F718x::getModelName()
{
	switch (model) 
	{
        case F71858: return "F71858";
        case F71862: return "F71862";
        case F71869: return "F71869";
        case F71882: return "F71882";
        case F71889ED: return "F71889ED";
        case F71889F: return "F71889F";
		case F71808:  return "F71808";	
	}
	
	return "unknown";
}